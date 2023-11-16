// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___FUNCTIONAL_OPERATIONS_H
#define _LIBCPP___FUNCTIONAL_OPERATIONS_H

#include <__config>
#include <__functional/binary_function.h>
#include <__functional/unary_function.h>
#include <__type_traits/integral_constant.h>
#include <__type_traits/operation_traits.h>
#include <__type_traits/predicate_traits.h>
#include <__utility/forward.h>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

_LIBCPP_BEGIN_NAMESPACE_STD

// Arithmetic operations

#if _LIBCPP_STD_VER >= 14
template <class _Tp = void>
#else
template <class _Tp>
#endif
struct _LIBCPP_TEMPLATE_VIS plus
    : __binary_function<_Tp, _Tp, _Tp>
{
    typedef _Tp __result_type;  // used by valarray
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    _Tp operator()(const _Tp& __x, const _Tp& __y) const
        {return __x + __y;}
};
_LIBCPP_CTAD_SUPPORTED_FOR_TYPE(plus);

template <class _Tp>
struct __is_trivial_plus_operation<plus<_Tp>, _Tp, _Tp> : true_type {};

#if _LIBCPP_STD_VER >= 14
template <class _Tp, class _Up>
struct __is_trivial_plus_operation<plus<>, _Tp, _Up> : true_type {};
#endif

#if _LIBCPP_STD_VER >= 14
template <>
struct _LIBCPP_TEMPLATE_VIS plus<void>
{
    template <class _T1, class _T2>
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    auto operator()(_T1&& __t, _T2&& __u) const
        noexcept(noexcept(_VSTD::forward<_T1>(__t) + _VSTD::forward<_T2>(__u)))
        -> decltype(      _VSTD::forward<_T1>(__t) + _VSTD::forward<_T2>(__u))
        { return          _VSTD::forward<_T1>(__t) + _VSTD::forward<_T2>(__u); }
    typedef void is_transparent;
};
#endif

#if _LIBCPP_STD_VER >= 14
template <class _Tp = void>
#else
template <class _Tp>
#endif
struct _LIBCPP_TEMPLATE_VIS minus
    : __binary_function<_Tp, _Tp, _Tp>
{
    typedef _Tp __result_type;  // used by valarray
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    _Tp operator()(const _Tp& __x, const _Tp& __y) const
        {return __x - __y;}
};
_LIBCPP_CTAD_SUPPORTED_FOR_TYPE(minus);

#if _LIBCPP_STD_VER >= 14
template <>
struct _LIBCPP_TEMPLATE_VIS minus<void>
{
    template <class _T1, class _T2>
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    auto operator()(_T1&& __t, _T2&& __u) const
        noexcept(noexcept(_VSTD::forward<_T1>(__t) - _VSTD::forward<_T2>(__u)))
        -> decltype(      _VSTD::forward<_T1>(__t) - _VSTD::forward<_T2>(__u))
        { return          _VSTD::forward<_T1>(__t) - _VSTD::forward<_T2>(__u); }
    typedef void is_transparent;
};
#endif

#if _LIBCPP_STD_VER >= 14
template <class _Tp = void>
#else
template <class _Tp>
#endif
struct _LIBCPP_TEMPLATE_VIS multiplies
    : __binary_function<_Tp, _Tp, _Tp>
{
    typedef _Tp __result_type;  // used by valarray
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    _Tp operator()(const _Tp& __x, const _Tp& __y) const
        {return __x * __y;}
};
_LIBCPP_CTAD_SUPPORTED_FOR_TYPE(multiplies);

#if _LIBCPP_STD_VER >= 14
template <>
struct _LIBCPP_TEMPLATE_VIS multiplies<void>
{
    template <class _T1, class _T2>
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    auto operator()(_T1&& __t, _T2&& __u) const
        noexcept(noexcept(_VSTD::forward<_T1>(__t) * _VSTD::forward<_T2>(__u)))
        -> decltype(      _VSTD::forward<_T1>(__t) * _VSTD::forward<_T2>(__u))
        { return          _VSTD::forward<_T1>(__t) * _VSTD::forward<_T2>(__u); }
    typedef void is_transparent;
};
#endif

#if _LIBCPP_STD_VER >= 14
template <class _Tp = void>
#else
template <class _Tp>
#endif
struct _LIBCPP_TEMPLATE_VIS divides
    : __binary_function<_Tp, _Tp, _Tp>
{
    typedef _Tp __result_type;  // used by valarray
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    _Tp operator()(const _Tp& __x, const _Tp& __y) const
        {return __x / __y;}
};
_LIBCPP_CTAD_SUPPORTED_FOR_TYPE(divides);

#if _LIBCPP_STD_VER >= 14
template <>
struct _LIBCPP_TEMPLATE_VIS divides<void>
{
    template <class _T1, class _T2>
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    auto operator()(_T1&& __t, _T2&& __u) const
        noexcept(noexcept(_VSTD::forward<_T1>(__t) / _VSTD::forward<_T2>(__u)))
        -> decltype(      _VSTD::forward<_T1>(__t) / _VSTD::forward<_T2>(__u))
        { return          _VSTD::forward<_T1>(__t) / _VSTD::forward<_T2>(__u); }
    typedef void is_transparent;
};
#endif

#if _LIBCPP_STD_VER >= 14
template <class _Tp = void>
#else
template <class _Tp>
#endif
struct _LIBCPP_TEMPLATE_VIS modulus
    : __binary_function<_Tp, _Tp, _Tp>
{
    typedef _Tp __result_type;  // used by valarray
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    _Tp operator()(const _Tp& __x, const _Tp& __y) const
        {return __x % __y;}
};
_LIBCPP_CTAD_SUPPORTED_FOR_TYPE(modulus);

#if _LIBCPP_STD_VER >= 14
template <>
struct _LIBCPP_TEMPLATE_VIS modulus<void>
{
    template <class _T1, class _T2>
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    auto operator()(_T1&& __t, _T2&& __u) const
        noexcept(noexcept(_VSTD::forward<_T1>(__t) % _VSTD::forward<_T2>(__u)))
        -> decltype(      _VSTD::forward<_T1>(__t) % _VSTD::forward<_T2>(__u))
        { return          _VSTD::forward<_T1>(__t) % _VSTD::forward<_T2>(__u); }
    typedef void is_transparent;
};
#endif

#if _LIBCPP_STD_VER >= 14
template <class _Tp = void>
#else
template <class _Tp>
#endif
struct _LIBCPP_TEMPLATE_VIS negate
    : __unary_function<_Tp, _Tp>
{
    typedef _Tp __result_type;  // used by valarray
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    _Tp operator()(const _Tp& __x) const
        {return -__x;}
};
_LIBCPP_CTAD_SUPPORTED_FOR_TYPE(negate);

#if _LIBCPP_STD_VER >= 14
template <>
struct _LIBCPP_TEMPLATE_VIS negate<void>
{
    template <class _Tp>
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    auto operator()(_Tp&& __x) const
        noexcept(noexcept(- _VSTD::forward<_Tp>(__x)))
        -> decltype(      - _VSTD::forward<_Tp>(__x))
        { return          - _VSTD::forward<_Tp>(__x); }
    typedef void is_transparent;
};
#endif

// Bitwise operations

#if _LIBCPP_STD_VER >= 14
template <class _Tp = void>
#else
template <class _Tp>
#endif
struct _LIBCPP_TEMPLATE_VIS bit_and
    : __binary_function<_Tp, _Tp, _Tp>
{
    typedef _Tp __result_type;  // used by valarray
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    _Tp operator()(const _Tp& __x, const _Tp& __y) const
        {return __x & __y;}
};
_LIBCPP_CTAD_SUPPORTED_FOR_TYPE(bit_and);

#if _LIBCPP_STD_VER >= 14
template <>
struct _LIBCPP_TEMPLATE_VIS bit_and<void>
{
    template <class _T1, class _T2>
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    auto operator()(_T1&& __t, _T2&& __u) const
        noexcept(noexcept(_VSTD::forward<_T1>(__t) & _VSTD::forward<_T2>(__u)))
        -> decltype(      _VSTD::forward<_T1>(__t) & _VSTD::forward<_T2>(__u))
        { return          _VSTD::forward<_T1>(__t) & _VSTD::forward<_T2>(__u); }
    typedef void is_transparent;
};
#endif

#if _LIBCPP_STD_VER >= 14
template <class _Tp = void>
struct _LIBCPP_TEMPLATE_VIS bit_not
    : __unary_function<_Tp, _Tp>
{
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    _Tp operator()(const _Tp& __x) const
        {return ~__x;}
};
_LIBCPP_CTAD_SUPPORTED_FOR_TYPE(bit_not);

template <>
struct _LIBCPP_TEMPLATE_VIS bit_not<void>
{
    template <class _Tp>
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    auto operator()(_Tp&& __x) const
        noexcept(noexcept(~_VSTD::forward<_Tp>(__x)))
        -> decltype(      ~_VSTD::forward<_Tp>(__x))
        { return          ~_VSTD::forward<_Tp>(__x); }
    typedef void is_transparent;
};
#endif

#if _LIBCPP_STD_VER >= 14
template <class _Tp = void>
#else
template <class _Tp>
#endif
struct _LIBCPP_TEMPLATE_VIS bit_or
    : __binary_function<_Tp, _Tp, _Tp>
{
    typedef _Tp __result_type;  // used by valarray
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    _Tp operator()(const _Tp& __x, const _Tp& __y) const
        {return __x | __y;}
};
_LIBCPP_CTAD_SUPPORTED_FOR_TYPE(bit_or);

#if _LIBCPP_STD_VER >= 14
template <>
struct _LIBCPP_TEMPLATE_VIS bit_or<void>
{
    template <class _T1, class _T2>
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    auto operator()(_T1&& __t, _T2&& __u) const
        noexcept(noexcept(_VSTD::forward<_T1>(__t) | _VSTD::forward<_T2>(__u)))
        -> decltype(      _VSTD::forward<_T1>(__t) | _VSTD::forward<_T2>(__u))
        { return          _VSTD::forward<_T1>(__t) | _VSTD::forward<_T2>(__u); }
    typedef void is_transparent;
};
#endif

#if _LIBCPP_STD_VER >= 14
template <class _Tp = void>
#else
template <class _Tp>
#endif
struct _LIBCPP_TEMPLATE_VIS bit_xor
    : __binary_function<_Tp, _Tp, _Tp>
{
    typedef _Tp __result_type;  // used by valarray
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    _Tp operator()(const _Tp& __x, const _Tp& __y) const
        {return __x ^ __y;}
};
_LIBCPP_CTAD_SUPPORTED_FOR_TYPE(bit_xor);

#if _LIBCPP_STD_VER >= 14
template <>
struct _LIBCPP_TEMPLATE_VIS bit_xor<void>
{
    template <class _T1, class _T2>
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    auto operator()(_T1&& __t, _T2&& __u) const
        noexcept(noexcept(_VSTD::forward<_T1>(__t) ^ _VSTD::forward<_T2>(__u)))
        -> decltype(      _VSTD::forward<_T1>(__t) ^ _VSTD::forward<_T2>(__u))
        { return          _VSTD::forward<_T1>(__t) ^ _VSTD::forward<_T2>(__u); }
    typedef void is_transparent;
};
#endif

// Comparison operations

#if _LIBCPP_STD_VER >= 14
template <class _Tp = void>
#else
template <class _Tp>
#endif
struct _LIBCPP_TEMPLATE_VIS equal_to
    : __binary_function<_Tp, _Tp, bool>
{
    typedef bool __result_type;  // used by valarray
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    bool operator()(const _Tp& __x, const _Tp& __y) const
        {return __x == __y;}
};
_LIBCPP_CTAD_SUPPORTED_FOR_TYPE(equal_to);

#if _LIBCPP_STD_VER >= 14
template <>
struct _LIBCPP_TEMPLATE_VIS equal_to<void>
{
    template <class _T1, class _T2>
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    auto operator()(_T1&& __t, _T2&& __u) const
        noexcept(noexcept(_VSTD::forward<_T1>(__t) == _VSTD::forward<_T2>(__u)))
        -> decltype(      _VSTD::forward<_T1>(__t) == _VSTD::forward<_T2>(__u))
        { return          _VSTD::forward<_T1>(__t) == _VSTD::forward<_T2>(__u); }
    typedef void is_transparent;
};
#endif

template <class _Tp>
struct __is_trivial_equality_predicate<equal_to<_Tp>, _Tp, _Tp> : true_type {};

#if _LIBCPP_STD_VER >= 14
template <class _Tp>
struct __is_trivial_equality_predicate<equal_to<>, _Tp, _Tp> : true_type {};
#endif

#if _LIBCPP_STD_VER >= 14
template <class _Tp = void>
#else
template <class _Tp>
#endif
struct _LIBCPP_TEMPLATE_VIS not_equal_to
    : __binary_function<_Tp, _Tp, bool>
{
    typedef bool __result_type;  // used by valarray
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    bool operator()(const _Tp& __x, const _Tp& __y) const
        {return __x != __y;}
};
_LIBCPP_CTAD_SUPPORTED_FOR_TYPE(not_equal_to);

#if _LIBCPP_STD_VER >= 14
template <>
struct _LIBCPP_TEMPLATE_VIS not_equal_to<void>
{
    template <class _T1, class _T2>
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    auto operator()(_T1&& __t, _T2&& __u) const
        noexcept(noexcept(_VSTD::forward<_T1>(__t) != _VSTD::forward<_T2>(__u)))
        -> decltype(      _VSTD::forward<_T1>(__t) != _VSTD::forward<_T2>(__u))
        { return          _VSTD::forward<_T1>(__t) != _VSTD::forward<_T2>(__u); }
    typedef void is_transparent;
};
#endif

#if _LIBCPP_STD_VER >= 14
template <class _Tp = void>
#else
template <class _Tp>
#endif
struct _LIBCPP_TEMPLATE_VIS less
    : __binary_function<_Tp, _Tp, bool>
{
    typedef bool __result_type;  // used by valarray
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    bool operator()(const _Tp& __x, const _Tp& __y) const
        {return __x < __y;}
};
_LIBCPP_CTAD_SUPPORTED_FOR_TYPE(less);

#if _LIBCPP_STD_VER >= 14
template <>
struct _LIBCPP_TEMPLATE_VIS less<void>
{
    template <class _T1, class _T2>
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    auto operator()(_T1&& __t, _T2&& __u) const
        noexcept(noexcept(_VSTD::forward<_T1>(__t) < _VSTD::forward<_T2>(__u)))
        -> decltype(      _VSTD::forward<_T1>(__t) < _VSTD::forward<_T2>(__u))
        { return          _VSTD::forward<_T1>(__t) < _VSTD::forward<_T2>(__u); }
    typedef void is_transparent;
};
#endif

#if _LIBCPP_STD_VER >= 14
template <class _Tp = void>
#else
template <class _Tp>
#endif
struct _LIBCPP_TEMPLATE_VIS less_equal
    : __binary_function<_Tp, _Tp, bool>
{
    typedef bool __result_type;  // used by valarray
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    bool operator()(const _Tp& __x, const _Tp& __y) const
        {return __x <= __y;}
};
_LIBCPP_CTAD_SUPPORTED_FOR_TYPE(less_equal);

#if _LIBCPP_STD_VER >= 14
template <>
struct _LIBCPP_TEMPLATE_VIS less_equal<void>
{
    template <class _T1, class _T2>
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    auto operator()(_T1&& __t, _T2&& __u) const
        noexcept(noexcept(_VSTD::forward<_T1>(__t) <= _VSTD::forward<_T2>(__u)))
        -> decltype(      _VSTD::forward<_T1>(__t) <= _VSTD::forward<_T2>(__u))
        { return          _VSTD::forward<_T1>(__t) <= _VSTD::forward<_T2>(__u); }
    typedef void is_transparent;
};
#endif

#if _LIBCPP_STD_VER >= 14
template <class _Tp = void>
#else
template <class _Tp>
#endif
struct _LIBCPP_TEMPLATE_VIS greater_equal
    : __binary_function<_Tp, _Tp, bool>
{
    typedef bool __result_type;  // used by valarray
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    bool operator()(const _Tp& __x, const _Tp& __y) const
        {return __x >= __y;}
};
_LIBCPP_CTAD_SUPPORTED_FOR_TYPE(greater_equal);

#if _LIBCPP_STD_VER >= 14
template <>
struct _LIBCPP_TEMPLATE_VIS greater_equal<void>
{
    template <class _T1, class _T2>
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    auto operator()(_T1&& __t, _T2&& __u) const
        noexcept(noexcept(_VSTD::forward<_T1>(__t) >= _VSTD::forward<_T2>(__u)))
        -> decltype(      _VSTD::forward<_T1>(__t) >= _VSTD::forward<_T2>(__u))
        { return          _VSTD::forward<_T1>(__t) >= _VSTD::forward<_T2>(__u); }
    typedef void is_transparent;
};
#endif

#if _LIBCPP_STD_VER >= 14
template <class _Tp = void>
#else
template <class _Tp>
#endif
struct _LIBCPP_TEMPLATE_VIS greater
    : __binary_function<_Tp, _Tp, bool>
{
    typedef bool __result_type;  // used by valarray
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    bool operator()(const _Tp& __x, const _Tp& __y) const
        {return __x > __y;}
};
_LIBCPP_CTAD_SUPPORTED_FOR_TYPE(greater);

#if _LIBCPP_STD_VER >= 14
template <>
struct _LIBCPP_TEMPLATE_VIS greater<void>
{
    template <class _T1, class _T2>
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    auto operator()(_T1&& __t, _T2&& __u) const
        noexcept(noexcept(_VSTD::forward<_T1>(__t) > _VSTD::forward<_T2>(__u)))
        -> decltype(      _VSTD::forward<_T1>(__t) > _VSTD::forward<_T2>(__u))
        { return          _VSTD::forward<_T1>(__t) > _VSTD::forward<_T2>(__u); }
    typedef void is_transparent;
};
#endif

// Logical operations

#if _LIBCPP_STD_VER >= 14
template <class _Tp = void>
#else
template <class _Tp>
#endif
struct _LIBCPP_TEMPLATE_VIS logical_and
    : __binary_function<_Tp, _Tp, bool>
{
    typedef bool __result_type;  // used by valarray
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    bool operator()(const _Tp& __x, const _Tp& __y) const
        {return __x && __y;}
};
_LIBCPP_CTAD_SUPPORTED_FOR_TYPE(logical_and);

#if _LIBCPP_STD_VER >= 14
template <>
struct _LIBCPP_TEMPLATE_VIS logical_and<void>
{
    template <class _T1, class _T2>
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    auto operator()(_T1&& __t, _T2&& __u) const
        noexcept(noexcept(_VSTD::forward<_T1>(__t) && _VSTD::forward<_T2>(__u)))
        -> decltype(      _VSTD::forward<_T1>(__t) && _VSTD::forward<_T2>(__u))
        { return          _VSTD::forward<_T1>(__t) && _VSTD::forward<_T2>(__u); }
    typedef void is_transparent;
};
#endif

#if _LIBCPP_STD_VER >= 14
template <class _Tp = void>
#else
template <class _Tp>
#endif
struct _LIBCPP_TEMPLATE_VIS logical_not
    : __unary_function<_Tp, bool>
{
    typedef bool __result_type;  // used by valarray
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    bool operator()(const _Tp& __x) const
        {return !__x;}
};
_LIBCPP_CTAD_SUPPORTED_FOR_TYPE(logical_not);

#if _LIBCPP_STD_VER >= 14
template <>
struct _LIBCPP_TEMPLATE_VIS logical_not<void>
{
    template <class _Tp>
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    auto operator()(_Tp&& __x) const
        noexcept(noexcept(!_VSTD::forward<_Tp>(__x)))
        -> decltype(      !_VSTD::forward<_Tp>(__x))
        { return          !_VSTD::forward<_Tp>(__x); }
    typedef void is_transparent;
};
#endif

#if _LIBCPP_STD_VER >= 14
template <class _Tp = void>
#else
template <class _Tp>
#endif
struct _LIBCPP_TEMPLATE_VIS logical_or
    : __binary_function<_Tp, _Tp, bool>
{
    typedef bool __result_type;  // used by valarray
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    bool operator()(const _Tp& __x, const _Tp& __y) const
        {return __x || __y;}
};
_LIBCPP_CTAD_SUPPORTED_FOR_TYPE(logical_or);

#if _LIBCPP_STD_VER >= 14
template <>
struct _LIBCPP_TEMPLATE_VIS logical_or<void>
{
    template <class _T1, class _T2>
    _LIBCPP_CONSTEXPR_SINCE_CXX14 _LIBCPP_INLINE_VISIBILITY
    auto operator()(_T1&& __t, _T2&& __u) const
        noexcept(noexcept(_VSTD::forward<_T1>(__t) || _VSTD::forward<_T2>(__u)))
        -> decltype(      _VSTD::forward<_T1>(__t) || _VSTD::forward<_T2>(__u))
        { return          _VSTD::forward<_T1>(__t) || _VSTD::forward<_T2>(__u); }
    typedef void is_transparent;
};
#endif

_LIBCPP_END_NAMESPACE_STD

#endif // _LIBCPP___FUNCTIONAL_OPERATIONS_H
