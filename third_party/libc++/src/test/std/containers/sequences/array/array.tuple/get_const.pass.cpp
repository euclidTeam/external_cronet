//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// <array>

// template <size_t I, class T, size_t N> const T& get(const array<T, N>& a);

#include <array>
#include <cassert>

#include "test_macros.h"

TEST_CONSTEXPR_CXX14 bool tests()
{
    {
        std::array<double, 1> const array = {3.3};
        assert(std::get<0>(array) == 3.3);
    }
    {
        std::array<double, 2> const array = {3.3, 4.4};
        assert(std::get<0>(array) == 3.3);
        assert(std::get<1>(array) == 4.4);
    }
    {
        std::array<double, 3> const array = {3.3, 4.4, 5.5};
        assert(std::get<0>(array) == 3.3);
        assert(std::get<1>(array) == 4.4);
        assert(std::get<2>(array) == 5.5);
    }
    {
        std::array<double, 1> const array = {3.3};
        static_assert(std::is_same<double const&, decltype(std::get<0>(array))>::value, "");
    }

    return true;
}

int main(int, char**)
{
    tests();
#if TEST_STD_VER >= 14
    static_assert(tests(), "");
#endif
    return 0;
}
