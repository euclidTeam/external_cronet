#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generate linker version scripts for Chrome on Android shared libraries."""

import argparse
import os

from util import build_utils

_SCRIPT_HEADER = """\
# AUTO-GENERATED FILE.  DO NOT MODIFY.
#
# See: %s

{
  global:
""" % os.path.relpath(__file__, build_utils.DIR_SOURCE_ROOT)

_SCRIPT_FOOTER = """\
  local:
    *;
};
"""


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--output',
      required=True,
      help='Path to output linker version script file.')
  parser.add_argument(
      '--export-java-symbols',
      action='store_true',
      help='Export Java_* JNI methods')
  parser.add_argument('--export-fortesting-java-symbols',
                      action='store_true',
                      help='Export Java_*_ForTesting JNI methods')
  parser.add_argument(
      '--export-symbol-allowlist-file',
      action='append',
      default=[],
      dest='allowlists',
      help='Path to an input file containing an allowlist of extra symbols to '
      'export, one symbol per line. Multiple files may be specified.')
  parser.add_argument(
      '--export-feature-registrations',
      action='store_true',
      help='Export JNI_OnLoad_* methods')
  options = parser.parse_args()

  if options.export_fortesting_java_symbols:
    assert options.export_java_symbols, ('Must export java symbols if exporting'
                                         'fortesting java symbols.')

  # JNI_OnLoad is always exported.
  # CrashpadHandlerMain() is the entry point to the Crashpad handler, required
  # for libcrashpad_handler_trampoline.so.
  symbol_list = ['CrashpadHandlerMain', 'JNI_OnLoad']

  if options.export_java_symbols:
    if options.export_fortesting_java_symbols:
      symbol_list.append('Java_*')
    else:
      # The linker uses unix shell globbing patterns, not regex. So, we have to
      # include everything that doesn't end in "ForTest(ing)" with this set of
      # globs.
      symbol_list.append('Java_*[!F]orTesting')
      symbol_list.append('Java_*[!o]rTesting')
      symbol_list.append('Java_*[!r]Testing')
      symbol_list.append('Java_*[!T]esting')
      symbol_list.append('Java_*[!e]sting')
      symbol_list.append('Java_*[!s]ting')
      symbol_list.append('Java_*[!t]ing')
      symbol_list.append('Java_*[!i]ng')
      symbol_list.append('Java_*[!n]g')
      symbol_list.append('Java_*[!F]orTest')
      symbol_list.append('Java_*[!o]rTest')
      symbol_list.append('Java_*[!r]Test')
      symbol_list.append('Java_*[!T]est')
      symbol_list.append('Java_*[!e]st')
      symbol_list.append('Java_*[!s]t')
      symbol_list.append('Java_*[!gt]')

  if options.export_feature_registrations:
    symbol_list.append('JNI_OnLoad_*')

  for allowlist in options.allowlists:
    with open(allowlist, 'rt') as f:
      for line in f:
        line = line.strip()
        if not line or line[0] == '#':
          continue
        symbol_list.append(line)

  script_content = [_SCRIPT_HEADER]
  for symbol in symbol_list:
    script_content.append('    %s;\n' % symbol)
  script_content.append(_SCRIPT_FOOTER)

  script = ''.join(script_content)

  with build_utils.AtomicOutput(options.output, mode='w') as f:
    f.write(script)


if __name__ == '__main__':
  main()
