//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14
// XFAIL: use_system_cxx_lib && target={{.+}}-apple-macosx10.{{9|10|11|12|13|14|15}}
// XFAIL: use_system_cxx_lib && target={{.+}}-apple-macosx{{11.0|12.0}}

// <memory_resource>

// class monotonic_buffer_resource

#include <memory_resource>
#include <cassert>

#include "count_new.h"
#include "test_macros.h"

int main(int, char**) {
  globalMemCounter.reset();
  {
    alignas(4) char buffer[17];
    auto mono1 = std::pmr::monotonic_buffer_resource(buffer + 1, 16, std::pmr::new_delete_resource());
    std::pmr::memory_resource& r1 = mono1;

    void* ret = r1.allocate(1, 1);
    assert(ret == buffer + 1);
    mono1.release();

    ret = r1.allocate(1, 2);
    assert(ret == buffer + 2);
    mono1.release();

    ret = r1.allocate(1, 4);
    assert(ret == buffer + 4);
    mono1.release();

    // Test a size that is just big enough to fit in the buffer,
    // but can't fit if it's aligned.
    ret = r1.allocate(16, 1);
    assert(ret == buffer + 1);
    mono1.release();

    assert(globalMemCounter.checkNewCalledEq(0));
    ret = r1.allocate(16, 2);
    ASSERT_WITH_LIBRARY_INTERNAL_ALLOCATIONS(globalMemCounter.checkNewCalledEq(1));
    ASSERT_WITH_LIBRARY_INTERNAL_ALLOCATIONS(globalMemCounter.checkLastNewSizeGe(16));
    mono1.release();
    ASSERT_WITH_LIBRARY_INTERNAL_ALLOCATIONS(globalMemCounter.checkDeleteCalledEq(1));
  }

  return 0;
}
