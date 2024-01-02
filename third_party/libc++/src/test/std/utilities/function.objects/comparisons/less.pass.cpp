//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// ADDITIONAL_COMPILE_FLAGS: -D_LIBCPP_DISABLE_DEPRECATION_WARNINGS

// <functional>

// less

#include <functional>
#include <type_traits>
#include <cassert>

#include "test_macros.h"
#include "pointer_comparison_test_helper.h"

int main(int, char**)
{
    typedef std::less<int> F;
    const F f = F();
#if TEST_STD_VER <= 17
    static_assert((std::is_same<int, F::first_argument_type>::value), "" );
    static_assert((std::is_same<int, F::second_argument_type>::value), "" );
    static_assert((std::is_same<bool, F::result_type>::value), "" );
#endif
    assert(!f(36, 36));
    assert(!f(36, 6));
    assert(f(6, 36));
    {
        // test total ordering of int* for less<int*> and less<void>.
        do_pointer_comparison_test<std::less>();
    }
#if TEST_STD_VER > 11
    typedef std::less<> F2;
    const F2 f2 = F2();
    assert(!f2(36, 36));
    assert(!f2(36, 6));
    assert( f2(6, 36));
    assert(!f2(36, 6.0));
    assert(!f2(36.0, 6));
    assert( f2(6, 36.0));
    assert( f2(6.0, 36));

    constexpr bool foo = std::less<int> () (36, 36);
    static_assert ( !foo, "" );

    constexpr bool bar = std::less<> () (36.0, 36);
    static_assert ( !bar, "" );
#endif

  return 0;
}
