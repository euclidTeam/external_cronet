#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing publish_package.py."""

import argparse
import unittest
import unittest.mock as mock

from io import StringIO

import publish_package

_PACKAGES = ['test_package']
_REPO = 'test_repo'


class PublishPackageTest(unittest.TestCase):
    """Unittests for publish_package.py."""

    def setUp(self) -> None:
        self._ffx_patcher = mock.patch('publish_package.run_ffx_command')
        self._ffx_mock = self._ffx_patcher.start()
        self.addCleanup(self._ffx_mock.stop)

    def test_new_repo(self) -> None:
        """Test setting |new_repo| to True in |publish_packages|."""

        publish_package.publish_packages(_PACKAGES, _REPO, True)
        self.assertEqual(self._ffx_mock.call_count, 2)
        first_call, second_call = self._ffx_mock.call_args_list
        self.assertEqual(['repository', 'create', _REPO],
                         first_call.kwargs['cmd'])
        self.assertEqual([
            'repository', 'publish', '--package-archive', _PACKAGES[0], _REPO
        ], second_call.kwargs['cmd'])

    def test_no_new_repo(self) -> None:
        """Test setting |new_repo| to False in |publish_packages|."""

        publish_package.publish_packages(['test_package'], 'test_repo', False)
        self.assertEqual(self._ffx_mock.call_count, 1)


    def test_allow_temp_repo(self) -> None:
        """Test setting |allow_temp_repo| to True in |register_package_args|."""

        parser = argparse.ArgumentParser()
        publish_package.register_package_args(parser, True)
        args = parser.parse_args(['--no-repo-init'])
        self.assertEqual(args.no_repo_init, True)

    @mock.patch('sys.stderr', new_callable=StringIO)
    def test_not_allow_temp_repo(self, mock_stderr) -> None:
        """Test setting |allow_temp_repo| to False in
        |register_package_args|."""

        parser = argparse.ArgumentParser()
        publish_package.register_package_args(parser)
        with self.assertRaises(SystemExit):
            parser.parse_args(['--no-repo-init'])
        self.assertRegex(mock_stderr.getvalue(), 'unrecognized arguments')

    def test_main_no_repo_flag(self) -> None:
        """Tests that not specifying packages raise a ValueError."""

        with mock.patch('sys.argv', ['publish_package.py', '--repo', _REPO]):
            with self.assertRaises(ValueError):
                publish_package.main()

    def test_main_no_packages_flag(self) -> None:
        """Tests that not specifying directory raise a ValueError."""

        with mock.patch('sys.argv',
                        ['publish_package.py', '--packages', _PACKAGES[0]]):
            with self.assertRaises(ValueError):
                publish_package.main()

    def test_main_no_out_dir_flag(self) -> None:
        """Tests |main| with `out_dir` omitted."""

        with mock.patch('sys.argv', [
                'publish_package.py', '--packages', _PACKAGES[0], '--repo',
                _REPO
        ]):
            publish_package.main()
            self.assertEqual(self._ffx_mock.call_count, 1)

    @mock.patch('publish_package.read_package_paths')
    def test_main(self, read_mock) -> None:
        """Tests |main|."""

        read_mock.return_value = ['out/test/package/path']
        with mock.patch('sys.argv', [
                'publish_package.py', '--packages', _PACKAGES[0], '--repo',
                _REPO, '--out-dir', 'out/test'
        ]):
            publish_package.main()
            self.assertEqual(self._ffx_mock.call_count, 1)

    @mock.patch('publish_package.read_package_paths')
    @mock.patch('publish_package.make_clean_directory')
    def test_purge_repo(self, read_mock, make_clean_directory_mock) -> None:
        """Tests purge_repo flag."""

        read_mock.return_value = ['out/test/package/path']
        with mock.patch('sys.argv', [
                'publish_package.py', '--packages', _PACKAGES[0], '--repo',
                _REPO, '--out-dir', 'out/test', '--purge-repo'
        ]):
            publish_package.main()
            self.assertEqual(self._ffx_mock.call_count, 2)
            self.assertEqual(make_clean_directory_mock.call_count, 1)


if __name__ == '__main__':
    unittest.main()
