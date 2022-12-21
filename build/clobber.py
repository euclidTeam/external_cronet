#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script provides methods for clobbering build directories."""

import argparse
import os
import shutil
import subprocess
import sys


def extract_gn_build_commands(build_ninja_file):
  """Extracts from a build.ninja the commands to run GN.

  The commands to run GN are the gn rule and build.ninja build step at the
  top of the build.ninja file. We want to keep these when deleting GN builds
  since we want to preserve the command-line flags to GN.

  On error, returns the empty string."""
  result = ""
  with open(build_ninja_file, 'r') as f:
    # Read until the third blank line. The first thing GN writes to the file
    # is "ninja_required_version = x.y.z", then the "rule gn" and the third
    # is the section for "build build.ninja", separated by blank lines.
    num_blank_lines = 0
    while num_blank_lines < 3:
      line = f.readline()
      if len(line) == 0:
        return ''  # Unexpected EOF.
      result += line
      if line[0] == '\n':
        num_blank_lines = num_blank_lines + 1
  return result


def delete_dir(build_dir):
  if os.path.islink(build_dir):
    return
  # For unknown reasons (anti-virus?) rmtree of Chromium build directories
  # often fails on Windows.
  if sys.platform.startswith('win'):
    subprocess.check_call(['rmdir', '/s', '/q', build_dir], shell=True)
  else:
    shutil.rmtree(build_dir)


def delete_build_dir(build_dir):
  # GN writes a build.ninja.d file. Note that not all GN builds have args.gn.
  build_ninja_d_file = os.path.join(build_dir, 'build.ninja.d')
  if not os.path.exists(build_ninja_d_file):
    delete_dir(build_dir)
    return

  # GN builds aren't automatically regenerated when you sync. To avoid
  # messing with the GN workflow, erase everything but the args file, and
  # write a dummy build.ninja file that will automatically rerun GN the next
  # time Ninja is run.
  build_ninja_file = os.path.join(build_dir, 'build.ninja')
  build_commands = extract_gn_build_commands(build_ninja_file)

  try:
    gn_args_file = os.path.join(build_dir, 'args.gn')
    with open(gn_args_file, 'r') as f:
      args_contents = f.read()
  except IOError:
    args_contents = ''

  e = None
  try:
    # delete_dir and os.mkdir() may fail, such as when chrome.exe is running,
    # and we still want to restore args.gn/build.ninja/build.ninja.d, so catch
    # the exception and rethrow it later.
    delete_dir(build_dir)
    os.mkdir(build_dir)
  except Exception as e:
    pass

  # Put back the args file (if any).
  if args_contents != '':
    with open(gn_args_file, 'w') as f:
      f.write(args_contents)

  # Write the build.ninja file sufficiently to regenerate itself.
  with open(os.path.join(build_dir, 'build.ninja'), 'w') as f:
    if build_commands != '':
      f.write(build_commands)
    else:
      # Couldn't parse the build.ninja file, write a default thing.
      f.write('''ninja_required_version = 1.7.2

rule gn
  command = gn -q gen //out/%s/
  description = Regenerating ninja files

build build.ninja: gn
  generator = 1
  depfile = build.ninja.d
''' % (os.path.split(build_dir)[1]))

  # Write a .d file for the build which references a nonexistant file. This
  # will make Ninja always mark the build as dirty.
  with open(build_ninja_d_file, 'w') as f:
    f.write('build.ninja: nonexistant_file.gn\n')

  if e:
    # Rethrow the exception we caught earlier.
    raise e

def clobber(out_dir):
  """Clobber contents of build directory.

  Don't delete the directory itself: some checkouts have the build directory
  mounted."""
  for f in os.listdir(out_dir):
    path = os.path.join(out_dir, f)
    if os.path.isfile(path):
      os.unlink(path)
    elif os.path.isdir(path):
      delete_build_dir(path)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('out_dir', help='The output directory to clobber')
  args = parser.parse_args()
  clobber(args.out_dir)
  return 0


if __name__ == '__main__':
  sys.exit(main())
