#!/usr/bin/env python3
#
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
from datetime import date
import re
import optparse
import os
from string import Template
import sys
import textwrap
import zipfile

from util import build_utils
from util import java_cpp_utils
import action_helpers  # build_utils adds //build to sys.path.
import zip_helpers


# List of C++ types that are compatible with the Java code generated by this
# script.
#
# This script can parse .idl files however, at present it ignores special
# rules such as [cpp_enum_prefix_override="ax_attr"].
ENUM_FIXED_TYPE_ALLOWLIST = [
    'char', 'unsigned char', 'short', 'unsigned short', 'int', 'int8_t',
    'int16_t', 'int32_t', 'uint8_t', 'uint16_t'
]


class EnumDefinition:
  def __init__(self, original_enum_name=None, class_name_override=None,
               enum_package=None, entries=None, comments=None, fixed_type=None):
    self.original_enum_name = original_enum_name
    self.class_name_override = class_name_override
    self.enum_package = enum_package
    self.entries = collections.OrderedDict(entries or [])
    self.comments = collections.OrderedDict(comments or [])
    self.prefix_to_strip = None
    self.fixed_type = fixed_type

  def AppendEntry(self, key, value):
    if key in self.entries:
      raise Exception('Multiple definitions of key %s found.' % key)
    self.entries[key] = value

  def AppendEntryComment(self, key, value):
    if key in self.comments:
      raise Exception('Multiple definitions of key %s found.' % key)
    self.comments[key] = value

  @property
  def class_name(self):
    return self.class_name_override or self.original_enum_name

  def Finalize(self):
    self._Validate()
    self._AssignEntryIndices()
    self._StripPrefix()
    self._NormalizeNames()

  def _Validate(self):
    assert self.class_name
    assert self.enum_package
    assert self.entries
    if self.fixed_type and self.fixed_type not in ENUM_FIXED_TYPE_ALLOWLIST:
      raise Exception('Fixed type %s for enum %s not in allowlist.' %
                      (self.fixed_type, self.class_name))

  def _AssignEntryIndices(self):
    # Enums, if given no value, are given the value of the previous enum + 1.
    if not all(self.entries.values()):
      prev_enum_value = -1
      for key, value in self.entries.items():
        if not value:
          self.entries[key] = prev_enum_value + 1
        elif value in self.entries:
          self.entries[key] = self.entries[value]
        else:
          try:
            self.entries[key] = int(value)
          except ValueError as e:
            raise Exception('Could not interpret integer from enum value "%s" '
                            'for key %s.' % (value, key)) from e
        prev_enum_value = self.entries[key]


  def _StripPrefix(self):
    prefix_to_strip = self.prefix_to_strip
    if not prefix_to_strip:
      shout_case = self.original_enum_name
      shout_case = re.sub('(?!^)([A-Z]+)', r'_\1', shout_case).upper()
      shout_case += '_'

      prefixes = [shout_case, self.original_enum_name,
                  'k' + self.original_enum_name]

      for prefix in prefixes:
        if all(w.startswith(prefix) for w in self.entries.keys()):
          prefix_to_strip = prefix
          break
      else:
        prefix_to_strip = ''

    def StripEntries(entries):
      ret = collections.OrderedDict()
      for k, v in entries.items():
        stripped_key = k.replace(prefix_to_strip, '', 1)
        if isinstance(v, str):
          stripped_value = v.replace(prefix_to_strip, '')
        else:
          stripped_value = v
        ret[stripped_key] = stripped_value

      return ret

    self.entries = StripEntries(self.entries)
    self.comments = StripEntries(self.comments)

  def _NormalizeNames(self):
    self.entries = _TransformKeys(self.entries, java_cpp_utils.KCamelToShouty)
    self.comments = _TransformKeys(self.comments, java_cpp_utils.KCamelToShouty)


def _TransformKeys(d, func):
  """Normalize keys in |d| and update references to old keys in |d| values."""
  keys_map = {k: func(k) for k in d}
  ret = collections.OrderedDict()
  for k, v in d.items():
    # Need to transform values as well when the entry value was explicitly set
    # (since it could contain references to other enum entry values).
    if isinstance(v, str):
      # First check if a full replacement is available. This avoids issues when
      # one key is a substring of another.
      if v in d:
        v = keys_map[v]
      else:
        for old_key, new_key in keys_map.items():
          v = v.replace(old_key, new_key)
    ret[keys_map[k]] = v
  return ret


class DirectiveSet:
  class_name_override_key = 'CLASS_NAME_OVERRIDE'
  enum_package_key = 'ENUM_PACKAGE'
  prefix_to_strip_key = 'PREFIX_TO_STRIP'

  known_keys = [class_name_override_key, enum_package_key, prefix_to_strip_key]

  def __init__(self):
    self._directives = {}

  def Update(self, key, value):
    if key not in DirectiveSet.known_keys:
      raise Exception("Unknown directive: " + key)
    self._directives[key] = value

  @property
  def empty(self):
    return len(self._directives) == 0

  def UpdateDefinition(self, definition):
    definition.class_name_override = self._directives.get(
        DirectiveSet.class_name_override_key, '')
    definition.enum_package = self._directives.get(
        DirectiveSet.enum_package_key)
    definition.prefix_to_strip = self._directives.get(
        DirectiveSet.prefix_to_strip_key)


class HeaderParser:
  single_line_comment_re = re.compile(r'\s*//\s*([^\n]*)')
  multi_line_comment_start_re = re.compile(r'\s*/\*')
  enum_line_re = re.compile(r'^\s*(\w+)(\s*\=\s*([^,\n]+))?,?')
  enum_end_re = re.compile(r'^\s*}\s*;\.*$')
  generator_error_re = re.compile(r'^\s*//\s+GENERATED_JAVA_(\w+)\s*:\s*$')
  generator_directive_re = re.compile(
      r'^\s*//\s+GENERATED_JAVA_(\w+)\s*:\s*([\.\w]+)$')
  multi_line_generator_directive_start_re = re.compile(
      r'^\s*//\s+GENERATED_JAVA_(\w+)\s*:\s*\(([\.\w]*)$')
  multi_line_directive_continuation_re = re.compile(r'^\s*//\s+([\.\w]+)$')
  multi_line_directive_end_re = re.compile(r'^\s*//\s+([\.\w]*)\)$')

  optional_class_or_struct_re = r'(class|struct)?'
  enum_name_re = r'(\w+)'
  optional_fixed_type_re = r'(\:\s*(\w+\s*\w+?))?'
  enum_start_re = re.compile(r'^\s*(?:\[cpp.*\])?\s*enum\s+' +
      optional_class_or_struct_re + '\s*' + enum_name_re + '\s*' +
      optional_fixed_type_re + '\s*{\s*')
  enum_single_line_re = re.compile(
      r'^\s*(?:\[cpp.*\])?\s*enum.*{(?P<enum_entries>.*)}.*$')

  def __init__(self, lines, path=''):
    self._lines = lines
    self._path = path
    self._enum_definitions = []
    self._in_enum = False
    self._current_definition = None
    self._current_comments = []
    self._generator_directives = DirectiveSet()
    self._multi_line_generator_directive = None
    self._current_enum_entry = ''

  def _ApplyGeneratorDirectives(self):
    self._generator_directives.UpdateDefinition(self._current_definition)
    self._generator_directives = DirectiveSet()

  def ParseDefinitions(self):
    for line in self._lines:
      self._ParseLine(line)
    return self._enum_definitions

  def _ParseLine(self, line):
    if self._multi_line_generator_directive:
      self._ParseMultiLineDirectiveLine(line)
    elif not self._in_enum:
      self._ParseRegularLine(line)
    else:
      self._ParseEnumLine(line)

  def _ParseEnumLine(self, line):
    if HeaderParser.multi_line_comment_start_re.match(line):
      raise Exception('Multi-line comments in enums are not supported in ' +
                      self._path)

    enum_comment = HeaderParser.single_line_comment_re.match(line)
    if enum_comment:
      comment = enum_comment.groups()[0]
      if comment:
        self._current_comments.append(comment)
    elif HeaderParser.enum_end_re.match(line):
      self._FinalizeCurrentEnumDefinition()
    else:
      self._AddToCurrentEnumEntry(line)
      if ',' in line:
        self._ParseCurrentEnumEntry()

  def _ParseSingleLineEnum(self, line):
    for entry in line.split(','):
      self._AddToCurrentEnumEntry(entry)
      self._ParseCurrentEnumEntry()

    self._FinalizeCurrentEnumDefinition()

  def _ParseCurrentEnumEntry(self):
    if not self._current_enum_entry:
      return

    enum_entry = HeaderParser.enum_line_re.match(self._current_enum_entry)
    if not enum_entry:
      raise Exception('Unexpected error while attempting to parse %s as enum '
                      'entry.' % self._current_enum_entry)

    enum_key = enum_entry.groups()[0]
    enum_value = enum_entry.groups()[2]
    self._current_definition.AppendEntry(enum_key, enum_value)
    if self._current_comments:
      self._current_definition.AppendEntryComment(
          enum_key, ' '.join(self._current_comments))
      self._current_comments = []
    self._current_enum_entry = ''

  def _AddToCurrentEnumEntry(self, line):
    self._current_enum_entry += ' ' + line.strip()

  def _FinalizeCurrentEnumDefinition(self):
    if self._current_enum_entry:
      self._ParseCurrentEnumEntry()
    self._ApplyGeneratorDirectives()
    self._current_definition.Finalize()
    self._enum_definitions.append(self._current_definition)
    self._current_definition = None
    self._in_enum = False

  def _ParseMultiLineDirectiveLine(self, line):
    multi_line_directive_continuation = (
        HeaderParser.multi_line_directive_continuation_re.match(line))
    multi_line_directive_end = (
        HeaderParser.multi_line_directive_end_re.match(line))

    if multi_line_directive_continuation:
      value_cont = multi_line_directive_continuation.groups()[0]
      self._multi_line_generator_directive[1].append(value_cont)
    elif multi_line_directive_end:
      directive_name = self._multi_line_generator_directive[0]
      directive_value = "".join(self._multi_line_generator_directive[1])
      directive_value += multi_line_directive_end.groups()[0]
      self._multi_line_generator_directive = None
      self._generator_directives.Update(directive_name, directive_value)
    else:
      raise Exception('Malformed multi-line directive declaration in ' +
                      self._path)

  def _ParseRegularLine(self, line):
    enum_start = HeaderParser.enum_start_re.match(line)
    generator_directive_error = HeaderParser.generator_error_re.match(line)
    generator_directive = HeaderParser.generator_directive_re.match(line)
    multi_line_generator_directive_start = (
        HeaderParser.multi_line_generator_directive_start_re.match(line))
    single_line_enum = HeaderParser.enum_single_line_re.match(line)

    if generator_directive_error:
      raise Exception('Malformed directive declaration in ' + self._path +
                      '. Use () for multi-line directives. E.g.\n' +
                      '// GENERATED_JAVA_ENUM_PACKAGE: (\n' +
                      '//   foo.package)')
    if generator_directive:
      directive_name = generator_directive.groups()[0]
      directive_value = generator_directive.groups()[1]
      self._generator_directives.Update(directive_name, directive_value)
    elif multi_line_generator_directive_start:
      directive_name = multi_line_generator_directive_start.groups()[0]
      directive_value = multi_line_generator_directive_start.groups()[1]
      self._multi_line_generator_directive = (directive_name, [directive_value])
    elif enum_start or single_line_enum:
      if self._generator_directives.empty:
        return
      self._current_definition = EnumDefinition(
          original_enum_name=enum_start.groups()[1],
          fixed_type=enum_start.groups()[3])
      self._in_enum = True
      if single_line_enum:
        self._ParseSingleLineEnum(single_line_enum.group('enum_entries'))


def DoGenerate(source_paths):
  for source_path in source_paths:
    enum_definitions = DoParseHeaderFile(source_path)
    if not enum_definitions:
      raise Exception('No enums found in %s\n'
                      'Did you forget prefixing enums with '
                      '"// GENERATED_JAVA_ENUM_PACKAGE: foo"?' %
                      source_path)
    for enum_definition in enum_definitions:
      output_path = java_cpp_utils.GetJavaFilePath(enum_definition.enum_package,
                                                   enum_definition.class_name)
      output = GenerateOutput(source_path, enum_definition)
      yield output_path, output


def DoParseHeaderFile(path):
  with open(path) as f:
    return HeaderParser(f.readlines(), path).ParseDefinitions()


def GenerateOutput(source_path, enum_definition):
  template = Template("""
// Copyright ${YEAR} The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     ${SCRIPT_NAME}
// From
//     ${SOURCE_PATH}

package ${PACKAGE};

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({
${INT_DEF}
})
@Retention(RetentionPolicy.SOURCE)
public @interface ${CLASS_NAME} {
${ENUM_ENTRIES}
}
""")

  enum_template = Template('  int ${NAME} = ${VALUE};')
  enum_entries_string = []
  enum_names = []
  for enum_name, enum_value in enum_definition.entries.items():
    values = {
        'NAME': enum_name,
        'VALUE': enum_value,
    }
    enum_comments = enum_definition.comments.get(enum_name)
    if enum_comments:
      enum_comments_indent = '   * '
      comments_line_wrapper = textwrap.TextWrapper(
          initial_indent=enum_comments_indent,
          subsequent_indent=enum_comments_indent,
          width=100)
      enum_entries_string.append('  /**')
      enum_entries_string.append('\n'.join(
          comments_line_wrapper.wrap(enum_comments)))
      enum_entries_string.append('   */')
    enum_entries_string.append(enum_template.substitute(values))
    if enum_name != "NUM_ENTRIES":
      enum_names.append(enum_definition.class_name + '.' + enum_name)
  enum_entries_string = '\n'.join(enum_entries_string)

  enum_names_indent = ' ' * 4
  wrapper = textwrap.TextWrapper(initial_indent = enum_names_indent,
                                 subsequent_indent = enum_names_indent,
                                 width = 100)
  enum_names_string = '\n'.join(wrapper.wrap(', '.join(enum_names)))

  values = {
      'CLASS_NAME': enum_definition.class_name,
      'ENUM_ENTRIES': enum_entries_string,
      'PACKAGE': enum_definition.enum_package,
      'INT_DEF': enum_names_string,
      'SCRIPT_NAME': java_cpp_utils.GetScriptName(),
      'SOURCE_PATH': source_path,
      'YEAR': str(date.today().year)
  }
  return template.substitute(values)


def DoMain(argv):
  usage = 'usage: %prog [options] [output_dir] input_file(s)...'
  parser = optparse.OptionParser(usage=usage)

  parser.add_option('--srcjar',
                    help='When specified, a .srcjar at the given path is '
                    'created instead of individual .java files.')

  options, args = parser.parse_args(argv)

  if not args:
    parser.error('Need to specify at least one input file')
  input_paths = args

  with action_helpers.atomic_output(options.srcjar) as f:
    with zipfile.ZipFile(f, 'w', zipfile.ZIP_STORED) as srcjar:
      for output_path, data in DoGenerate(input_paths):
        zip_helpers.add_to_zip_hermetic(srcjar, output_path, data=data)


if __name__ == '__main__':
  DoMain(sys.argv[1:])
