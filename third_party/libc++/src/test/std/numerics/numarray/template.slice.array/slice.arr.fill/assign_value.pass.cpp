//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// <valarray>

// template <class T> class slice_array

// void operator=(const value_type& x) const;

#include <valarray>
#include <cassert>

#include "test_macros.h"

int main(int, char**)
{
    int a1[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    std::valarray<int> v1(a1, sizeof(a1)/sizeof(a1[0]));
    v1[std::slice(1, 5, 3)] = 20;
    assert(v1.size() == 16);
    assert(v1[ 0] ==  0);
    assert(v1[ 1] == 20);
    assert(v1[ 2] ==  2);
    assert(v1[ 3] ==  3);
    assert(v1[ 4] == 20);
    assert(v1[ 5] ==  5);
    assert(v1[ 6] ==  6);
    assert(v1[ 7] == 20);
    assert(v1[ 8] ==  8);
    assert(v1[ 9] ==  9);
    assert(v1[10] == 20);
    assert(v1[11] == 11);
    assert(v1[12] == 12);
    assert(v1[13] == 20);
    assert(v1[14] == 14);
    assert(v1[15] == 15);

  return 0;
}
