#!/usr/bin/env python3
#
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Writes list of native libraries to srcjar file."""

import argparse
import os
import sys
import zipfile

from util import build_utils


_NATIVE_LIBRARIES_TEMPLATE = """\
// This file is autogenerated by
//     build/android/gyp/write_native_libraries_java.py
// Please do not change its content.

package org.chromium.build;

public class NativeLibraries {{
    public static final int CPU_FAMILY_UNKNOWN = 0;
    public static final int CPU_FAMILY_ARM = 1;
    public static final int CPU_FAMILY_MIPS = 2;
    public static final int CPU_FAMILY_X86 = 3;

    // Set to true to enable the use of the Chromium Linker.
    public static {MAYBE_FINAL}boolean sUseLinker{USE_LINKER};

    // This is the list of native libraries to be loaded (in the correct order)
    // by LibraryLoader.java.
    public static {MAYBE_FINAL}String[] LIBRARIES = {{{LIBRARIES}}};

    public static {MAYBE_FINAL}int sCpuFamily = {CPU_FAMILY};
}}
"""


def _FormatLibraryName(library_name):
  filename = os.path.split(library_name)[1]
  assert filename.startswith('lib')
  assert filename.endswith('.so')
  # Remove lib prefix and .so suffix.
  return '"%s"' % filename[3:-3]


def main():
  parser = argparse.ArgumentParser()

  build_utils.AddDepfileOption(parser)
  parser.add_argument('--final', action='store_true', help='Use final fields.')
  parser.add_argument(
      '--enable-chromium-linker',
      action='store_true',
      help='Enable Chromium linker.')
  parser.add_argument(
      '--native-libraries-list', help='File with list of native libraries.')
  parser.add_argument(
      '--cpu-family',
      choices={
          'CPU_FAMILY_ARM', 'CPU_FAMILY_X86', 'CPU_FAMILY_MIPS',
          'CPU_FAMILY_UNKNOWN'
      },
      required=True,
      default='CPU_FAMILY_UNKNOWN',
      help='CPU family.')
  parser.add_argument(
      '--main-component-library',
      help='If used, the list of native libraries will only contain this '
      'library. Dependencies are found in the library\'s "NEEDED" section.')

  parser.add_argument(
      '--output', required=True, help='Path to the generated srcjar file.')

  options = parser.parse_args(build_utils.ExpandFileArgs(sys.argv[1:]))

  native_libraries = []
  if options.main_component_library:
    native_libraries.append(options.main_component_library)
  elif options.native_libraries_list:
    with open(options.native_libraries_list) as f:
      native_libraries.extend(l.strip() for l in f)

  if options.enable_chromium_linker and len(native_libraries) > 1:
    sys.stderr.write(
        'Multiple libraries not supported when using chromium linker. Found:\n')
    sys.stderr.write('\n'.join(native_libraries))
    sys.stderr.write('\n')
    sys.exit(1)

  def bool_str(value):
    if value:
      return ' = true'
    if options.final:
      return ' = false'
    return ''

  format_dict = {
      'MAYBE_FINAL': 'final ' if options.final else '',
      'USE_LINKER': bool_str(options.enable_chromium_linker),
      'LIBRARIES': ','.join(_FormatLibraryName(n) for n in native_libraries),
      'CPU_FAMILY': options.cpu_family,
  }
  with build_utils.AtomicOutput(options.output) as f:
    with zipfile.ZipFile(f.name, 'w') as srcjar_file:
      build_utils.AddToZipHermetic(
          zip_file=srcjar_file,
          zip_path='org/chromium/build/NativeLibraries.java',
          data=_NATIVE_LIBRARIES_TEMPLATE.format(**format_dict))

  if options.depfile:
    assert options.native_libraries_list
    build_utils.WriteDepfile(options.depfile,
                             options.output,
                             inputs=[options.native_libraries_list])


if __name__ == '__main__':
  sys.exit(main())
