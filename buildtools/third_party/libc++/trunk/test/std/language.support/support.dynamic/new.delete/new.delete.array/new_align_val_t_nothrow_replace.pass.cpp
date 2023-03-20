//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14
// UNSUPPORTED: sanitizer-new-delete

// XFAIL: LIBCXX-AIX-FIXME

// Aligned allocation was not provided before macosx10.13 and as a result we
// get availability errors when the deployment target is older than macosx10.13.
// XFAIL: use_system_cxx_lib && target={{.+}}-apple-macosx10.{{9|10|11|12}}

// Libcxx when built for z/OS doesn't contain the aligned allocation functions,
// nor does the dynamic library shipped with z/OS.
// UNSUPPORTED: target={{.+}}-zos{{.*}}

// test operator new nothrow by replacing only operator new

#include <new>
#include <cstddef>
#include <cstdlib>
#include <cassert>
#include <limits>

#include "test_macros.h"

constexpr auto OverAligned = __STDCPP_DEFAULT_NEW_ALIGNMENT__ * 2;

int A_constructed = 0;

struct alignas(OverAligned) A
{
    A() {++A_constructed;}
    ~A() {--A_constructed;}
};

int B_constructed = 0;

struct B {
  std::max_align_t member;
  B() { ++B_constructed; }
  ~B() { --B_constructed; }
};

int new_called = 0;
alignas(OverAligned) char Buff[OverAligned * 3];

void* operator new[](std::size_t s, std::align_val_t a) TEST_THROW_SPEC(std::bad_alloc)
{
    assert(!new_called);
    assert(s <= sizeof(Buff));
    assert(static_cast<std::size_t>(a) == OverAligned);
    ++new_called;
    return Buff;
}

void  operator delete[](void* p, std::align_val_t a) TEST_NOEXCEPT
{
    ASSERT_WITH_OPERATOR_NEW_FALLBACKS(p == Buff);
    assert(static_cast<std::size_t>(a) == OverAligned);
    ASSERT_WITH_OPERATOR_NEW_FALLBACKS(new_called);
    --new_called;
}

int main(int, char**)
{
    {
        A* ap = new (std::nothrow) A[2];
        assert(ap);
        assert(A_constructed == 2);
        ASSERT_WITH_OPERATOR_NEW_FALLBACKS(new_called);
        delete [] ap;
        assert(A_constructed == 0);
        ASSERT_WITH_OPERATOR_NEW_FALLBACKS(!new_called);
    }
    {
        B* bp = new (std::nothrow) B[2];
        assert(bp);
        assert(B_constructed == 2);
        ASSERT_WITH_OPERATOR_NEW_FALLBACKS(!new_called);
        delete [] bp;
        ASSERT_WITH_OPERATOR_NEW_FALLBACKS(!new_called);
        assert(!B_constructed);
    }

  return 0;
}
