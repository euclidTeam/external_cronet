//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// <string>

// int compare(const charT *s) const; // constexpr since C++20

#include <string>
#include <cassert>

#include "test_macros.h"
#include "min_allocator.h"

TEST_CONSTEXPR_CXX20 int sign(int x) {
  if (x == 0)
    return 0;
  if (x < 0)
    return -1;
  return 1;
}

template <class S>
TEST_CONSTEXPR_CXX20 void test(const S& s, const typename S::value_type* str, int x) {
  LIBCPP_ASSERT_NOEXCEPT(s.compare(str));
  assert(sign(s.compare(str)) == sign(x));
}

template <class S>
TEST_CONSTEXPR_CXX20 void test_string() {
  test(S(""), "", 0);
  test(S(""), "abcde", -5);
  test(S(""), "abcdefghij", -10);
  test(S(""), "abcdefghijklmnopqrst", -20);
  test(S("abcde"), "", 5);
  test(S("abcde"), "abcde", 0);
  test(S("abcde"), "abcdefghij", -5);
  test(S("abcde"), "abcdefghijklmnopqrst", -15);
  test(S("abcdefghij"), "", 10);
  test(S("abcdefghij"), "abcde", 5);
  test(S("abcdefghij"), "abcdefghij", 0);
  test(S("abcdefghij"), "abcdefghijklmnopqrst", -10);
  test(S("abcdefghijklmnopqrst"), "", 20);
  test(S("abcdefghijklmnopqrst"), "abcde", 15);
  test(S("abcdefghijklmnopqrst"), "abcdefghij", 10);
  test(S("abcdefghijklmnopqrst"), "abcdefghijklmnopqrst", 0);
}

TEST_CONSTEXPR_CXX20 bool test() {
  test_string<std::string>();
#if TEST_STD_VER >= 11
  test_string<std::basic_string<char, std::char_traits<char>, min_allocator<char> > >();
#endif

  return true;
}

int main(int, char**) {
  test();
#if TEST_STD_VER > 17
  static_assert(test());
#endif

  return 0;
}
