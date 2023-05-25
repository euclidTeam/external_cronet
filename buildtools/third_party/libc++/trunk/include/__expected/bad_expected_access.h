// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef _LIBCPP___EXPECTED_BAD_EXPECTED_ACCESS_H
#define _LIBCPP___EXPECTED_BAD_EXPECTED_ACCESS_H

#include <__config>
#include <__exception/exception.h>
#include <__utility/move.h>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

#if _LIBCPP_STD_VER >= 23

_LIBCPP_BEGIN_NAMESPACE_STD

template <class _Err>
class bad_expected_access;

template <>
class bad_expected_access<void> : public exception {
protected:
  _LIBCPP_HIDE_FROM_ABI bad_expected_access() noexcept                             = default;
  _LIBCPP_HIDE_FROM_ABI bad_expected_access(const bad_expected_access&)            = default;
  _LIBCPP_HIDE_FROM_ABI bad_expected_access(bad_expected_access&&)                 = default;
  _LIBCPP_HIDE_FROM_ABI bad_expected_access& operator=(const bad_expected_access&) = default;
  _LIBCPP_HIDE_FROM_ABI bad_expected_access& operator=(bad_expected_access&&)      = default;
  ~bad_expected_access() override                                                  = default;

public:
  // The way this has been designed (by using a class template below) means that we'll already
  // have a profusion of these vtables in TUs, and the dynamic linker will already have a bunch
  // of work to do. So it is not worth hiding the <void> specialization in the dylib, given that
  // it adds deployment target restrictions.
  const char* what() const noexcept override { return "bad access to std::expected"; }
};

template <class _Err>
class bad_expected_access : public bad_expected_access<void> {
public:
  _LIBCPP_HIDE_FROM_ABI explicit bad_expected_access(_Err __e) : __unex_(std::move(__e)) {}

  _LIBCPP_HIDE_FROM_ABI _Err& error() & noexcept { return __unex_; }
  _LIBCPP_HIDE_FROM_ABI const _Err& error() const& noexcept { return __unex_; }
  _LIBCPP_HIDE_FROM_ABI _Err&& error() && noexcept { return std::move(__unex_); }
  _LIBCPP_HIDE_FROM_ABI const _Err&& error() const&& noexcept { return std::move(__unex_); }

private:
  _Err __unex_;
};

_LIBCPP_END_NAMESPACE_STD

#endif // _LIBCPP_STD_VER >= 23

#endif // _LIBCPP___EXPECTED_BAD_EXPECTED_ACCESS_H
