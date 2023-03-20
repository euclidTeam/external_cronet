//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___TYPE_TRAITS_IS_TRIVIALLY_COPY_ASSIGNABLE_H
#define _LIBCPP___TYPE_TRAITS_IS_TRIVIALLY_COPY_ASSIGNABLE_H

#include <__config>
#include <__type_traits/add_const.h>
#include <__type_traits/add_lvalue_reference.h>
#include <__type_traits/integral_constant.h>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

_LIBCPP_BEGIN_NAMESPACE_STD

template <class _Tp>
struct _LIBCPP_TEMPLATE_VIS is_trivially_copy_assignable
    : public integral_constant<
          bool,
          __is_trivially_assignable(
              __add_lvalue_reference_t<_Tp>,
              __add_lvalue_reference_t<typename add_const<_Tp>::type>)> {};

#if _LIBCPP_STD_VER > 14
template <class _Tp>
inline constexpr bool is_trivially_copy_assignable_v = is_trivially_copy_assignable<_Tp>::value;
#endif

_LIBCPP_END_NAMESPACE_STD

#endif // _LIBCPP___TYPE_TRAITS_IS_TRIVIALLY_COPY_ASSIGNABLE_H
