//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// UNSUPPORTED: no-threads
// UNSUPPORTED: c++03, c++11

// XFAIL: availability-synchronization_library-missing

// <barrier>

#include <barrier>
#include <thread>
#include <cassert>

#include "make_test_thread.h"
#include "test_macros.h"

int main(int, char**)
{
  std::barrier<> b(2);

  std::thread t = support::make_test_thread([&](){
    b.arrive_and_drop();
  });

  b.arrive_and_wait();
  b.arrive_and_wait();
  t.join();
  return 0;
}
