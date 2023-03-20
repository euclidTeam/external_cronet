# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import collections
import itertools
import json
import logging
import time

import six

from pylib.base import base_test_result

def GenerateResultsDict(test_run_results, global_tags=None):
  """Create a results dict from |test_run_results| suitable for writing to JSON.
  Args:
    test_run_results: a list of base_test_result.TestRunResults objects.
  Returns:
    A results dict that mirrors the one generated by
      base/test/launcher/test_results_tracker.cc:SaveSummaryAsJSON.
  """
  # Example json output.
  # {
  #   "global_tags": [],
  #   "all_tests": [
  #     "test1",
  #     "test2",
  #    ],
  #   "disabled_tests": [],
  #   "per_iteration_data": [
  #     {
  #       "test1": [
  #         {
  #           "status": "SUCCESS",
  #           "elapsed_time_ms": 1,
  #           "output_snippet": "",
  #           "output_snippet_base64": "",
  #           "losless_snippet": "",
  #         },
  #         ...
  #       ],
  #       "test2": [
  #         {
  #           "status": "FAILURE",
  #           "elapsed_time_ms": 12,
  #           "output_snippet": "",
  #           "output_snippet_base64": "",
  #           "losless_snippet": "",
  #         },
  #         ...
  #       ],
  #     },
  #     {
  #       "test1": [
  #         {
  #           "status": "SUCCESS",
  #           "elapsed_time_ms": 1,
  #           "output_snippet": "",
  #           "output_snippet_base64": "",
  #           "losless_snippet": "",
  #         },
  #       ],
  #       "test2": [
  #         {
  #           "status": "FAILURE",
  #           "elapsed_time_ms": 12,
  #           "output_snippet": "",
  #           "output_snippet_base64": "",
  #           "losless_snippet": "",
  #         },
  #       ],
  #     },
  #     ...
  #   ],
  # }

  all_tests = set()
  per_iteration_data = []
  test_run_links = {}

  for test_run_result in test_run_results:
    iteration_data = collections.defaultdict(list)
    if isinstance(test_run_result, list):
      results_iterable = itertools.chain(*(t.GetAll() for t in test_run_result))
      for tr in test_run_result:
        test_run_links.update(tr.GetLinks())

    else:
      results_iterable = test_run_result.GetAll()
      test_run_links.update(test_run_result.GetLinks())

    for r in results_iterable:
      result_dict = {
          'status': r.GetType(),
          'elapsed_time_ms': r.GetDuration(),
          'output_snippet': six.ensure_text(r.GetLog(), errors='replace'),
          'losless_snippet': True,
          'output_snippet_base64': '',
          'links': r.GetLinks(),
      }
      iteration_data[r.GetName()].append(result_dict)

    all_tests = all_tests.union(set(six.iterkeys(iteration_data)))
    per_iteration_data.append(iteration_data)

  return {
    'global_tags': global_tags or [],
    'all_tests': sorted(list(all_tests)),
    # TODO(jbudorick): Add support for disabled tests within base_test_result.
    'disabled_tests': [],
    'per_iteration_data': per_iteration_data,
    'links': test_run_links,
  }


def GenerateJsonTestResultFormatDict(test_run_results, interrupted):
  """Create a results dict from |test_run_results| suitable for writing to JSON.

  Args:
    test_run_results: a list of base_test_result.TestRunResults objects.
    interrupted: True if tests were interrupted, e.g. timeout listing tests
  Returns:
    A results dict that mirrors the standard JSON Test Results Format.
  """

  tests = {}
  counts = {'PASS': 0, 'FAIL': 0, 'SKIP': 0, 'CRASH': 0, 'TIMEOUT': 0}

  for test_run_result in test_run_results:
    if isinstance(test_run_result, list):
      results_iterable = itertools.chain(*(t.GetAll() for t in test_run_result))
    else:
      results_iterable = test_run_result.GetAll()

    for r in results_iterable:
      element = tests
      for key in r.GetName().split('.'):
        if key not in element:
          element[key] = {}
        element = element[key]

      element['expected'] = 'PASS'

      if r.GetType() == base_test_result.ResultType.PASS:
        result = 'PASS'
      elif r.GetType() == base_test_result.ResultType.SKIP:
        result = 'SKIP'
      elif r.GetType() == base_test_result.ResultType.CRASH:
        result = 'CRASH'
      elif r.GetType() == base_test_result.ResultType.TIMEOUT:
        result = 'TIMEOUT'
      else:
        result = 'FAIL'

      if 'actual' in element:
        element['actual'] += ' ' + result
      else:
        counts[result] += 1
        element['actual'] = result
        if result == 'FAIL':
          element['is_unexpected'] = True

      if r.GetDuration() != 0:
        element['time'] = r.GetDuration()

  # Fill in required fields.
  return {
      'interrupted': interrupted,
      'num_failures_by_type': counts,
      'path_delimiter': '.',
      'seconds_since_epoch': time.time(),
      'tests': tests,
      'version': 3,
  }


def GenerateJsonResultsFile(test_run_result, file_path, global_tags=None,
                            **kwargs):
  """Write |test_run_result| to JSON.

  This emulates the format of the JSON emitted by
  base/test/launcher/test_results_tracker.cc:SaveSummaryAsJSON.

  Args:
    test_run_result: a base_test_result.TestRunResults object.
    file_path: The path to the JSON file to write.
  """
  with open(file_path, 'w') as json_result_file:
    json_result_file.write(json.dumps(
        GenerateResultsDict(test_run_result, global_tags=global_tags),
        **kwargs))
    logging.info('Generated json results file at %s', file_path)


def GenerateJsonTestResultFormatFile(test_run_result, interrupted, file_path,
                                     **kwargs):
  """Write |test_run_result| to JSON.

  This uses the official Chromium Test Results Format.

  Args:
    test_run_result: a base_test_result.TestRunResults object.
    interrupted: True if tests were interrupted, e.g. timeout listing tests
    file_path: The path to the JSON file to write.
  """
  with open(file_path, 'w') as json_result_file:
    json_result_file.write(
        json.dumps(
            GenerateJsonTestResultFormatDict(test_run_result, interrupted),
            **kwargs))
    logging.info('Generated json results file at %s', file_path)


def ParseResultsFromJson(json_results):
  """Creates a list of BaseTestResult objects from JSON.

  Args:
    json_results: A JSON dict in the format created by
                  GenerateJsonResultsFile.
  """

  def string_as_status(s):
    if s in base_test_result.ResultType.GetTypes():
      return s
    return base_test_result.ResultType.UNKNOWN

  results_list = []
  testsuite_runs = json_results['per_iteration_data']
  for testsuite_run in testsuite_runs:
    for test, test_runs in six.iteritems(testsuite_run):
      results_list.extend(
          [base_test_result.BaseTestResult(test,
                                           string_as_status(tr['status']),
                                           duration=tr['elapsed_time_ms'],
                                           log=tr.get('output_snippet'))
          for tr in test_runs])
  return results_list
