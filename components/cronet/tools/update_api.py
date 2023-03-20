#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""update_api.py - Update committed Cronet API."""



import argparse
import filecmp
import fileinput
import hashlib
import os
import re
import shutil
import sys
import tempfile


REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))

sys.path.insert(0, os.path.join(REPOSITORY_ROOT, 'build/android/gyp'))
from util import build_utils  # pylint: disable=wrong-import-position

# Filename of dump of current API.
API_FILENAME = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', 'android', 'api.txt'))
# Filename of file containing the interface API version number.
INTERFACE_API_VERSION_FILENAME = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', 'android', 'interface_api_version.txt'))

# Regular expression that catches the beginning of lines that declare classes.
# The first group returned by a match is the class name.
CLASS_RE = re.compile(r'.*class ([^ ]*) .*\{')

# Regular expression that matches a string containing an unnamed class name,
# for example 'Foo$1'.
UNNAMED_CLASS_RE = re.compile(r'.*\$[0-9]')

# javap still prints internal (package private, nested...) classes even though
# -protected is passed so they need to be filtered out.
INTERNAL_CLASS_RE = re.compile(r'^(?!public ((final|abstract) )?class).*')

JAR_PATH = os.path.join(build_utils.JAVA_HOME, 'bin', 'jar')
JAVAP_PATH = os.path.join(build_utils.JAVA_HOME, 'bin', 'javap')


def generate_api(api_jar, output_filename):
  # Dumps the API in |api_jar| into |outpuf_filename|.

  with open(output_filename, 'w') as output_file:
    output_file.write(
        'DO NOT EDIT THIS FILE, USE update_api.py TO UPDATE IT\n\n')

  # Extract API class files from api_jar.
  temp_dir = tempfile.mkdtemp()
  old_cwd = os.getcwd()
  api_jar_path = os.path.abspath(api_jar)
  jar_cmd = '%s xf %s' % (os.path.relpath(JAR_PATH, temp_dir), api_jar_path)
  os.chdir(temp_dir)
  if os.system(jar_cmd):
    print('ERROR: jar failed on ' + api_jar)
    return False
  os.chdir(old_cwd)
  shutil.rmtree(os.path.join(temp_dir, 'META-INF'), ignore_errors=True)

  # Collect names of all API class files
  api_class_files = []
  for root, _, filenames in os.walk(temp_dir):
    api_class_files += [os.path.join(root, f) for f in filenames]
  api_class_files.sort()

  # Dump API class files into |output_filename|
  javap_cmd = (
      '%s -protected %s >> %s' % (
          JAVAP_PATH, ' '.join(api_class_files), output_filename)
  ).replace('$', '\\$')
  if os.system(javap_cmd):
    print('ERROR: javap command failed: ' + javap_cmd)
    return False
  shutil.rmtree(temp_dir)

  # Strip out pieces we don't need to compare.
  output_file = fileinput.FileInput(output_filename, inplace=True)
  skip_to_next_class = False
  md5_hash = hashlib.md5()
  for line in output_file:
    # Skip 'Compiled from ' lines as they're not part of the API.
    if line.startswith('Compiled from "'):
      continue
    if CLASS_RE.match(line):
      skip_to_next_class = (
          # Skip internal classes, they aren't exposed.
          UNNAMED_CLASS_RE.match(line)) or (INTERNAL_CLASS_RE.match(line))
    if skip_to_next_class:
      skip_to_next_class = line != '}'
      continue
    md5_hash.update(line.encode('utf8'))
    sys.stdout.write(line)
  output_file.close()
  with open(output_filename, 'a') as output_file:
    output_file.write('Stamp: %s\n' % md5_hash.hexdigest())
  return True


def check_up_to_date(api_jar):
  # Returns True if API_FILENAME matches the API exposed by |api_jar|.

  [_, temp_filename] = tempfile.mkstemp()
  if not generate_api(api_jar, temp_filename):
    return False
  ret = filecmp.cmp(API_FILENAME, temp_filename)
  os.remove(temp_filename)
  return ret


def check_api_update(old_api, new_api):
  # Enforce that lines are only added when updating API.
  new_hash = hashlib.md5()
  old_hash = hashlib.md5()
  seen_stamp = False
  with open(old_api, 'r') as old_api_file, open(new_api, 'r') as new_api_file:
    for old_line in old_api_file:
      while True:
        new_line = new_api_file.readline()
        if seen_stamp:
          print('ERROR: Stamp is not the last line.')
          return False
        if new_line.startswith('Stamp: ') and old_line.startswith('Stamp: '):
          if old_line != 'Stamp: %s\n' % old_hash.hexdigest():
            print('ERROR: Prior api.txt not stamped by update_api.py')
            return False
          if new_line != 'Stamp: %s\n' % new_hash.hexdigest():
            print('ERROR: New api.txt not stamped by update_api.py')
            return False
          seen_stamp = True
          break
        new_hash.update(new_line.encode('utf8'))
        if new_line == old_line:
          break
        if not new_line:
          if old_line.startswith('Stamp: '):
            print('ERROR: New api.txt not stamped by update_api.py')
          else:
            print('ERROR: This API was modified or removed:')
            print('           ' + old_line)
            print('       Cronet API methods and classes cannot be modified.')
          return False
      old_hash.update(old_line.encode('utf8'))
  if not seen_stamp:
    print('ERROR: api.txt not stamped by update_api.py.')
    return False
  return True


def main(args):
  parser = argparse.ArgumentParser(description='Update Cronet api.txt.')
  parser.add_argument('--api_jar',
                      help='Path to API jar (i.e. cronet_api.jar)',
                      required=True,
                      metavar='path/to/cronet_api.jar')
  opts = parser.parse_args(args)

  if check_up_to_date(opts.api_jar):
    return True

  [_, temp_filename] = tempfile.mkstemp()
  if (generate_api(opts.api_jar, temp_filename) and
      check_api_update(API_FILENAME, temp_filename)):
    # Update API version number to new version number
    with open(INTERFACE_API_VERSION_FILENAME,'r+') as f:
      version = int(f.read())
      f.seek(0)
      f.write(str(version + 1))
    # Update API file to new API
    shutil.move(temp_filename, API_FILENAME)
    return True
  os.remove(temp_filename)
  return False


if __name__ == '__main__':
  sys.exit(0 if main(sys.argv[1:]) else -1)
