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

// <unordered_set>

// Test the feature test macros defined by <unordered_set>

/*  Constant                                       Value
    __cpp_lib_allocator_traits_is_always_equal     201411L [C++17]
    __cpp_lib_associative_heterogeneous_erasure    202110L [C++2b]
    __cpp_lib_erase_if                             202002L [C++20]
    __cpp_lib_generic_unordered_lookup             201811L [C++20]
    __cpp_lib_node_extract                         201606L [C++17]
    __cpp_lib_nonmember_container_access           201411L [C++17]
    __cpp_lib_ranges_to_container                  202202L [C++2b]
*/

#include <unordered_set>
#include "test_macros.h"

#if TEST_STD_VER < 14

# ifdef __cpp_lib_allocator_traits_is_always_equal
#   error "__cpp_lib_allocator_traits_is_always_equal should not be defined before c++17"
# endif

# ifdef __cpp_lib_associative_heterogeneous_erasure
#   error "__cpp_lib_associative_heterogeneous_erasure should not be defined before c++2b"
# endif

# ifdef __cpp_lib_erase_if
#   error "__cpp_lib_erase_if should not be defined before c++20"
# endif

# ifdef __cpp_lib_generic_unordered_lookup
#   error "__cpp_lib_generic_unordered_lookup should not be defined before c++20"
# endif

# ifdef __cpp_lib_node_extract
#   error "__cpp_lib_node_extract should not be defined before c++17"
# endif

# ifdef __cpp_lib_nonmember_container_access
#   error "__cpp_lib_nonmember_container_access should not be defined before c++17"
# endif

# ifdef __cpp_lib_ranges_to_container
#   error "__cpp_lib_ranges_to_container should not be defined before c++2b"
# endif

#elif TEST_STD_VER == 14

# ifdef __cpp_lib_allocator_traits_is_always_equal
#   error "__cpp_lib_allocator_traits_is_always_equal should not be defined before c++17"
# endif

# ifdef __cpp_lib_associative_heterogeneous_erasure
#   error "__cpp_lib_associative_heterogeneous_erasure should not be defined before c++2b"
# endif

# ifdef __cpp_lib_erase_if
#   error "__cpp_lib_erase_if should not be defined before c++20"
# endif

# ifdef __cpp_lib_generic_unordered_lookup
#   error "__cpp_lib_generic_unordered_lookup should not be defined before c++20"
# endif

# ifdef __cpp_lib_node_extract
#   error "__cpp_lib_node_extract should not be defined before c++17"
# endif

# ifdef __cpp_lib_nonmember_container_access
#   error "__cpp_lib_nonmember_container_access should not be defined before c++17"
# endif

# ifdef __cpp_lib_ranges_to_container
#   error "__cpp_lib_ranges_to_container should not be defined before c++2b"
# endif

#elif TEST_STD_VER == 17

# ifndef __cpp_lib_allocator_traits_is_always_equal
#   error "__cpp_lib_allocator_traits_is_always_equal should be defined in c++17"
# endif
# if __cpp_lib_allocator_traits_is_always_equal != 201411L
#   error "__cpp_lib_allocator_traits_is_always_equal should have the value 201411L in c++17"
# endif

# ifdef __cpp_lib_associative_heterogeneous_erasure
#   error "__cpp_lib_associative_heterogeneous_erasure should not be defined before c++2b"
# endif

# ifdef __cpp_lib_erase_if
#   error "__cpp_lib_erase_if should not be defined before c++20"
# endif

# ifdef __cpp_lib_generic_unordered_lookup
#   error "__cpp_lib_generic_unordered_lookup should not be defined before c++20"
# endif

# ifndef __cpp_lib_node_extract
#   error "__cpp_lib_node_extract should be defined in c++17"
# endif
# if __cpp_lib_node_extract != 201606L
#   error "__cpp_lib_node_extract should have the value 201606L in c++17"
# endif

# ifndef __cpp_lib_nonmember_container_access
#   error "__cpp_lib_nonmember_container_access should be defined in c++17"
# endif
# if __cpp_lib_nonmember_container_access != 201411L
#   error "__cpp_lib_nonmember_container_access should have the value 201411L in c++17"
# endif

# ifdef __cpp_lib_ranges_to_container
#   error "__cpp_lib_ranges_to_container should not be defined before c++2b"
# endif

#elif TEST_STD_VER == 20

# ifndef __cpp_lib_allocator_traits_is_always_equal
#   error "__cpp_lib_allocator_traits_is_always_equal should be defined in c++20"
# endif
# if __cpp_lib_allocator_traits_is_always_equal != 201411L
#   error "__cpp_lib_allocator_traits_is_always_equal should have the value 201411L in c++20"
# endif

# ifdef __cpp_lib_associative_heterogeneous_erasure
#   error "__cpp_lib_associative_heterogeneous_erasure should not be defined before c++2b"
# endif

# ifndef __cpp_lib_erase_if
#   error "__cpp_lib_erase_if should be defined in c++20"
# endif
# if __cpp_lib_erase_if != 202002L
#   error "__cpp_lib_erase_if should have the value 202002L in c++20"
# endif

# ifndef __cpp_lib_generic_unordered_lookup
#   error "__cpp_lib_generic_unordered_lookup should be defined in c++20"
# endif
# if __cpp_lib_generic_unordered_lookup != 201811L
#   error "__cpp_lib_generic_unordered_lookup should have the value 201811L in c++20"
# endif

# ifndef __cpp_lib_node_extract
#   error "__cpp_lib_node_extract should be defined in c++20"
# endif
# if __cpp_lib_node_extract != 201606L
#   error "__cpp_lib_node_extract should have the value 201606L in c++20"
# endif

# ifndef __cpp_lib_nonmember_container_access
#   error "__cpp_lib_nonmember_container_access should be defined in c++20"
# endif
# if __cpp_lib_nonmember_container_access != 201411L
#   error "__cpp_lib_nonmember_container_access should have the value 201411L in c++20"
# endif

# ifdef __cpp_lib_ranges_to_container
#   error "__cpp_lib_ranges_to_container should not be defined before c++2b"
# endif

#elif TEST_STD_VER > 20

# ifndef __cpp_lib_allocator_traits_is_always_equal
#   error "__cpp_lib_allocator_traits_is_always_equal should be defined in c++2b"
# endif
# if __cpp_lib_allocator_traits_is_always_equal != 201411L
#   error "__cpp_lib_allocator_traits_is_always_equal should have the value 201411L in c++2b"
# endif

# if !defined(_LIBCPP_VERSION)
#   ifndef __cpp_lib_associative_heterogeneous_erasure
#     error "__cpp_lib_associative_heterogeneous_erasure should be defined in c++2b"
#   endif
#   if __cpp_lib_associative_heterogeneous_erasure != 202110L
#     error "__cpp_lib_associative_heterogeneous_erasure should have the value 202110L in c++2b"
#   endif
# else // _LIBCPP_VERSION
#   ifdef __cpp_lib_associative_heterogeneous_erasure
#     error "__cpp_lib_associative_heterogeneous_erasure should not be defined because it is unimplemented in libc++!"
#   endif
# endif

# ifndef __cpp_lib_erase_if
#   error "__cpp_lib_erase_if should be defined in c++2b"
# endif
# if __cpp_lib_erase_if != 202002L
#   error "__cpp_lib_erase_if should have the value 202002L in c++2b"
# endif

# ifndef __cpp_lib_generic_unordered_lookup
#   error "__cpp_lib_generic_unordered_lookup should be defined in c++2b"
# endif
# if __cpp_lib_generic_unordered_lookup != 201811L
#   error "__cpp_lib_generic_unordered_lookup should have the value 201811L in c++2b"
# endif

# ifndef __cpp_lib_node_extract
#   error "__cpp_lib_node_extract should be defined in c++2b"
# endif
# if __cpp_lib_node_extract != 201606L
#   error "__cpp_lib_node_extract should have the value 201606L in c++2b"
# endif

# ifndef __cpp_lib_nonmember_container_access
#   error "__cpp_lib_nonmember_container_access should be defined in c++2b"
# endif
# if __cpp_lib_nonmember_container_access != 201411L
#   error "__cpp_lib_nonmember_container_access should have the value 201411L in c++2b"
# endif

# if !defined(_LIBCPP_VERSION)
#   ifndef __cpp_lib_ranges_to_container
#     error "__cpp_lib_ranges_to_container should be defined in c++2b"
#   endif
#   if __cpp_lib_ranges_to_container != 202202L
#     error "__cpp_lib_ranges_to_container should have the value 202202L in c++2b"
#   endif
# else // _LIBCPP_VERSION
#   ifdef __cpp_lib_ranges_to_container
#     error "__cpp_lib_ranges_to_container should not be defined because it is unimplemented in libc++!"
#   endif
# endif

#endif // TEST_STD_VER > 20

