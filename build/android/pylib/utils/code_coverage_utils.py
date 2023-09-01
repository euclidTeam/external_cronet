# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for code coverage related processings."""

import logging
import os
import posixpath
import subprocess

from devil import base_error
from pylib import constants

# These are use for code coverage.
LLVM_PROFDATA_PATH = os.path.join(constants.DIR_SOURCE_ROOT, 'third_party',
                                  'llvm-build', 'Release+Asserts', 'bin',
                                  'llvm-profdata')
# Name of the file extension for profraw data files.
_PROFRAW_FILE_EXTENSION = 'profraw'
# Name of the file where profraw data files are merged.
_MERGE_PROFDATA_FILE_NAME = 'coverage_merged.' + _PROFRAW_FILE_EXTENSION


def GetDeviceClangCoverageDir(device):
  """Gets the directory to generate clang coverage data on device.

  Args:
    device: The working device.

  Returns:
    The directory path on the device.
  """
  return posixpath.join(device.GetExternalStoragePath(), 'chrome', 'test',
                        'coverage', 'profraw')


def PullClangCoverageFiles(device, device_coverage_dir, output_dir):
  """Pulls clang coverage files on device to host directory.

  Args:
    device: The working device.
    device_coverage_dir: The directory to store coverage data on device.
    output_dir: The output directory on host.
  """
  try:
    if not os.path.exists(output_dir):
      os.makedirs(output_dir)
    device.PullFile(device_coverage_dir, output_dir)
    if not os.listdir(os.path.join(output_dir, 'profraw')):
      logging.warning('No clang coverage data was generated for this run')
  except (OSError, base_error.BaseError) as e:
    logging.warning('Failed to pull clang coverage data, error: %s', e)
  finally:
    device.RemovePath(device_coverage_dir, force=True, recursive=True)


def MergeClangCoverageFiles(coverage_dir, profdata_dir):
  """Merge coverage data files.

  Each instrumentation activity generates a separate profraw data file. This
  merges all profraw files in profdata_dir into a single file in
  coverage_dir. This happens after each test, rather than waiting until after
  all tests are ran to reduce the memory footprint used by all the profraw
  files.

  Args:
    coverage_dir: The path to the coverage directory.
    profdata_dir: The directory where the profraw data file(s) are located.

  Return:
    None
  """
  # profdata_dir may not exist if pulling coverage files failed.
  if not os.path.exists(profdata_dir):
    logging.debug('Profraw directory does not exist: %s', profdata_dir)
    return

  merge_file = os.path.join(coverage_dir, _MERGE_PROFDATA_FILE_NAME)
  profraw_files = [
      os.path.join(profdata_dir, f) for f in os.listdir(profdata_dir)
      if f.endswith(_PROFRAW_FILE_EXTENSION)
  ]

  try:
    logging.debug('Merging target profraw files into merged profraw file.')
    subprocess_cmd = [
        LLVM_PROFDATA_PATH,
        'merge',
        '-o',
        merge_file,
        '-sparse=true',
    ]
    # Grow the merge file by merging it with itself and the new files.
    if os.path.exists(merge_file):
      subprocess_cmd.append(merge_file)
    subprocess_cmd.extend(profraw_files)
    output = subprocess.check_output(subprocess_cmd).decode('utf8')
    logging.debug('Merge output: %s', output)

  except subprocess.CalledProcessError:
    # Don't raise error as that will kill the test run. When code coverage
    # generates a report, that will raise the error in the report generation.
    logging.error(
        'Failed to merge target profdata files to create '
        'merged profraw file for files: %s', profraw_files)

  # Free up memory space on bot as all data is in the merge file.
  for f in profraw_files:
    os.remove(f)
