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

// <charconv>

// Test the feature test macros defined by <charconv>

/*  Constant                        Value
    __cpp_lib_constexpr_charconv    202207L [C++23]
    __cpp_lib_to_chars              201611L [C++17]
                                    202306L [C++26]
*/

#include <charconv>
#include "test_macros.h"

#if TEST_STD_VER < 14

# ifdef __cpp_lib_constexpr_charconv
#   error "__cpp_lib_constexpr_charconv should not be defined before c++23"
# endif

# ifdef __cpp_lib_to_chars
#   error "__cpp_lib_to_chars should not be defined before c++17"
# endif

#elif TEST_STD_VER == 14

# ifdef __cpp_lib_constexpr_charconv
#   error "__cpp_lib_constexpr_charconv should not be defined before c++23"
# endif

# ifdef __cpp_lib_to_chars
#   error "__cpp_lib_to_chars should not be defined before c++17"
# endif

#elif TEST_STD_VER == 17

# ifdef __cpp_lib_constexpr_charconv
#   error "__cpp_lib_constexpr_charconv should not be defined before c++23"
# endif

# if !defined(_LIBCPP_VERSION)
#   ifndef __cpp_lib_to_chars
#     error "__cpp_lib_to_chars should be defined in c++17"
#   endif
#   if __cpp_lib_to_chars != 201611L
#     error "__cpp_lib_to_chars should have the value 201611L in c++17"
#   endif
# else // _LIBCPP_VERSION
#   ifdef __cpp_lib_to_chars
#     error "__cpp_lib_to_chars should not be defined because it is unimplemented in libc++!"
#   endif
# endif

#elif TEST_STD_VER == 20

# ifdef __cpp_lib_constexpr_charconv
#   error "__cpp_lib_constexpr_charconv should not be defined before c++23"
# endif

# if !defined(_LIBCPP_VERSION)
#   ifndef __cpp_lib_to_chars
#     error "__cpp_lib_to_chars should be defined in c++20"
#   endif
#   if __cpp_lib_to_chars != 201611L
#     error "__cpp_lib_to_chars should have the value 201611L in c++20"
#   endif
# else // _LIBCPP_VERSION
#   ifdef __cpp_lib_to_chars
#     error "__cpp_lib_to_chars should not be defined because it is unimplemented in libc++!"
#   endif
# endif

#elif TEST_STD_VER == 23

# ifndef __cpp_lib_constexpr_charconv
#   error "__cpp_lib_constexpr_charconv should be defined in c++23"
# endif
# if __cpp_lib_constexpr_charconv != 202207L
#   error "__cpp_lib_constexpr_charconv should have the value 202207L in c++23"
# endif

# if !defined(_LIBCPP_VERSION)
#   ifndef __cpp_lib_to_chars
#     error "__cpp_lib_to_chars should be defined in c++23"
#   endif
#   if __cpp_lib_to_chars != 201611L
#     error "__cpp_lib_to_chars should have the value 201611L in c++23"
#   endif
# else // _LIBCPP_VERSION
#   ifdef __cpp_lib_to_chars
#     error "__cpp_lib_to_chars should not be defined because it is unimplemented in libc++!"
#   endif
# endif

#elif TEST_STD_VER > 23

# ifndef __cpp_lib_constexpr_charconv
#   error "__cpp_lib_constexpr_charconv should be defined in c++26"
# endif
# if __cpp_lib_constexpr_charconv != 202207L
#   error "__cpp_lib_constexpr_charconv should have the value 202207L in c++26"
# endif

# if !defined(_LIBCPP_VERSION)
#   ifndef __cpp_lib_to_chars
#     error "__cpp_lib_to_chars should be defined in c++26"
#   endif
#   if __cpp_lib_to_chars != 202306L
#     error "__cpp_lib_to_chars should have the value 202306L in c++26"
#   endif
# else // _LIBCPP_VERSION
#   ifdef __cpp_lib_to_chars
#     error "__cpp_lib_to_chars should not be defined because it is unimplemented in libc++!"
#   endif
# endif

#endif // TEST_STD_VER > 23

