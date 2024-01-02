//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// ADDITIONAL_COMPILE_FLAGS: -D_LIBCPP_DISABLE_DEPRECATION_WARNINGS

// <strstream>

// class strstreambuf

// streambuf* setbuf(char* s, streamsize n);

#include <strstream>
#include <cassert>

#include "test_macros.h"

int main(int, char**)
{
    {
        char buf[] = "0123456789";
        std::strstreambuf sb(buf, 0);
        assert(sb.pubsetbuf(0, 0) == &sb);
        assert(sb.str() == std::string("0123456789"));
    }

  return 0;
}
