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

// UNSUPPORTED: no-localization

// <ios>

// Test that <ios> includes all the other headers it's supposed to.

#include <ios>
#include "test_macros.h"

#if !defined(_LIBCPP_IOS)
 #   error "<ios> was expected to define _LIBCPP_IOS"
#endif
#if !defined(_LIBCPP_IOSFWD)
 #   error "<ios> should include <iosfwd> in C++03 and later"
#endif
