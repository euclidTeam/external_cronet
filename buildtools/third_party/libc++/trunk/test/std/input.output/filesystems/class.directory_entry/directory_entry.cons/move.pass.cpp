//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03

// <filesystem>

// class directory_entry

// directory_entry(directory_entry&&) noexcept = default;

#include "filesystem_include.h"
#include <type_traits>
#include <cassert>

#include "test_macros.h"
#include "filesystem_test_helper.h"
#include "test_convertible.h"

static void move_ctor() {
  using namespace fs;
  // Move
  {
    static_assert(std::is_nothrow_move_constructible<directory_entry>::value,
                  "directory_entry must be nothrow move constructible");
    const path p("foo/bar/baz");
    directory_entry e(p);
    assert(e.path() == p);
    directory_entry e2(std::move(e));
    assert(e2.path() == p);
    assert(e.path() != p); // Testing moved from state.
  }
}

static void move_ctor_copies_cache() {
  using namespace fs;
  scoped_test_env env;
  const path dir = env.create_dir("dir");
  const path file = env.create_file("dir/file", 42);
  const path sym = env.create_symlink("dir/file", "sym");

  {
    directory_entry ent(sym);

    fs::remove(sym);

    directory_entry ent_cp(std::move(ent));
    assert(ent_cp.path() == sym);
    assert(ent_cp.is_symlink());
  }

  {
    directory_entry ent(file);

    fs::remove(file);

    directory_entry ent_cp(std::move(ent));
    assert(ent_cp.path() == file);
    assert(ent_cp.is_regular_file());
  }
}

int main(int, char**) {
  move_ctor();
  move_ctor_copies_cache();

  return 0;
}
