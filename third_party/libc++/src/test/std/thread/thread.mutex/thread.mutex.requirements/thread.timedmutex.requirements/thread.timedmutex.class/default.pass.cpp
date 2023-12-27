//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: no-threads

// <mutex>

// class timed_mutex;

// timed_mutex();

#include <mutex>

#include "test_macros.h"

int main(int, char**)
{
    std::timed_mutex m;

  return 0;
}
