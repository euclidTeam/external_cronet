// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___FORMAT_UNICODE_H
#define _LIBCPP___FORMAT_UNICODE_H

#include <__assert>
#include <__bit/countl.h>
#include <__concepts/same_as.h>
#include <__config>
#include <__format/extended_grapheme_cluster_table.h>
#include <__iterator/concepts.h>
#include <__iterator/readable_traits.h> // iter_value_t
#include <string_view>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

_LIBCPP_BEGIN_NAMESPACE_STD

#if _LIBCPP_STD_VER >= 20

namespace __unicode {

// Helper struct for the result of a consume operation.
//
// The status value for a correct code point is 0. This allows a valid value to
// be used without masking.
// When the decoding fails it know the number of code units affected. For the
// current use-cases that value is not needed, therefore it is not stored.
// The escape routine needs the number of code units for both a valid and
// invalid character and keeps track of it itself. Doing it in this result
// unconditionally would give some overhead when the value is unneeded.
struct __consume_result {
  // When __status == __ok it contains the decoded code point.
  // Else it contains the replacement character U+FFFD
  char32_t __code_point : 31;

  enum : char32_t {
    // Consumed a well-formed code point.
    __ok = 0,
    // Encountered invalid UTF-8
    __error = 1
  } __status : 1 {__ok};
};
static_assert(sizeof(__consume_result) == sizeof(char32_t));

#  ifndef _LIBCPP_HAS_NO_UNICODE

/// Implements the grapheme cluster boundary rules
///
/// These rules are used to implement format's width estimation as stated in
/// [format.string.std]/11
///
/// The Standard refers to UAX \#29 for Unicode 12.0.0
/// https://www.unicode.org/reports/tr29/#Grapheme_Cluster_Boundary_Rules
///
/// The data tables used are
/// https://www.unicode.org/Public/UCD/latest/ucd/auxiliary/GraphemeBreakProperty.txt
/// https://www.unicode.org/Public/UCD/latest/ucd/emoji/emoji-data.txt
/// https://www.unicode.org/Public/UCD/latest/ucd/auxiliary/GraphemeBreakTest.txt (for testing only)

inline constexpr char32_t __replacement_character = U'\ufffd';

// The error of a consume operation.
//
// This sets the code point to the replacement character. This code point does
// not participate in the grapheme clustering, so grapheme clustering code can
// ignore the error status and always use the code point.
inline constexpr __consume_result __consume_result_error{__replacement_character, __consume_result::__error};

[[nodiscard]] _LIBCPP_HIDE_FROM_ABI constexpr bool __is_high_surrogate(char32_t __value) {
  return __value >= 0xd800 && __value <= 0xdbff;
}

[[nodiscard]] _LIBCPP_HIDE_FROM_ABI constexpr bool __is_low_surrogate(char32_t __value) {
  return __value >= 0xdc00 && __value <= 0xdfff;
}

// https://www.unicode.org/glossary/#surrogate_code_point
[[nodiscard]] _LIBCPP_HIDE_FROM_ABI inline constexpr bool __is_surrogate(char32_t __value) {
  return __value >= 0xd800 && __value <= 0xdfff;
}

// https://www.unicode.org/glossary/#code_point
[[nodiscard]] _LIBCPP_HIDE_FROM_ABI inline constexpr bool __is_code_point(char32_t __value) {
  return __value <= 0x10ffff;
}

// https://www.unicode.org/glossary/#unicode_scalar_value
[[nodiscard]] _LIBCPP_HIDE_FROM_ABI inline constexpr bool __is_scalar_value(char32_t __value) {
  return __unicode::__is_code_point(__value) && !__unicode::__is_surrogate(__value);
}

template <contiguous_iterator _Iterator>
  requires same_as<iter_value_t<_Iterator>, char>
_LIBCPP_HIDE_FROM_ABI constexpr bool __is_continuation(_Iterator __char, int __count) {
  do {
    if ((*__char & 0b1100'0000) != 0b1000'0000)
      return false;
    --__count;
    ++__char;
  } while (__count);
  return true;
}

/// Helper class to extract a code unit from a Unicode character range.
///
/// The stored range is a view. There are multiple specialization for different
/// character types.
template <class _CharT>
class __code_point_view;

/// UTF-8 specialization.
template <>
class __code_point_view<char> {
  using _Iterator = basic_string_view<char>::const_iterator;

public:
  _LIBCPP_HIDE_FROM_ABI constexpr explicit __code_point_view(_Iterator __first, _Iterator __last)
      : __first_(__first), __last_(__last) {}

  _LIBCPP_HIDE_FROM_ABI constexpr bool __at_end() const noexcept { return __first_ == __last_; }
  _LIBCPP_HIDE_FROM_ABI constexpr _Iterator __position() const noexcept { return __first_; }

  // https://www.unicode.org/versions/latest/ch03.pdf#G7404
  // Based on Table 3-7, Well-Formed UTF-8 Byte Sequences
  //
  // Code Points        First Byte Second Byte Third Byte Fourth Byte  Remarks
  // U+0000..U+007F     00..7F                                         U+0000..U+007F 1 code unit range
  //                    C0..C1     80..BF                              invalid overlong encoding
  // U+0080..U+07FF     C2..DF     80..BF                              U+0080..U+07FF 2 code unit range
  //                    E0         80..9F      80..BF                  invalid overlong encoding
  // U+0800..U+0FFF     E0         A0..BF      80..BF                  U+0800..U+FFFF 3 code unit range
  // U+1000..U+CFFF     E1..EC     80..BF      80..BF
  // U+D000..U+D7FF     ED         80..9F      80..BF
  // U+D800..U+DFFF     ED         A0..BF      80..BF                  invalid encoding of surrogate code point
  // U+E000..U+FFFF     EE..EF     80..BF      80..BF
  //                    F0         80..8F      80..BF     80..BF       invalid overlong encoding
  // U+10000..U+3FFFF   F0         90..BF      80..BF     80..BF       U+10000..U+10FFFF 4 code unit range
  // U+40000..U+FFFFF   F1..F3     80..BF      80..BF     80..BF
  // U+100000..U+10FFFF F4         80..8F      80..BF     80..BF
  //                    F4         90..BF      80..BF     80..BF       U+110000.. invalid code point range
  //
  // Unlike other parsers, these invalid entries are tested after decoding.
  // - The parser always needs to consume these code units
  // - The code is optimized for well-formed UTF-8
  [[nodiscard]] _LIBCPP_HIDE_FROM_ABI constexpr __consume_result __consume() noexcept {
    _LIBCPP_ASSERT_UNCATEGORIZED(__first_ != __last_, "can't move beyond the end of input");

    // Based on the number of leading 1 bits the number of code units in the
    // code point can be determined. See
    // https://en.wikipedia.org/wiki/UTF-8#Encoding
    switch (std::countl_one(static_cast<unsigned char>(*__first_))) {
    case 0:
      return {static_cast<unsigned char>(*__first_++)};

    case 2: {
      if (__last_ - __first_ < 2 || !__unicode::__is_continuation(__first_ + 1, 1)) [[unlikely]]
        break;

      char32_t __value = static_cast<unsigned char>(*__first_++) & 0x1f;
      __value <<= 6;
      __value |= static_cast<unsigned char>(*__first_++) & 0x3f;

      // These values should be encoded in 1 UTF-8 code unit.
      if (__value < 0x0080) [[unlikely]]
        return __consume_result_error;

      return {__value};
    }

    case 3: {
      if (__last_ - __first_ < 3 || !__unicode::__is_continuation(__first_ + 1, 2)) [[unlikely]]
        break;

      char32_t __value = static_cast<unsigned char>(*__first_++) & 0x0f;
      __value <<= 6;
      __value |= static_cast<unsigned char>(*__first_++) & 0x3f;
      __value <<= 6;
      __value |= static_cast<unsigned char>(*__first_++) & 0x3f;

      // These values should be encoded in 1 or 2 UTF-8 code units.
      if (__value < 0x0800) [[unlikely]]
        return __consume_result_error;

      // A surrogate value is always encoded in 3 UTF-8 code units.
      if (__unicode::__is_surrogate(__value)) [[unlikely]]
        return __consume_result_error;

      return {__value};
    }

    case 4: {
      if (__last_ - __first_ < 4 || !__unicode::__is_continuation(__first_ + 1, 3)) [[unlikely]]
        break;

      char32_t __value = static_cast<unsigned char>(*__first_++) & 0x07;
      __value <<= 6;
      __value |= static_cast<unsigned char>(*__first_++) & 0x3f;
      __value <<= 6;
      __value |= static_cast<unsigned char>(*__first_++) & 0x3f;
      __value <<= 6;
      __value |= static_cast<unsigned char>(*__first_++) & 0x3f;

      // These values should be encoded in 1, 2, or 3 UTF-8 code units.
      if (__value < 0x10000) [[unlikely]]
        return __consume_result_error;

      // A value too large is always encoded in 4 UTF-8 code units.
      if (!__unicode::__is_code_point(__value)) [[unlikely]]
        return __consume_result_error;

      return {__value};
    }
    }
    // An invalid number of leading ones can be garbage or a code unit in the
    // middle of a code point. By consuming one code unit the parser may get
    // "in sync" after a few code units.
    ++__first_;
    return __consume_result_error;
  }

private:
  _Iterator __first_;
  _Iterator __last_;
};

#    ifndef _LIBCPP_HAS_NO_WIDE_CHARACTERS
_LIBCPP_HIDE_FROM_ABI constexpr bool __is_surrogate_pair_high(wchar_t __value) {
  return __value >= 0xd800 && __value <= 0xdbff;
}

_LIBCPP_HIDE_FROM_ABI constexpr bool __is_surrogate_pair_low(wchar_t __value) {
  return __value >= 0xdc00 && __value <= 0xdfff;
}

/// This specialization depends on the size of wchar_t
/// - 2 UTF-16 (for example Windows and AIX)
/// - 4 UTF-32 (for example Linux)
template <>
class __code_point_view<wchar_t> {
  using _Iterator = typename basic_string_view<wchar_t>::const_iterator;

public:
  static_assert(sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4, "sizeof(wchar_t) has a not implemented value");

  _LIBCPP_HIDE_FROM_ABI constexpr explicit __code_point_view(_Iterator __first, _Iterator __last)
      : __first_(__first), __last_(__last) {}

  _LIBCPP_HIDE_FROM_ABI constexpr _Iterator __position() const noexcept { return __first_; }
  _LIBCPP_HIDE_FROM_ABI constexpr bool __at_end() const noexcept { return __first_ == __last_; }

  [[nodiscard]] _LIBCPP_HIDE_FROM_ABI constexpr __consume_result __consume() noexcept {
    _LIBCPP_ASSERT_UNCATEGORIZED(__first_ != __last_, "can't move beyond the end of input");

    char32_t __value = static_cast<char32_t>(*__first_++);
    if constexpr (sizeof(wchar_t) == 2) {
      if (__unicode::__is_low_surrogate(__value)) [[unlikely]]
        return __consume_result_error;

      if (__unicode::__is_high_surrogate(__value)) {
        if (__first_ == __last_ || !__unicode::__is_low_surrogate(static_cast<char32_t>(*__first_))) [[unlikely]]
          return __consume_result_error;

        __value -= 0xd800;
        __value <<= 10;
        __value += static_cast<char32_t>(*__first_++) - 0xdc00;
        __value += 0x10000;

        if (!__unicode::__is_code_point(__value)) [[unlikely]]
          return __consume_result_error;
      }
    } else {
      if (!__unicode::__is_scalar_value(__value)) [[unlikely]]
        return __consume_result_error;
    }

    return {__value};
  }

private:
  _Iterator __first_;
  _Iterator __last_;
};
#    endif // _LIBCPP_HAS_NO_WIDE_CHARACTERS

_LIBCPP_HIDE_FROM_ABI constexpr bool __at_extended_grapheme_cluster_break(
    bool& __ri_break_allowed,
    bool __has_extened_pictographic,
    __extended_grapheme_custer_property_boundary::__property __prev,
    __extended_grapheme_custer_property_boundary::__property __next) {
  using __extended_grapheme_custer_property_boundary::__property;

  __has_extened_pictographic |= __prev == __property::__Extended_Pictographic;

  // https://www.unicode.org/reports/tr29/tr29-39.html#Grapheme_Cluster_Boundary_Rules

  // *** Break at the start and end of text, unless the text is empty. ***

  _LIBCPP_ASSERT_UNCATEGORIZED(__prev != __property::__sot, "should be handled in the constructor"); // GB1
  _LIBCPP_ASSERT_UNCATEGORIZED(__prev != __property::__eot, "should be handled by our caller");      // GB2

  // *** Do not break between a CR and LF. Otherwise, break before and after controls. ***
  if (__prev == __property::__CR && __next == __property::__LF) // GB3
    return false;

  if (__prev == __property::__Control || __prev == __property::__CR || __prev == __property::__LF) // GB4
    return true;

  if (__next == __property::__Control || __next == __property::__CR || __next == __property::__LF) // GB5
    return true;

  // *** Do not break Hangul syllable sequences. ***
  if (__prev == __property::__L &&
      (__next == __property::__L || __next == __property::__V || __next == __property::__LV ||
       __next == __property::__LVT)) // GB6
    return false;

  if ((__prev == __property::__LV || __prev == __property::__V) &&
      (__next == __property::__V || __next == __property::__T)) // GB7
    return false;

  if ((__prev == __property::__LVT || __prev == __property::__T) && __next == __property::__T) // GB8
    return false;

  // *** Do not break before extending characters or ZWJ. ***
  if (__next == __property::__Extend || __next == __property::__ZWJ)
    return false; // GB9

  // *** Do not break before SpacingMarks, or after Prepend characters. ***
  if (__next == __property::__SpacingMark) // GB9a
    return false;

  if (__prev == __property::__Prepend) // GB9b
    return false;

  // *** Do not break within emoji modifier sequences or emoji zwj sequences. ***

  // GB11 \p{Extended_Pictographic} Extend* ZWJ x \p{Extended_Pictographic}
  //
  // Note that several parts of this rule are matched by GB9: Any x (Extend | ZWJ)
  // - \p{Extended_Pictographic} x Extend
  // - Extend x Extend
  // - \p{Extended_Pictographic} x ZWJ
  // - Extend x ZWJ
  //
  // So the only case left to test is
  // - \p{Extended_Pictographic}' x ZWJ x \p{Extended_Pictographic}
  //   where  \p{Extended_Pictographic}' is stored in __has_extened_pictographic
  if (__has_extened_pictographic && __prev == __property::__ZWJ && __next == __property::__Extended_Pictographic)
    return false;

  // *** Do not break within emoji flag sequences ***

  // That is, do not break between regional indicator (RI) symbols if there
  // is an odd number of RI characters before the break point.

  if (__prev == __property::__Regional_Indicator && __next == __property::__Regional_Indicator) { // GB12 + GB13
    __ri_break_allowed = !__ri_break_allowed;
    return __ri_break_allowed;
  }

  // *** Otherwise, break everywhere. ***
  return true; // GB999
}

/// Helper class to extract an extended grapheme cluster from a Unicode character range.
///
/// This function is used to determine the column width of an extended grapheme
/// cluster. In order to do that only the first code point is evaluated.
/// Therefore only this code point is extracted.
template <class _CharT>
class __extended_grapheme_cluster_view {
  using _Iterator = typename basic_string_view<_CharT>::const_iterator;

public:
  _LIBCPP_HIDE_FROM_ABI constexpr explicit __extended_grapheme_cluster_view(_Iterator __first, _Iterator __last)
      : __code_point_view_(__first, __last),
        __next_code_point_(__code_point_view_.__consume().__code_point),
        __next_prop_(__extended_grapheme_custer_property_boundary::__get_property(__next_code_point_)) {}

  struct __cluster {
    /// The first code point of the extended grapheme cluster.
    ///
    /// The first code point is used to estimate the width of the extended
    /// grapheme cluster.
    char32_t __code_point_;

    /// Points one beyond the last code unit in the extended grapheme cluster.
    ///
    /// It's expected the caller has the start position and thus can determine
    /// the code unit range of the extended grapheme cluster.
    _Iterator __last_;
  };

  _LIBCPP_HIDE_FROM_ABI constexpr __cluster __consume() {
    _LIBCPP_ASSERT_UNCATEGORIZED(
        __next_prop_ != __extended_grapheme_custer_property_boundary::__property::__eot,
        "can't move beyond the end of input");

    char32_t __code_point = __next_code_point_;
    if (!__code_point_view_.__at_end())
      return {__code_point, __get_break()};

    __next_prop_ = __extended_grapheme_custer_property_boundary::__property::__eot;
    return {__code_point, __code_point_view_.__position()};
  }

private:
  __code_point_view<_CharT> __code_point_view_;

  char32_t __next_code_point_;
  __extended_grapheme_custer_property_boundary::__property __next_prop_;

  _LIBCPP_HIDE_FROM_ABI constexpr _Iterator __get_break() {
    bool __ri_break_allowed         = true;
    bool __has_extened_pictographic = false;
    while (true) {
      _Iterator __result                                              = __code_point_view_.__position();
      __extended_grapheme_custer_property_boundary::__property __prev = __next_prop_;
      if (__code_point_view_.__at_end()) {
        __next_prop_ = __extended_grapheme_custer_property_boundary::__property::__eot;
        return __result;
      }
      __next_code_point_ = __code_point_view_.__consume().__code_point;
      __next_prop_       = __extended_grapheme_custer_property_boundary::__get_property(__next_code_point_);

      __has_extened_pictographic |=
          __prev == __extended_grapheme_custer_property_boundary::__property::__Extended_Pictographic;

      if (__at_extended_grapheme_cluster_break(__ri_break_allowed, __has_extened_pictographic, __prev, __next_prop_))
        return __result;
    }
  }
};

template <contiguous_iterator _Iterator>
__extended_grapheme_cluster_view(_Iterator, _Iterator) -> __extended_grapheme_cluster_view<iter_value_t<_Iterator>>;

#  else //  _LIBCPP_HAS_NO_UNICODE

// For ASCII every character is a "code point".
// This makes it easier to write code agnostic of the _LIBCPP_HAS_NO_UNICODE define.
template <class _CharT>
class __code_point_view {
  using _Iterator = typename basic_string_view<_CharT>::const_iterator;

public:
  _LIBCPP_HIDE_FROM_ABI constexpr explicit __code_point_view(_Iterator __first, _Iterator __last)
      : __first_(__first), __last_(__last) {}

  _LIBCPP_HIDE_FROM_ABI constexpr bool __at_end() const noexcept { return __first_ == __last_; }
  _LIBCPP_HIDE_FROM_ABI constexpr _Iterator __position() const noexcept { return __first_; }

  [[nodiscard]] _LIBCPP_HIDE_FROM_ABI constexpr __consume_result __consume() noexcept {
    _LIBCPP_ASSERT_UNCATEGORIZED(__first_ != __last_, "can't move beyond the end of input");
    return {static_cast<char32_t>(*__first_++)};
  }

private:
  _Iterator __first_;
  _Iterator __last_;
};

#  endif //  _LIBCPP_HAS_NO_UNICODE

} // namespace __unicode

#endif //_LIBCPP_STD_VER >= 20

_LIBCPP_END_NAMESPACE_STD

#endif // _LIBCPP___FORMAT_UNICODE_H
