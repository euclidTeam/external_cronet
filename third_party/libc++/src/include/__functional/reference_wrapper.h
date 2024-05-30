// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___FUNCTIONAL_REFERENCE_WRAPPER_H
#define _LIBCPP___FUNCTIONAL_REFERENCE_WRAPPER_H

#include <__config>
#include <__functional/invoke.h>
#include <__functional/weak_result_type.h>
#include <__memory/addressof.h>
#include <__type_traits/enable_if.h>
#include <__type_traits/remove_cvref.h>
#include <__utility/declval.h>
#include <__utility/forward.h>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

_LIBCPP_BEGIN_NAMESPACE_STD

template <class _Tp>
class _LIBCPP_TEMPLATE_VIS reference_wrapper : public __weak_result_type<_Tp> {
public:
  // types
  typedef _Tp type;

private:
  type* __f_;

  static void __fun(_Tp&) _NOEXCEPT;
  static void __fun(_Tp&&) = delete;

public:
  template <
      class _Up,
      class = __enable_if_t<!__is_same_uncvref<_Up, reference_wrapper>::value, decltype(__fun(std::declval<_Up>())) > >
  _LIBCPP_HIDE_FROM_ABI _LIBCPP_CONSTEXPR_SINCE_CXX20 reference_wrapper(_Up&& __u)
      _NOEXCEPT_(noexcept(__fun(std::declval<_Up>()))) {
    type& __f = static_cast<_Up&&>(__u);
    __f_      = std::addressof(__f);
  }

  // access
  _LIBCPP_HIDE_FROM_ABI _LIBCPP_CONSTEXPR_SINCE_CXX20 operator type&() const _NOEXCEPT { return *__f_; }
  _LIBCPP_HIDE_FROM_ABI _LIBCPP_CONSTEXPR_SINCE_CXX20 type& get() const _NOEXCEPT { return *__f_; }

  // invoke
  template <class... _ArgTypes>
  _LIBCPP_HIDE_FROM_ABI _LIBCPP_CONSTEXPR_SINCE_CXX20 typename __invoke_of<type&, _ArgTypes...>::type
  operator()(_ArgTypes&&... __args) const
#if _LIBCPP_STD_VER >= 17
      // Since is_nothrow_invocable requires C++17 LWG3764 is not backported
      // to earlier versions.
      noexcept(is_nothrow_invocable_v<_Tp&, _ArgTypes...>)
#endif
  {
    return std::__invoke(get(), std::forward<_ArgTypes>(__args)...);
  }
};

#if _LIBCPP_STD_VER >= 17
template <class _Tp>
reference_wrapper(_Tp&) -> reference_wrapper<_Tp>;
#endif

template <class _Tp>
inline _LIBCPP_HIDE_FROM_ABI _LIBCPP_CONSTEXPR_SINCE_CXX20 reference_wrapper<_Tp> ref(_Tp& __t) _NOEXCEPT {
  return reference_wrapper<_Tp>(__t);
}

template <class _Tp>
inline _LIBCPP_HIDE_FROM_ABI _LIBCPP_CONSTEXPR_SINCE_CXX20 reference_wrapper<_Tp>
ref(reference_wrapper<_Tp> __t) _NOEXCEPT {
  return __t;
}

template <class _Tp>
inline _LIBCPP_HIDE_FROM_ABI _LIBCPP_CONSTEXPR_SINCE_CXX20 reference_wrapper<const _Tp> cref(const _Tp& __t) _NOEXCEPT {
  return reference_wrapper<const _Tp>(__t);
}

template <class _Tp>
inline _LIBCPP_HIDE_FROM_ABI _LIBCPP_CONSTEXPR_SINCE_CXX20 reference_wrapper<const _Tp>
cref(reference_wrapper<_Tp> __t) _NOEXCEPT {
  return __t;
}

template <class _Tp>
void ref(const _Tp&&) = delete;
template <class _Tp>
void cref(const _Tp&&) = delete;

_LIBCPP_END_NAMESPACE_STD

#endif // _LIBCPP___FUNCTIONAL_REFERENCE_WRAPPER_H
