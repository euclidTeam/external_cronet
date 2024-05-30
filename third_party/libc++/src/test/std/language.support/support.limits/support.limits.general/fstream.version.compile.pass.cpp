//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// WARNING: This test was generated by generate_feature_test_macro_components.py
// and should not be edited manually.
//
// clang-format off

// UNSUPPORTED: no-localization

// <fstream>

// Test the feature test macros defined by <fstream>

/*  Constant                           Value
    __cpp_lib_fstream_native_handle    202306L [C++26]
*/

#include <fstream>
#include "test_macros.h"

#if TEST_STD_VER < 14

# ifdef __cpp_lib_fstream_native_handle
#   error "__cpp_lib_fstream_native_handle should not be defined before c++26"
# endif

#elif TEST_STD_VER == 14

# ifdef __cpp_lib_fstream_native_handle
#   error "__cpp_lib_fstream_native_handle should not be defined before c++26"
# endif

#elif TEST_STD_VER == 17

# ifdef __cpp_lib_fstream_native_handle
#   error "__cpp_lib_fstream_native_handle should not be defined before c++26"
# endif

#elif TEST_STD_VER == 20

# ifdef __cpp_lib_fstream_native_handle
#   error "__cpp_lib_fstream_native_handle should not be defined before c++26"
# endif

#elif TEST_STD_VER == 23

# ifdef __cpp_lib_fstream_native_handle
#   error "__cpp_lib_fstream_native_handle should not be defined before c++26"
# endif

#elif TEST_STD_VER > 23

# ifndef __cpp_lib_fstream_native_handle
#   error "__cpp_lib_fstream_native_handle should be defined in c++26"
# endif
# if __cpp_lib_fstream_native_handle != 202306L
#   error "__cpp_lib_fstream_native_handle should have the value 202306L in c++26"
# endif

#endif // TEST_STD_VER > 23

