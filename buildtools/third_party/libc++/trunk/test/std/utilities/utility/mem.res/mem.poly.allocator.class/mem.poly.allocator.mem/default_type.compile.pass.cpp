//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17
// UNSUPPORTED: use_system_cxx_lib && target={{.+}}-apple-macosx10.{{9|10|11|12|13|14|15}}
// UNSUPPORTED: use_system_cxx_lib && target={{.+}}-apple-macosx{{11.0|12.0}}

// test_memory_resource requires RTTI for dynamic_cast
// UNSUPPORTED: no-rtti

#include <memory_resource>
#include <type_traits>

static_assert(std::is_same_v<std::pmr::polymorphic_allocator<>, std::pmr::polymorphic_allocator<std::byte>>);
