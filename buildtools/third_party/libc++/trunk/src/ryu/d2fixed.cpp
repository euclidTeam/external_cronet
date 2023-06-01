//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Copyright 2018 Ulf Adams
// Copyright (c) Microsoft Corporation. All rights reserved.

// Boost Software License - Version 1.0 - August 17th, 2003

// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:

// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

// Avoid formatting to keep the changes with the original code minimal.
// clang-format off

#include <__assert>
#include <__config>
#include <charconv>
#include <cstring>

#include "include/ryu/common.h"
#include "include/ryu/d2fixed.h"
#include "include/ryu/d2fixed_full_table.h"
#include "include/ryu/d2s.h"
#include "include/ryu/d2s_intrinsics.h"
#include "include/ryu/digit_table.h"

_LIBCPP_BEGIN_NAMESPACE_STD

inline constexpr int __POW10_ADDITIONAL_BITS = 120;

#ifdef _LIBCPP_INTRINSIC128
// Returns the low 64 bits of the high 128 bits of the 256-bit product of a and b.
[[nodiscard]] _LIBCPP_HIDE_FROM_ABI inline uint64_t __umul256_hi128_lo64(
  const uint64_t __aHi, const uint64_t __aLo, const uint64_t __bHi, const uint64_t __bLo) {
  uint64_t __b00Hi;
  const uint64_t __b00Lo = __ryu_umul128(__aLo, __bLo, &__b00Hi);
  uint64_t __b01Hi;
  const uint64_t __b01Lo = __ryu_umul128(__aLo, __bHi, &__b01Hi);
  uint64_t __b10Hi;
  const uint64_t __b10Lo = __ryu_umul128(__aHi, __bLo, &__b10Hi);
  uint64_t __b11Hi;
  const uint64_t __b11Lo = __ryu_umul128(__aHi, __bHi, &__b11Hi);
  (void) __b00Lo; // unused
  (void) __b11Hi; // unused
  const uint64_t __temp1Lo = __b10Lo + __b00Hi;
  const uint64_t __temp1Hi = __b10Hi + (__temp1Lo < __b10Lo);
  const uint64_t __temp2Lo = __b01Lo + __temp1Lo;
  const uint64_t __temp2Hi = __b01Hi + (__temp2Lo < __b01Lo);
  return __b11Lo + __temp1Hi + __temp2Hi;
}

[[nodiscard]] _LIBCPP_HIDE_FROM_ABI inline uint32_t __uint128_mod1e9(const uint64_t __vHi, const uint64_t __vLo) {
  // After multiplying, we're going to shift right by 29, then truncate to uint32_t.
  // This means that we need only 29 + 32 = 61 bits, so we can truncate to uint64_t before shifting.
  const uint64_t __multiplied = __umul256_hi128_lo64(__vHi, __vLo, 0x89705F4136B4A597u, 0x31680A88F8953031u);

  // For uint32_t truncation, see the __mod1e9() comment in d2s_intrinsics.h.
  const uint32_t __shifted = static_cast<uint32_t>(__multiplied >> 29);

  return static_cast<uint32_t>(__vLo) - 1000000000 * __shifted;
}
#endif // ^^^ intrinsics available ^^^

[[nodiscard]] _LIBCPP_HIDE_FROM_ABI inline uint32_t __mulShift_mod1e9(const uint64_t __m, const uint64_t* const __mul, const int32_t __j) {
  uint64_t __high0;                                               // 64
  const uint64_t __low0 = __ryu_umul128(__m, __mul[0], &__high0); // 0
  uint64_t __high1;                                               // 128
  const uint64_t __low1 = __ryu_umul128(__m, __mul[1], &__high1); // 64
  uint64_t __high2;                                               // 192
  const uint64_t __low2 = __ryu_umul128(__m, __mul[2], &__high2); // 128
  const uint64_t __s0low = __low0;                  // 0
  (void) __s0low; // unused
  const uint64_t __s0high = __low1 + __high0;       // 64
  const uint32_t __c1 = __s0high < __low1;
  const uint64_t __s1low = __low2 + __high1 + __c1; // 128
  const uint32_t __c2 = __s1low < __low2; // __high1 + __c1 can't overflow, so compare against __low2
  const uint64_t __s1high = __high2 + __c2;         // 192
  _LIBCPP_ASSERT(__j >= 128, "");
  _LIBCPP_ASSERT(__j <= 180, "");
#ifdef _LIBCPP_INTRINSIC128
  const uint32_t __dist = static_cast<uint32_t>(__j - 128); // __dist: [0, 52]
  const uint64_t __shiftedhigh = __s1high >> __dist;
  const uint64_t __shiftedlow = __ryu_shiftright128(__s1low, __s1high, __dist);
  return __uint128_mod1e9(__shiftedhigh, __shiftedlow);
#else // ^^^ intrinsics available ^^^ / vvv intrinsics unavailable vvv
  if (__j < 160) { // __j: [128, 160)
    const uint64_t __r0 = __mod1e9(__s1high);
    const uint64_t __r1 = __mod1e9((__r0 << 32) | (__s1low >> 32));
    const uint64_t __r2 = ((__r1 << 32) | (__s1low & 0xffffffff));
    return __mod1e9(__r2 >> (__j - 128));
  } else { // __j: [160, 192)
    const uint64_t __r0 = __mod1e9(__s1high);
    const uint64_t __r1 = ((__r0 << 32) | (__s1low >> 32));
    return __mod1e9(__r1 >> (__j - 160));
  }
#endif // ^^^ intrinsics unavailable ^^^
}

void __append_n_digits(const uint32_t __olength, uint32_t __digits, char* const __result) {
  uint32_t __i = 0;
  while (__digits >= 10000) {
#ifdef __clang__ // TRANSITION, LLVM-38217
    const uint32_t __c = __digits - 10000 * (__digits / 10000);
#else
    const uint32_t __c = __digits % 10000;
#endif
    __digits /= 10000;
    const uint32_t __c0 = (__c % 100) << 1;
    const uint32_t __c1 = (__c / 100) << 1;
    std::memcpy(__result + __olength - __i - 2, __DIGIT_TABLE + __c0, 2);
    std::memcpy(__result + __olength - __i - 4, __DIGIT_TABLE + __c1, 2);
    __i += 4;
  }
  if (__digits >= 100) {
    const uint32_t __c = (__digits % 100) << 1;
    __digits /= 100;
    std::memcpy(__result + __olength - __i - 2, __DIGIT_TABLE + __c, 2);
    __i += 2;
  }
  if (__digits >= 10) {
    const uint32_t __c = __digits << 1;
    std::memcpy(__result + __olength - __i - 2, __DIGIT_TABLE + __c, 2);
  } else {
    __result[0] = static_cast<char>('0' + __digits);
  }
}

_LIBCPP_HIDE_FROM_ABI inline void __append_d_digits(const uint32_t __olength, uint32_t __digits, char* const __result) {
  uint32_t __i = 0;
  while (__digits >= 10000) {
#ifdef __clang__ // TRANSITION, LLVM-38217
    const uint32_t __c = __digits - 10000 * (__digits / 10000);
#else
    const uint32_t __c = __digits % 10000;
#endif
    __digits /= 10000;
    const uint32_t __c0 = (__c % 100) << 1;
    const uint32_t __c1 = (__c / 100) << 1;
    std::memcpy(__result + __olength + 1 - __i - 2, __DIGIT_TABLE + __c0, 2);
    std::memcpy(__result + __olength + 1 - __i - 4, __DIGIT_TABLE + __c1, 2);
    __i += 4;
  }
  if (__digits >= 100) {
    const uint32_t __c = (__digits % 100) << 1;
    __digits /= 100;
    std::memcpy(__result + __olength + 1 - __i - 2, __DIGIT_TABLE + __c, 2);
    __i += 2;
  }
  if (__digits >= 10) {
    const uint32_t __c = __digits << 1;
    __result[2] = __DIGIT_TABLE[__c + 1];
    __result[1] = '.';
    __result[0] = __DIGIT_TABLE[__c];
  } else {
    __result[1] = '.';
    __result[0] = static_cast<char>('0' + __digits);
  }
}

_LIBCPP_HIDE_FROM_ABI inline void __append_c_digits(const uint32_t __count, uint32_t __digits, char* const __result) {
  uint32_t __i = 0;
  for (; __i < __count - 1; __i += 2) {
    const uint32_t __c = (__digits % 100) << 1;
    __digits /= 100;
    std::memcpy(__result + __count - __i - 2, __DIGIT_TABLE + __c, 2);
  }
  if (__i < __count) {
    const char __c = static_cast<char>('0' + (__digits % 10));
    __result[__count - __i - 1] = __c;
  }
}

void __append_nine_digits(uint32_t __digits, char* const __result) {
  if (__digits == 0) {
    std::memset(__result, '0', 9);
    return;
  }

  for (uint32_t __i = 0; __i < 5; __i += 4) {
#ifdef __clang__ // TRANSITION, LLVM-38217
    const uint32_t __c = __digits - 10000 * (__digits / 10000);
#else
    const uint32_t __c = __digits % 10000;
#endif
    __digits /= 10000;
    const uint32_t __c0 = (__c % 100) << 1;
    const uint32_t __c1 = (__c / 100) << 1;
    std::memcpy(__result + 7 - __i, __DIGIT_TABLE + __c0, 2);
    std::memcpy(__result + 5 - __i, __DIGIT_TABLE + __c1, 2);
  }
  __result[0] = static_cast<char>('0' + __digits);
}

[[nodiscard]] _LIBCPP_HIDE_FROM_ABI inline uint32_t __indexForExponent(const uint32_t __e) {
  return (__e + 15) / 16;
}

[[nodiscard]] _LIBCPP_HIDE_FROM_ABI inline uint32_t __pow10BitsForIndex(const uint32_t __idx) {
  return 16 * __idx + __POW10_ADDITIONAL_BITS;
}

[[nodiscard]] _LIBCPP_HIDE_FROM_ABI inline uint32_t __lengthForIndex(const uint32_t __idx) {
  // +1 for ceil, +16 for mantissa, +8 to round up when dividing by 9
  return (__log10Pow2(16 * static_cast<int32_t>(__idx)) + 1 + 16 + 8) / 9;
}

[[nodiscard]] to_chars_result __d2fixed_buffered_n(char* _First, char* const _Last, const double __d,
  const uint32_t __precision) {
  char* const _Original_first = _First;

  const uint64_t __bits = __double_to_bits(__d);

  // Case distinction; exit early for the easy cases.
  if (__bits == 0) {
    const int32_t _Total_zero_length = 1 // leading zero
      + static_cast<int32_t>(__precision != 0) // possible decimal point
      + static_cast<int32_t>(__precision); // zeroes after decimal point

    if (_Last - _First < _Total_zero_length) {
      return { _Last, errc::value_too_large };
    }

    *_First++ = '0';
    if (__precision > 0) {
      *_First++ = '.';
      std::memset(_First, '0', __precision);
      _First += __precision;
    }
    return { _First, errc{} };
  }

  // Decode __bits into mantissa and exponent.
  const uint64_t __ieeeMantissa = __bits & ((1ull << __DOUBLE_MANTISSA_BITS) - 1);
  const uint32_t __ieeeExponent = static_cast<uint32_t>(__bits >> __DOUBLE_MANTISSA_BITS);

  int32_t __e2;
  uint64_t __m2;
  if (__ieeeExponent == 0) {
    __e2 = 1 - __DOUBLE_BIAS - __DOUBLE_MANTISSA_BITS;
    __m2 = __ieeeMantissa;
  } else {
    __e2 = static_cast<int32_t>(__ieeeExponent) - __DOUBLE_BIAS - __DOUBLE_MANTISSA_BITS;
    __m2 = (1ull << __DOUBLE_MANTISSA_BITS) | __ieeeMantissa;
  }

  bool __nonzero = false;
  if (__e2 >= -52) {
    const uint32_t __idx = __e2 < 0 ? 0 : __indexForExponent(static_cast<uint32_t>(__e2));
    const uint32_t __p10bits = __pow10BitsForIndex(__idx);
    const int32_t __len = static_cast<int32_t>(__lengthForIndex(__idx));
    for (int32_t __i = __len - 1; __i >= 0; --__i) {
      const uint32_t __j = __p10bits - __e2;
      // Temporary: __j is usually around 128, and by shifting a bit, we push it to 128 or above, which is
      // a slightly faster code path in __mulShift_mod1e9. Instead, we can just increase the multipliers.
      const uint32_t __digits = __mulShift_mod1e9(__m2 << 8, __POW10_SPLIT[__POW10_OFFSET[__idx] + __i],
        static_cast<int32_t>(__j + 8));
      if (__nonzero) {
        if (_Last - _First < 9) {
          return { _Last, errc::value_too_large };
        }
        __append_nine_digits(__digits, _First);
        _First += 9;
      } else if (__digits != 0) {
        const uint32_t __olength = __decimalLength9(__digits);
        if (_Last - _First < static_cast<ptrdiff_t>(__olength)) {
          return { _Last, errc::value_too_large };
        }
        __append_n_digits(__olength, __digits, _First);
        _First += __olength;
        __nonzero = true;
      }
    }
  }
  if (!__nonzero) {
    if (_First == _Last) {
      return { _Last, errc::value_too_large };
    }
    *_First++ = '0';
  }
  if (__precision > 0) {
    if (_First == _Last) {
      return { _Last, errc::value_too_large };
    }
    *_First++ = '.';
  }
  if (__e2 < 0) {
    const int32_t __idx = -__e2 / 16;
    const uint32_t __blocks = __precision / 9 + 1;
    // 0 = don't round up; 1 = round up unconditionally; 2 = round up if odd.
    int __roundUp = 0;
    uint32_t __i = 0;
    if (__blocks <= __MIN_BLOCK_2[__idx]) {
      __i = __blocks;
      if (_Last - _First < static_cast<ptrdiff_t>(__precision)) {
        return { _Last, errc::value_too_large };
      }
      std::memset(_First, '0', __precision);
      _First += __precision;
    } else if (__i < __MIN_BLOCK_2[__idx]) {
      __i = __MIN_BLOCK_2[__idx];
      if (_Last - _First < static_cast<ptrdiff_t>(9 * __i)) {
        return { _Last, errc::value_too_large };
      }
      std::memset(_First, '0', 9 * __i);
      _First += 9 * __i;
    }
    for (; __i < __blocks; ++__i) {
      const int32_t __j = __ADDITIONAL_BITS_2 + (-__e2 - 16 * __idx);
      const uint32_t __p = __POW10_OFFSET_2[__idx] + __i - __MIN_BLOCK_2[__idx];
      if (__p >= __POW10_OFFSET_2[__idx + 1]) {
        // If the remaining digits are all 0, then we might as well use memset.
        // No rounding required in this case.
        const uint32_t __fill = __precision - 9 * __i;
        if (_Last - _First < static_cast<ptrdiff_t>(__fill)) {
          return { _Last, errc::value_too_large };
        }
        std::memset(_First, '0', __fill);
        _First += __fill;
        break;
      }
      // Temporary: __j is usually around 128, and by shifting a bit, we push it to 128 or above, which is
      // a slightly faster code path in __mulShift_mod1e9. Instead, we can just increase the multipliers.
      uint32_t __digits = __mulShift_mod1e9(__m2 << 8, __POW10_SPLIT_2[__p], __j + 8);
      if (__i < __blocks - 1) {
        if (_Last - _First < 9) {
          return { _Last, errc::value_too_large };
        }
        __append_nine_digits(__digits, _First);
        _First += 9;
      } else {
        const uint32_t __maximum = __precision - 9 * __i;
        uint32_t __lastDigit = 0;
        for (uint32_t __k = 0; __k < 9 - __maximum; ++__k) {
          __lastDigit = __digits % 10;
          __digits /= 10;
        }
        if (__lastDigit != 5) {
          __roundUp = __lastDigit > 5;
        } else {
          // Is m * 10^(additionalDigits + 1) / 2^(-__e2) integer?
          const int32_t __requiredTwos = -__e2 - static_cast<int32_t>(__precision) - 1;
          const bool __trailingZeros = __requiredTwos <= 0
            || (__requiredTwos < 60 && __multipleOfPowerOf2(__m2, static_cast<uint32_t>(__requiredTwos)));
          __roundUp = __trailingZeros ? 2 : 1;
        }
        if (__maximum > 0) {
          if (_Last - _First < static_cast<ptrdiff_t>(__maximum)) {
            return { _Last, errc::value_too_large };
          }
          __append_c_digits(__maximum, __digits, _First);
          _First += __maximum;
        }
        break;
      }
    }
    if (__roundUp != 0) {
      char* _Round = _First;
      char* _Dot = _Last;
      while (true) {
        if (_Round == _Original_first) {
          _Round[0] = '1';
          if (_Dot != _Last) {
            _Dot[0] = '0';
            _Dot[1] = '.';
          }
          if (_First == _Last) {
            return { _Last, errc::value_too_large };
          }
          *_First++ = '0';
          break;
        }
        --_Round;
        const char __c = _Round[0];
        if (__c == '.') {
          _Dot = _Round;
        } else if (__c == '9') {
          _Round[0] = '0';
          __roundUp = 1;
        } else {
          if (__roundUp == 1 || __c % 2 != 0) {
            _Round[0] = __c + 1;
          }
          break;
        }
      }
    }
  } else {
    if (_Last - _First < static_cast<ptrdiff_t>(__precision)) {
      return { _Last, errc::value_too_large };
    }
    std::memset(_First, '0', __precision);
    _First += __precision;
  }
  return { _First, errc{} };
}

[[nodiscard]] to_chars_result __d2exp_buffered_n(char* _First, char* const _Last, const double __d,
  uint32_t __precision) {
  char* const _Original_first = _First;

  const uint64_t __bits = __double_to_bits(__d);

  // Case distinction; exit early for the easy cases.
  if (__bits == 0) {
    const int32_t _Total_zero_length = 1 // leading zero
      + static_cast<int32_t>(__precision != 0) // possible decimal point
      + static_cast<int32_t>(__precision) // zeroes after decimal point
      + 4; // "e+00"
    if (_Last - _First < _Total_zero_length) {
      return { _Last, errc::value_too_large };
    }
    *_First++ = '0';
    if (__precision > 0) {
      *_First++ = '.';
      std::memset(_First, '0', __precision);
      _First += __precision;
    }
    std::memcpy(_First, "e+00", 4);
    _First += 4;
    return { _First, errc{} };
  }

  // Decode __bits into mantissa and exponent.
  const uint64_t __ieeeMantissa = __bits & ((1ull << __DOUBLE_MANTISSA_BITS) - 1);
  const uint32_t __ieeeExponent = static_cast<uint32_t>(__bits >> __DOUBLE_MANTISSA_BITS);

  int32_t __e2;
  uint64_t __m2;
  if (__ieeeExponent == 0) {
    __e2 = 1 - __DOUBLE_BIAS - __DOUBLE_MANTISSA_BITS;
    __m2 = __ieeeMantissa;
  } else {
    __e2 = static_cast<int32_t>(__ieeeExponent) - __DOUBLE_BIAS - __DOUBLE_MANTISSA_BITS;
    __m2 = (1ull << __DOUBLE_MANTISSA_BITS) | __ieeeMantissa;
  }

  const bool __printDecimalPoint = __precision > 0;
  ++__precision;
  uint32_t __digits = 0;
  uint32_t __printedDigits = 0;
  uint32_t __availableDigits = 0;
  int32_t __exp = 0;
  if (__e2 >= -52) {
    const uint32_t __idx = __e2 < 0 ? 0 : __indexForExponent(static_cast<uint32_t>(__e2));
    const uint32_t __p10bits = __pow10BitsForIndex(__idx);
    const int32_t __len = static_cast<int32_t>(__lengthForIndex(__idx));
    for (int32_t __i = __len - 1; __i >= 0; --__i) {
      const uint32_t __j = __p10bits - __e2;
      // Temporary: __j is usually around 128, and by shifting a bit, we push it to 128 or above, which is
      // a slightly faster code path in __mulShift_mod1e9. Instead, we can just increase the multipliers.
      __digits = __mulShift_mod1e9(__m2 << 8, __POW10_SPLIT[__POW10_OFFSET[__idx] + __i],
        static_cast<int32_t>(__j + 8));
      if (__printedDigits != 0) {
        if (__printedDigits + 9 > __precision) {
          __availableDigits = 9;
          break;
        }
        if (_Last - _First < 9) {
          return { _Last, errc::value_too_large };
        }
        __append_nine_digits(__digits, _First);
        _First += 9;
        __printedDigits += 9;
      } else if (__digits != 0) {
        __availableDigits = __decimalLength9(__digits);
        __exp = __i * 9 + static_cast<int32_t>(__availableDigits) - 1;
        if (__availableDigits > __precision) {
          break;
        }
        if (__printDecimalPoint) {
          if (_Last - _First < static_cast<ptrdiff_t>(__availableDigits + 1)) {
            return { _Last, errc::value_too_large };
          }
          __append_d_digits(__availableDigits, __digits, _First);
          _First += __availableDigits + 1; // +1 for decimal point
        } else {
          if (_First == _Last) {
            return { _Last, errc::value_too_large };
          }
          *_First++ = static_cast<char>('0' + __digits);
        }
        __printedDigits = __availableDigits;
        __availableDigits = 0;
      }
    }
  }

  if (__e2 < 0 && __availableDigits == 0) {
    const int32_t __idx = -__e2 / 16;
    for (int32_t __i = __MIN_BLOCK_2[__idx]; __i < 200; ++__i) {
      const int32_t __j = __ADDITIONAL_BITS_2 + (-__e2 - 16 * __idx);
      const uint32_t __p = __POW10_OFFSET_2[__idx] + static_cast<uint32_t>(__i) - __MIN_BLOCK_2[__idx];
      // Temporary: __j is usually around 128, and by shifting a bit, we push it to 128 or above, which is
      // a slightly faster code path in __mulShift_mod1e9. Instead, we can just increase the multipliers.
      __digits = (__p >= __POW10_OFFSET_2[__idx + 1]) ? 0 : __mulShift_mod1e9(__m2 << 8, __POW10_SPLIT_2[__p], __j + 8);
      if (__printedDigits != 0) {
        if (__printedDigits + 9 > __precision) {
          __availableDigits = 9;
          break;
        }
        if (_Last - _First < 9) {
          return { _Last, errc::value_too_large };
        }
        __append_nine_digits(__digits, _First);
        _First += 9;
        __printedDigits += 9;
      } else if (__digits != 0) {
        __availableDigits = __decimalLength9(__digits);
        __exp = -(__i + 1) * 9 + static_cast<int32_t>(__availableDigits) - 1;
        if (__availableDigits > __precision) {
          break;
        }
        if (__printDecimalPoint) {
          if (_Last - _First < static_cast<ptrdiff_t>(__availableDigits + 1)) {
            return { _Last, errc::value_too_large };
          }
          __append_d_digits(__availableDigits, __digits, _First);
          _First += __availableDigits + 1; // +1 for decimal point
        } else {
          if (_First == _Last) {
            return { _Last, errc::value_too_large };
          }
          *_First++ = static_cast<char>('0' + __digits);
        }
        __printedDigits = __availableDigits;
        __availableDigits = 0;
      }
    }
  }

  const uint32_t __maximum = __precision - __printedDigits;
  if (__availableDigits == 0) {
    __digits = 0;
  }
  uint32_t __lastDigit = 0;
  if (__availableDigits > __maximum) {
    for (uint32_t __k = 0; __k < __availableDigits - __maximum; ++__k) {
      __lastDigit = __digits % 10;
      __digits /= 10;
    }
  }
  // 0 = don't round up; 1 = round up unconditionally; 2 = round up if odd.
  int __roundUp = 0;
  if (__lastDigit != 5) {
    __roundUp = __lastDigit > 5;
  } else {
    // Is m * 2^__e2 * 10^(__precision + 1 - __exp) integer?
    // __precision was already increased by 1, so we don't need to write + 1 here.
    const int32_t __rexp = static_cast<int32_t>(__precision) - __exp;
    const int32_t __requiredTwos = -__e2 - __rexp;
    bool __trailingZeros = __requiredTwos <= 0
      || (__requiredTwos < 60 && __multipleOfPowerOf2(__m2, static_cast<uint32_t>(__requiredTwos)));
    if (__rexp < 0) {
      const int32_t __requiredFives = -__rexp;
      __trailingZeros = __trailingZeros && __multipleOfPowerOf5(__m2, static_cast<uint32_t>(__requiredFives));
    }
    __roundUp = __trailingZeros ? 2 : 1;
  }
  if (__printedDigits != 0) {
    if (_Last - _First < static_cast<ptrdiff_t>(__maximum)) {
      return { _Last, errc::value_too_large };
    }
    if (__digits == 0) {
      std::memset(_First, '0', __maximum);
    } else {
      __append_c_digits(__maximum, __digits, _First);
    }
    _First += __maximum;
  } else {
    if (__printDecimalPoint) {
      if (_Last - _First < static_cast<ptrdiff_t>(__maximum + 1)) {
        return { _Last, errc::value_too_large };
      }
      __append_d_digits(__maximum, __digits, _First);
      _First += __maximum + 1; // +1 for decimal point
    } else {
      if (_First == _Last) {
        return { _Last, errc::value_too_large };
      }
      *_First++ = static_cast<char>('0' + __digits);
    }
  }
  if (__roundUp != 0) {
    char* _Round = _First;
    while (true) {
      if (_Round == _Original_first) {
        _Round[0] = '1';
        ++__exp;
        break;
      }
      --_Round;
      const char __c = _Round[0];
      if (__c == '.') {
        // Keep going.
      } else if (__c == '9') {
        _Round[0] = '0';
        __roundUp = 1;
      } else {
        if (__roundUp == 1 || __c % 2 != 0) {
          _Round[0] = __c + 1;
        }
        break;
      }
    }
  }

  char _Sign_character;

  if (__exp < 0) {
    _Sign_character = '-';
    __exp = -__exp;
  } else {
    _Sign_character = '+';
  }

  const int _Exponent_part_length = __exp >= 100
    ? 5 // "e+NNN"
    : 4; // "e+NN"

  if (_Last - _First < _Exponent_part_length) {
    return { _Last, errc::value_too_large };
  }

  *_First++ = 'e';
  *_First++ = _Sign_character;

  if (__exp >= 100) {
    const int32_t __c = __exp % 10;
    std::memcpy(_First, __DIGIT_TABLE + 2 * (__exp / 10), 2);
    _First[2] = static_cast<char>('0' + __c);
    _First += 3;
  } else {
    std::memcpy(_First, __DIGIT_TABLE + 2 * __exp, 2);
    _First += 2;
  }

  return { _First, errc{} };
}

_LIBCPP_END_NAMESPACE_STD

// clang-format on
