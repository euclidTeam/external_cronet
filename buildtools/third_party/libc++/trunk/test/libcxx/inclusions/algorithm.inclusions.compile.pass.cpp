//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// WARNING: This test was generated by generate_header_inclusion_tests.py
// and should not be edited manually.
//
// clang-format off

// <algorithm>

// Test that <algorithm> includes all the other headers it's supposed to.

#include <algorithm>
#include "test_macros.h"

#if !defined(_LIBCPP_ALGORITHM)
 #   error "<algorithm> was expected to define _LIBCPP_ALGORITHM"
#endif
#if TEST_STD_VER > 03 && !defined(_LIBCPP_INITIALIZER_LIST)
 #   error "<algorithm> should include <initializer_list> in C++11 and later"
#endif
