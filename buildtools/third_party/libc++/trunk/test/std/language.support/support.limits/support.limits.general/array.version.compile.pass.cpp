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

// <array>

// Test the feature test macros defined by <array>

/*  Constant                                Value
    __cpp_lib_array_constexpr               201603L [C++17]
                                            201811L [C++20]
    __cpp_lib_nonmember_container_access    201411L [C++17]
    __cpp_lib_to_array                      201907L [C++20]
*/

#include <array>
#include "test_macros.h"

#if TEST_STD_VER < 14

# ifdef __cpp_lib_array_constexpr
#   error "__cpp_lib_array_constexpr should not be defined before c++17"
# endif

# ifdef __cpp_lib_nonmember_container_access
#   error "__cpp_lib_nonmember_container_access should not be defined before c++17"
# endif

# ifdef __cpp_lib_to_array
#   error "__cpp_lib_to_array should not be defined before c++20"
# endif

#elif TEST_STD_VER == 14

# ifdef __cpp_lib_array_constexpr
#   error "__cpp_lib_array_constexpr should not be defined before c++17"
# endif

# ifdef __cpp_lib_nonmember_container_access
#   error "__cpp_lib_nonmember_container_access should not be defined before c++17"
# endif

# ifdef __cpp_lib_to_array
#   error "__cpp_lib_to_array should not be defined before c++20"
# endif

#elif TEST_STD_VER == 17

# ifndef __cpp_lib_array_constexpr
#   error "__cpp_lib_array_constexpr should be defined in c++17"
# endif
# if __cpp_lib_array_constexpr != 201603L
#   error "__cpp_lib_array_constexpr should have the value 201603L in c++17"
# endif

# ifndef __cpp_lib_nonmember_container_access
#   error "__cpp_lib_nonmember_container_access should be defined in c++17"
# endif
# if __cpp_lib_nonmember_container_access != 201411L
#   error "__cpp_lib_nonmember_container_access should have the value 201411L in c++17"
# endif

# ifdef __cpp_lib_to_array
#   error "__cpp_lib_to_array should not be defined before c++20"
# endif

#elif TEST_STD_VER == 20

# ifndef __cpp_lib_array_constexpr
#   error "__cpp_lib_array_constexpr should be defined in c++20"
# endif
# if __cpp_lib_array_constexpr != 201811L
#   error "__cpp_lib_array_constexpr should have the value 201811L in c++20"
# endif

# ifndef __cpp_lib_nonmember_container_access
#   error "__cpp_lib_nonmember_container_access should be defined in c++20"
# endif
# if __cpp_lib_nonmember_container_access != 201411L
#   error "__cpp_lib_nonmember_container_access should have the value 201411L in c++20"
# endif

# ifndef __cpp_lib_to_array
#   error "__cpp_lib_to_array should be defined in c++20"
# endif
# if __cpp_lib_to_array != 201907L
#   error "__cpp_lib_to_array should have the value 201907L in c++20"
# endif

#elif TEST_STD_VER > 20

# ifndef __cpp_lib_array_constexpr
#   error "__cpp_lib_array_constexpr should be defined in c++2b"
# endif
# if __cpp_lib_array_constexpr != 201811L
#   error "__cpp_lib_array_constexpr should have the value 201811L in c++2b"
# endif

# ifndef __cpp_lib_nonmember_container_access
#   error "__cpp_lib_nonmember_container_access should be defined in c++2b"
# endif
# if __cpp_lib_nonmember_container_access != 201411L
#   error "__cpp_lib_nonmember_container_access should have the value 201411L in c++2b"
# endif

# ifndef __cpp_lib_to_array
#   error "__cpp_lib_to_array should be defined in c++2b"
# endif
# if __cpp_lib_to_array != 201907L
#   error "__cpp_lib_to_array should have the value 201907L in c++2b"
# endif

#endif // TEST_STD_VER > 20

