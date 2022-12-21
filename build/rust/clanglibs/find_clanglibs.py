#!/usr/bin/env/python3

# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Enumerate all clang libraries and generate a .rs file which is built into a
# library that links all the clanglibs into it. Depending on that library
# ensures linking against our clang libraries.
# See BUILD.gn in this directory for more context.

import argparse
import os
import stat
import sys
import shutil
import subprocess
import re

LIB_BASENAME_RE = re.compile(r"lib(.*)\.(?:lib|a)$", re.I)


def main():
  parser = argparse.ArgumentParser("find_clanglibs.py")
  parser.add_argument("--output", help="Path to rsp file", required=True)
  parser.add_argument("--depfile", help="Path to write depfile", required=True)
  parser.add_argument("--clang-libs-dir",
                      help="Where to find clang and LLVM libs",
                      required=True)
  args = parser.parse_args()
  if not os.path.exists(os.path.join(
      args.clang_libs_dir, "libclang.a")) and not os.path.exists(
          os.path.join(args.clang_libs_dir, "libclang.lib")):
    print("Couldn't find libclang.a|lib. Please ensure you set a custom var in "
          "your .gclient file like so:\n"
          "  \"custom_vars\": { \"checkout_clang_libs\": True, }\n"
          "Then run gclient sync; rm "
          "<out dir>/obj/build/rust/clanglibs/clang_libs.rsp.")
    return -1
  with open(args.depfile, 'w') as depfile:
    depfile.write("%s:" % args.output)
    with open(args.output, 'w') as output:
      output.write("//! Generated by build/rust/clanglibs/find_clanglibs.py\n")
      output.write("//! @generated\n")
      for f in os.listdir(args.clang_libs_dir):
        m = LIB_BASENAME_RE.match(f)
        if m:
          basename = m.group(1)
          output.write("#[link(name=\"{}\")]\n".format(basename))
          full_path = os.path.join(args.clang_libs_dir, f)
          depfile.write(" {}\n".format(full_path))
      output.write("#[link(name=\"stdc++\")]\n")
      output.write("#[link(name=\"z\")]\n")
      output.write("extern {}\n")


if __name__ == '__main__':
  sys.exit(main())
