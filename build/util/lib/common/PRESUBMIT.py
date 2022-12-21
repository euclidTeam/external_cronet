# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


USE_PYTHON3 = True


def _RunTests(input_api, output_api):
  return (input_api.canned_checks.RunUnitTestsInDirectory(
      input_api,
      output_api,
      '.',
      files_to_check=[r'.+_test.py$'],
      run_on_python2=False,
      skip_shebang_check=True))


def CheckChangeOnUpload(input_api, output_api):
  return _RunTests(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _RunTests(input_api, output_api)
