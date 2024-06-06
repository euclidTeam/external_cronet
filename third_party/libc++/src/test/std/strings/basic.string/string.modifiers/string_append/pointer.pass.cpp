//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// <string>

// basic_string<charT,traits,Allocator>& append(const charT* s); // constexpr since C++20

#include <string>
#include <stdexcept>
#include <cassert>

#include "test_macros.h"
#include "min_allocator.h"
#include "asan_testing.h"

template <class S>
TEST_CONSTEXPR_CXX20 void test(S s, const typename S::value_type* str, S expected) {
  s.append(str);
  LIBCPP_ASSERT(s.__invariants());
  assert(s == expected);
  LIBCPP_ASSERT(is_string_asan_correct(s));
}

template <class S>
TEST_CONSTEXPR_CXX20 void test_string() {
  test(S(), "", S());
  test(S(), "12345", S("12345"));
  test(S(), "12345678901234567890", S("12345678901234567890"));

  test(S("12345"), "", S("12345"));
  test(S("12345"), "12345", S("1234512345"));
  test(S("12345"), "1234567890", S("123451234567890"));

  test(S("12345678901234567890"), "", S("12345678901234567890"));
  test(S("12345678901234567890"), "12345", S("1234567890123456789012345"));
  test(S("12345678901234567890"), "12345678901234567890", S("1234567890123456789012345678901234567890"));
}

TEST_CONSTEXPR_CXX20 bool test() {
  test_string<std::string>();
#if TEST_STD_VER >= 11
  test_string<std::basic_string<char, std::char_traits<char>, min_allocator<char>>>();
  test_string<std::basic_string<char, std::char_traits<char>, safe_allocator<char>>>();
#endif

  { // test appending to self
    typedef std::string S;
    S s_short = "123/";
    S s_long  = "Lorem ipsum dolor sit amet, consectetur/";

    s_short.append(s_short.c_str());
    assert(s_short == "123/123/");
    s_short.append(s_short.c_str());
    assert(s_short == "123/123/123/123/");
    s_short.append(s_short.c_str());
    assert(s_short == "123/123/123/123/123/123/123/123/");

    s_long.append(s_long.c_str());
    assert(s_long == "Lorem ipsum dolor sit amet, consectetur/Lorem ipsum dolor sit amet, consectetur/");
  }
  return true;
}

int main(int, char**) {
  test();
#if TEST_STD_VER > 17
  static_assert(test());
#endif

  return 0;
}
