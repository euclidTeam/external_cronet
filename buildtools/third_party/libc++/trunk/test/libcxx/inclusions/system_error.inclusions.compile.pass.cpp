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

// <system_error>

// Test that <system_error> includes all the other headers it's supposed to.

#include <system_error>
#include "test_macros.h"

#if !defined(_LIBCPP_SYSTEM_ERROR)
 #   error "<system_error> was expected to define _LIBCPP_SYSTEM_ERROR"
#endif
#if TEST_STD_VER > 17 && !defined(_LIBCPP_COMPARE)
 #   error "<system_error> should include <compare> in C++20 and later"
#endif
