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

// UNSUPPORTED: c++03

// <unordered_map>

// Test that <unordered_map> includes all the other headers it's supposed to.

#include <unordered_map>
#include "test_macros.h"

#if !defined(_LIBCPP_UNORDERED_MAP)
 #   error "<unordered_map> was expected to define _LIBCPP_UNORDERED_MAP"
#endif
#if TEST_STD_VER > 17 && !defined(_LIBCPP_COMPARE)
 #   error "<unordered_map> should include <compare> in C++20 and later"
#endif
#if TEST_STD_VER > 03 && !defined(_LIBCPP_INITIALIZER_LIST)
 #   error "<unordered_map> should include <initializer_list> in C++11 and later"
#endif
