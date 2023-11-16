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

// <utility>

// Test the feature test macros defined by <utility>

/*  Constant                                  Value
    __cpp_lib_as_const                        201510L [C++17]
    __cpp_lib_constexpr_algorithms            201806L [C++20]
    __cpp_lib_constexpr_utility               201811L [C++20]
    __cpp_lib_exchange_function               201304L [C++14]
    __cpp_lib_forward_like                    202207L [C++23]
    __cpp_lib_integer_comparison_functions    202002L [C++20]
    __cpp_lib_integer_sequence                201304L [C++14]
    __cpp_lib_ranges_zip                      202110L [C++23]
    __cpp_lib_to_underlying                   202102L [C++23]
    __cpp_lib_tuples_by_type                  201304L [C++14]
    __cpp_lib_unreachable                     202202L [C++23]
*/

#include <utility>
#include "test_macros.h"

#if TEST_STD_VER < 14

# ifdef __cpp_lib_as_const
#   error "__cpp_lib_as_const should not be defined before c++17"
# endif

# ifdef __cpp_lib_constexpr_algorithms
#   error "__cpp_lib_constexpr_algorithms should not be defined before c++20"
# endif

# ifdef __cpp_lib_constexpr_utility
#   error "__cpp_lib_constexpr_utility should not be defined before c++20"
# endif

# ifdef __cpp_lib_exchange_function
#   error "__cpp_lib_exchange_function should not be defined before c++14"
# endif

# ifdef __cpp_lib_forward_like
#   error "__cpp_lib_forward_like should not be defined before c++23"
# endif

# ifdef __cpp_lib_integer_comparison_functions
#   error "__cpp_lib_integer_comparison_functions should not be defined before c++20"
# endif

# ifdef __cpp_lib_integer_sequence
#   error "__cpp_lib_integer_sequence should not be defined before c++14"
# endif

# ifdef __cpp_lib_ranges_zip
#   error "__cpp_lib_ranges_zip should not be defined before c++23"
# endif

# ifdef __cpp_lib_to_underlying
#   error "__cpp_lib_to_underlying should not be defined before c++23"
# endif

# ifdef __cpp_lib_tuples_by_type
#   error "__cpp_lib_tuples_by_type should not be defined before c++14"
# endif

# ifdef __cpp_lib_unreachable
#   error "__cpp_lib_unreachable should not be defined before c++23"
# endif

#elif TEST_STD_VER == 14

# ifdef __cpp_lib_as_const
#   error "__cpp_lib_as_const should not be defined before c++17"
# endif

# ifdef __cpp_lib_constexpr_algorithms
#   error "__cpp_lib_constexpr_algorithms should not be defined before c++20"
# endif

# ifdef __cpp_lib_constexpr_utility
#   error "__cpp_lib_constexpr_utility should not be defined before c++20"
# endif

# ifndef __cpp_lib_exchange_function
#   error "__cpp_lib_exchange_function should be defined in c++14"
# endif
# if __cpp_lib_exchange_function != 201304L
#   error "__cpp_lib_exchange_function should have the value 201304L in c++14"
# endif

# ifdef __cpp_lib_forward_like
#   error "__cpp_lib_forward_like should not be defined before c++23"
# endif

# ifdef __cpp_lib_integer_comparison_functions
#   error "__cpp_lib_integer_comparison_functions should not be defined before c++20"
# endif

# ifndef __cpp_lib_integer_sequence
#   error "__cpp_lib_integer_sequence should be defined in c++14"
# endif
# if __cpp_lib_integer_sequence != 201304L
#   error "__cpp_lib_integer_sequence should have the value 201304L in c++14"
# endif

# ifdef __cpp_lib_ranges_zip
#   error "__cpp_lib_ranges_zip should not be defined before c++23"
# endif

# ifdef __cpp_lib_to_underlying
#   error "__cpp_lib_to_underlying should not be defined before c++23"
# endif

# ifndef __cpp_lib_tuples_by_type
#   error "__cpp_lib_tuples_by_type should be defined in c++14"
# endif
# if __cpp_lib_tuples_by_type != 201304L
#   error "__cpp_lib_tuples_by_type should have the value 201304L in c++14"
# endif

# ifdef __cpp_lib_unreachable
#   error "__cpp_lib_unreachable should not be defined before c++23"
# endif

#elif TEST_STD_VER == 17

# ifndef __cpp_lib_as_const
#   error "__cpp_lib_as_const should be defined in c++17"
# endif
# if __cpp_lib_as_const != 201510L
#   error "__cpp_lib_as_const should have the value 201510L in c++17"
# endif

# ifdef __cpp_lib_constexpr_algorithms
#   error "__cpp_lib_constexpr_algorithms should not be defined before c++20"
# endif

# ifdef __cpp_lib_constexpr_utility
#   error "__cpp_lib_constexpr_utility should not be defined before c++20"
# endif

# ifndef __cpp_lib_exchange_function
#   error "__cpp_lib_exchange_function should be defined in c++17"
# endif
# if __cpp_lib_exchange_function != 201304L
#   error "__cpp_lib_exchange_function should have the value 201304L in c++17"
# endif

# ifdef __cpp_lib_forward_like
#   error "__cpp_lib_forward_like should not be defined before c++23"
# endif

# ifdef __cpp_lib_integer_comparison_functions
#   error "__cpp_lib_integer_comparison_functions should not be defined before c++20"
# endif

# ifndef __cpp_lib_integer_sequence
#   error "__cpp_lib_integer_sequence should be defined in c++17"
# endif
# if __cpp_lib_integer_sequence != 201304L
#   error "__cpp_lib_integer_sequence should have the value 201304L in c++17"
# endif

# ifdef __cpp_lib_ranges_zip
#   error "__cpp_lib_ranges_zip should not be defined before c++23"
# endif

# ifdef __cpp_lib_to_underlying
#   error "__cpp_lib_to_underlying should not be defined before c++23"
# endif

# ifndef __cpp_lib_tuples_by_type
#   error "__cpp_lib_tuples_by_type should be defined in c++17"
# endif
# if __cpp_lib_tuples_by_type != 201304L
#   error "__cpp_lib_tuples_by_type should have the value 201304L in c++17"
# endif

# ifdef __cpp_lib_unreachable
#   error "__cpp_lib_unreachable should not be defined before c++23"
# endif

#elif TEST_STD_VER == 20

# ifndef __cpp_lib_as_const
#   error "__cpp_lib_as_const should be defined in c++20"
# endif
# if __cpp_lib_as_const != 201510L
#   error "__cpp_lib_as_const should have the value 201510L in c++20"
# endif

# ifndef __cpp_lib_constexpr_algorithms
#   error "__cpp_lib_constexpr_algorithms should be defined in c++20"
# endif
# if __cpp_lib_constexpr_algorithms != 201806L
#   error "__cpp_lib_constexpr_algorithms should have the value 201806L in c++20"
# endif

# ifndef __cpp_lib_constexpr_utility
#   error "__cpp_lib_constexpr_utility should be defined in c++20"
# endif
# if __cpp_lib_constexpr_utility != 201811L
#   error "__cpp_lib_constexpr_utility should have the value 201811L in c++20"
# endif

# ifndef __cpp_lib_exchange_function
#   error "__cpp_lib_exchange_function should be defined in c++20"
# endif
# if __cpp_lib_exchange_function != 201304L
#   error "__cpp_lib_exchange_function should have the value 201304L in c++20"
# endif

# ifdef __cpp_lib_forward_like
#   error "__cpp_lib_forward_like should not be defined before c++23"
# endif

# ifndef __cpp_lib_integer_comparison_functions
#   error "__cpp_lib_integer_comparison_functions should be defined in c++20"
# endif
# if __cpp_lib_integer_comparison_functions != 202002L
#   error "__cpp_lib_integer_comparison_functions should have the value 202002L in c++20"
# endif

# ifndef __cpp_lib_integer_sequence
#   error "__cpp_lib_integer_sequence should be defined in c++20"
# endif
# if __cpp_lib_integer_sequence != 201304L
#   error "__cpp_lib_integer_sequence should have the value 201304L in c++20"
# endif

# ifdef __cpp_lib_ranges_zip
#   error "__cpp_lib_ranges_zip should not be defined before c++23"
# endif

# ifdef __cpp_lib_to_underlying
#   error "__cpp_lib_to_underlying should not be defined before c++23"
# endif

# ifndef __cpp_lib_tuples_by_type
#   error "__cpp_lib_tuples_by_type should be defined in c++20"
# endif
# if __cpp_lib_tuples_by_type != 201304L
#   error "__cpp_lib_tuples_by_type should have the value 201304L in c++20"
# endif

# ifdef __cpp_lib_unreachable
#   error "__cpp_lib_unreachable should not be defined before c++23"
# endif

#elif TEST_STD_VER == 23

# ifndef __cpp_lib_as_const
#   error "__cpp_lib_as_const should be defined in c++23"
# endif
# if __cpp_lib_as_const != 201510L
#   error "__cpp_lib_as_const should have the value 201510L in c++23"
# endif

# ifndef __cpp_lib_constexpr_algorithms
#   error "__cpp_lib_constexpr_algorithms should be defined in c++23"
# endif
# if __cpp_lib_constexpr_algorithms != 201806L
#   error "__cpp_lib_constexpr_algorithms should have the value 201806L in c++23"
# endif

# ifndef __cpp_lib_constexpr_utility
#   error "__cpp_lib_constexpr_utility should be defined in c++23"
# endif
# if __cpp_lib_constexpr_utility != 201811L
#   error "__cpp_lib_constexpr_utility should have the value 201811L in c++23"
# endif

# ifndef __cpp_lib_exchange_function
#   error "__cpp_lib_exchange_function should be defined in c++23"
# endif
# if __cpp_lib_exchange_function != 201304L
#   error "__cpp_lib_exchange_function should have the value 201304L in c++23"
# endif

# ifndef __cpp_lib_forward_like
#   error "__cpp_lib_forward_like should be defined in c++23"
# endif
# if __cpp_lib_forward_like != 202207L
#   error "__cpp_lib_forward_like should have the value 202207L in c++23"
# endif

# ifndef __cpp_lib_integer_comparison_functions
#   error "__cpp_lib_integer_comparison_functions should be defined in c++23"
# endif
# if __cpp_lib_integer_comparison_functions != 202002L
#   error "__cpp_lib_integer_comparison_functions should have the value 202002L in c++23"
# endif

# ifndef __cpp_lib_integer_sequence
#   error "__cpp_lib_integer_sequence should be defined in c++23"
# endif
# if __cpp_lib_integer_sequence != 201304L
#   error "__cpp_lib_integer_sequence should have the value 201304L in c++23"
# endif

# if !defined(_LIBCPP_VERSION)
#   ifndef __cpp_lib_ranges_zip
#     error "__cpp_lib_ranges_zip should be defined in c++23"
#   endif
#   if __cpp_lib_ranges_zip != 202110L
#     error "__cpp_lib_ranges_zip should have the value 202110L in c++23"
#   endif
# else // _LIBCPP_VERSION
#   ifdef __cpp_lib_ranges_zip
#     error "__cpp_lib_ranges_zip should not be defined because it is unimplemented in libc++!"
#   endif
# endif

# ifndef __cpp_lib_to_underlying
#   error "__cpp_lib_to_underlying should be defined in c++23"
# endif
# if __cpp_lib_to_underlying != 202102L
#   error "__cpp_lib_to_underlying should have the value 202102L in c++23"
# endif

# ifndef __cpp_lib_tuples_by_type
#   error "__cpp_lib_tuples_by_type should be defined in c++23"
# endif
# if __cpp_lib_tuples_by_type != 201304L
#   error "__cpp_lib_tuples_by_type should have the value 201304L in c++23"
# endif

# ifndef __cpp_lib_unreachable
#   error "__cpp_lib_unreachable should be defined in c++23"
# endif
# if __cpp_lib_unreachable != 202202L
#   error "__cpp_lib_unreachable should have the value 202202L in c++23"
# endif

#elif TEST_STD_VER > 23

# ifndef __cpp_lib_as_const
#   error "__cpp_lib_as_const should be defined in c++26"
# endif
# if __cpp_lib_as_const != 201510L
#   error "__cpp_lib_as_const should have the value 201510L in c++26"
# endif

# ifndef __cpp_lib_constexpr_algorithms
#   error "__cpp_lib_constexpr_algorithms should be defined in c++26"
# endif
# if __cpp_lib_constexpr_algorithms != 201806L
#   error "__cpp_lib_constexpr_algorithms should have the value 201806L in c++26"
# endif

# ifndef __cpp_lib_constexpr_utility
#   error "__cpp_lib_constexpr_utility should be defined in c++26"
# endif
# if __cpp_lib_constexpr_utility != 201811L
#   error "__cpp_lib_constexpr_utility should have the value 201811L in c++26"
# endif

# ifndef __cpp_lib_exchange_function
#   error "__cpp_lib_exchange_function should be defined in c++26"
# endif
# if __cpp_lib_exchange_function != 201304L
#   error "__cpp_lib_exchange_function should have the value 201304L in c++26"
# endif

# ifndef __cpp_lib_forward_like
#   error "__cpp_lib_forward_like should be defined in c++26"
# endif
# if __cpp_lib_forward_like != 202207L
#   error "__cpp_lib_forward_like should have the value 202207L in c++26"
# endif

# ifndef __cpp_lib_integer_comparison_functions
#   error "__cpp_lib_integer_comparison_functions should be defined in c++26"
# endif
# if __cpp_lib_integer_comparison_functions != 202002L
#   error "__cpp_lib_integer_comparison_functions should have the value 202002L in c++26"
# endif

# ifndef __cpp_lib_integer_sequence
#   error "__cpp_lib_integer_sequence should be defined in c++26"
# endif
# if __cpp_lib_integer_sequence != 201304L
#   error "__cpp_lib_integer_sequence should have the value 201304L in c++26"
# endif

# if !defined(_LIBCPP_VERSION)
#   ifndef __cpp_lib_ranges_zip
#     error "__cpp_lib_ranges_zip should be defined in c++26"
#   endif
#   if __cpp_lib_ranges_zip != 202110L
#     error "__cpp_lib_ranges_zip should have the value 202110L in c++26"
#   endif
# else // _LIBCPP_VERSION
#   ifdef __cpp_lib_ranges_zip
#     error "__cpp_lib_ranges_zip should not be defined because it is unimplemented in libc++!"
#   endif
# endif

# ifndef __cpp_lib_to_underlying
#   error "__cpp_lib_to_underlying should be defined in c++26"
# endif
# if __cpp_lib_to_underlying != 202102L
#   error "__cpp_lib_to_underlying should have the value 202102L in c++26"
# endif

# ifndef __cpp_lib_tuples_by_type
#   error "__cpp_lib_tuples_by_type should be defined in c++26"
# endif
# if __cpp_lib_tuples_by_type != 201304L
#   error "__cpp_lib_tuples_by_type should have the value 201304L in c++26"
# endif

# ifndef __cpp_lib_unreachable
#   error "__cpp_lib_unreachable should be defined in c++26"
# endif
# if __cpp_lib_unreachable != 202202L
#   error "__cpp_lib_unreachable should have the value 202202L in c++26"
# endif

#endif // TEST_STD_VER > 23

