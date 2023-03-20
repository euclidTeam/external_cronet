#!/usr/bin/env python3

# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates APKs for use on system images."""

import argparse
import os
import pathlib
import tempfile
import shutil
import sys
import zipfile

_DIR_SOURCE_ROOT = str(pathlib.Path(__file__).parents[2])
sys.path.append(os.path.join(_DIR_SOURCE_ROOT, 'build', 'android', 'gyp'))
from util import build_utils


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--input', required=True, help='Input path')
  parser.add_argument('--output', required=True, help='Output path')
  parser.add_argument('--bundle-wrapper', help='APK operations script path')
  parser.add_argument('--fuse-apk',
                      help='Create single .apk rather than using apk splits',
                      action='store_true')
  args = parser.parse_args()

  if not args.bundle_wrapper:
    shutil.copyfile(args.input, args.output)
    return

  with tempfile.NamedTemporaryFile(suffix='.apks') as tmp_file:
    cmd = [
        args.bundle_wrapper, 'build-bundle-apks', '--output-apks', tmp_file.name
    ]
    cmd += ['--build-mode', 'system' if args.fuse_apk else 'system_apks']

    # Creates a .apks zip file that contains the system image APK(s).
    build_utils.CheckOutput(cmd)

    if args.fuse_apk:
      with zipfile.ZipFile(tmp_file.name) as z:
        pathlib.Path(args.output).write_bytes(z.read('system/system.apk'))
      return

    # Rename .apk files and remove toc.pb to make it clear that system apks
    # should not be installed via bundletool.
    with zipfile.ZipFile(tmp_file.name) as z_input, \
        zipfile.ZipFile(args.output, 'w') as z_output:
      for info in z_input.infolist():
        if info.filename.endswith('.apk'):
          data = z_input.read(info)
          info.filename = (info.filename.replace('splits/',
                                                 '').replace('-master', ''))
          z_output.writestr(info, data)


if __name__ == '__main__':
  sys.exit(main())
