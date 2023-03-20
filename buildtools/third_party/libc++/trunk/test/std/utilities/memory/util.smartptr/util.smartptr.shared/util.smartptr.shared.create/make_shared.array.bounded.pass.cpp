//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14, c++17

// Aligned deallocation isn't provided before macOS 10.14, and some tests for overaligned types
// below require that feature.
// XFAIL: use_system_cxx_lib && target={{.+}}-apple-macosx{{10.9|10.10|10.11|10.12|10.13}}

// <memory>

// shared_ptr

// template<class T>
// shared_ptr<T> make_shared(); // T is U[N]
//
// template<class T>
// shared_ptr<T> make_shared(const remove_extent_t<T>& u); // T is U[N]

#include <cassert>
#include <concepts>
#include <cstdint> // std::uintptr_t
#include <memory>
#include <utility>

#include "operator_hijacker.h"
#include "types.h"

template <class T, class ...Args>
concept CanMakeShared = requires(Args&& ...args) {
  { std::make_shared<T>(std::forward<Args>(args)...) } -> std::same_as<std::shared_ptr<T>>;
};

int main(int, char**) {
  // Make sure we initialize elements correctly
  {
    // Without passing an initial value
    {
      using Array = int[8];
      std::shared_ptr<Array> ptr = std::make_shared<Array>();
      for (unsigned i = 0; i < 8; ++i) {
        assert(ptr[i] == 0);
      }
    }
    {
      using Array = int[8][3];
      std::shared_ptr<Array> ptr = std::make_shared<Array>();
      for (unsigned i = 0; i < 8; ++i) {
        assert(ptr[i][0] == 0);
        assert(ptr[i][1] == 0);
        assert(ptr[i][2] == 0);
      }
    }
    {
      using Array = int[8][3][2];
      std::shared_ptr<Array> ptr = std::make_shared<Array>();
      for (unsigned i = 0; i < 8; ++i) {
        assert(ptr[i][0][0] == 0);
        assert(ptr[i][0][1] == 0);
        assert(ptr[i][1][0] == 0);
        assert(ptr[i][1][1] == 0);
        assert(ptr[i][2][0] == 0);
        assert(ptr[i][2][1] == 0);
      }
    }

    // Passing an initial value
    {
      using Array = int[8];
      int init = 42;
      std::shared_ptr<Array> ptr = std::make_shared<Array>(init);
      for (unsigned i = 0; i < 8; ++i) {
        assert(ptr[i] == init);
      }
    }
    {
      using Array = int[8][3];
      int init[3] = {42, 43, 44};
      std::shared_ptr<Array> ptr = std::make_shared<Array>(init);
      for (unsigned i = 0; i < 8; ++i) {
        assert(ptr[i][0] == 42);
        assert(ptr[i][1] == 43);
        assert(ptr[i][2] == 44);
      }
    }
    {
      using Array = int[8][3][2];
      int init[3][2] = {{31, 32}, {41, 42}, {51, 52}};
      std::shared_ptr<Array> ptr = std::make_shared<Array>(init);
      for (unsigned i = 0; i < 8; ++i) {
        assert(ptr[i][0][0] == 31);
        assert(ptr[i][0][1] == 32);
        assert(ptr[i][1][0] == 41);
        assert(ptr[i][1][1] == 42);
        assert(ptr[i][2][0] == 51);
        assert(ptr[i][2][1] == 52);
      }
    }
  }

  // Make sure array elements are destroyed in reverse order
  {
    // Without passing an initial value
    {
      using Array = DestroyInReverseOrder[8];
      DestroyInReverseOrder::reset();
      {
        std::shared_ptr<Array> ptr = std::make_shared<Array>();
        assert(DestroyInReverseOrder::alive() == 8);
      }
      assert(DestroyInReverseOrder::alive() == 0);
    }
    {
      using Array = DestroyInReverseOrder[8][3];
      DestroyInReverseOrder::reset();
      {
        std::shared_ptr<Array> ptr = std::make_shared<Array>();
        assert(DestroyInReverseOrder::alive() == 8 * 3);
      }
      assert(DestroyInReverseOrder::alive() == 0);
    }
    {
      using Array = DestroyInReverseOrder[8][3][2];
      DestroyInReverseOrder::reset();
      {
        std::shared_ptr<Array> ptr = std::make_shared<Array>();
        assert(DestroyInReverseOrder::alive() == 8 * 3 * 2);
      }
      assert(DestroyInReverseOrder::alive() == 0);
    }

    // Passing an initial value
    {
      using Array = DestroyInReverseOrder[8];
      int count = 0;
      DestroyInReverseOrder init(&count);
      int init_count = 1;
      {
        std::shared_ptr<Array> ptr = std::make_shared<Array>(init);
        assert(count == 8 + init_count);
      }
      assert(count == init_count);
    }
    {
      using Array = DestroyInReverseOrder[8][3];
      int count = 0;
      DestroyInReverseOrder init[3] = {&count, &count, &count};
      int init_count = 3;
      {
        std::shared_ptr<Array> ptr = std::make_shared<Array>(init);
        assert(count == 8 * 3 + init_count);
      }
      assert(count == init_count);
    }
    {
      using Array = DestroyInReverseOrder[8][3][2];
      int count = 0;
      DestroyInReverseOrder init[3][2] = {{&count, &count}, {&count, &count}, {&count, &count}};
      int init_count = 3 * 2;
      {
        std::shared_ptr<Array> ptr = std::make_shared<Array>(init);
        assert(count == 8 * 3 * 2 + init_count);
      }
      assert(count == init_count);
    }
  }

  // Count the number of copies being made
  {
    // Without passing an initial value
    {
      using Array = CountCopies[8];
      CountCopies::reset();
      std::shared_ptr<Array> ptr = std::make_shared<Array>();
      assert(CountCopies::copies() == 0);
    }
    {
      using Array = CountCopies[8][3];
      CountCopies::reset();
      std::shared_ptr<Array> ptr = std::make_shared<Array>();
      assert(CountCopies::copies() == 0);
    }
    {
      using Array = CountCopies[8][3][2];
      CountCopies::reset();
      std::shared_ptr<Array> ptr = std::make_shared<Array>();
      assert(CountCopies::copies() == 0);
    }

    // Passing an initial value
    {
      using Array = CountCopies[8];
      int copies = 0;
      CountCopies init(&copies);
      std::shared_ptr<Array> ptr = std::make_shared<Array>(init);
      assert(copies == 8);
    }
    {
      using Array = CountCopies[8][3];
      int copies = 0;
      CountCopies init[3] = {&copies, &copies, &copies};
      std::shared_ptr<Array> ptr = std::make_shared<Array>(init);
      assert(copies == 8 * 3);
    }
    {
      using Array = CountCopies[8][3][2];
      int copies = 0;
      CountCopies init[3][2] = {{&copies, &copies}, {&copies, &copies}, {&copies, &copies}};
      std::shared_ptr<Array> ptr = std::make_shared<Array>(init);
      assert(copies == 8 * 3 * 2);
    }
  }

  // Make sure array elements are aligned properly when the array contains an overaligned type.
  //
  // Here, we don't need to test both the with-initial-value and without-initial-value code paths,
  // since we're just checking the alignment and both are going to use the same code path unless
  // the implementation is completely crazy.
  {
    auto check_alignment = []<class T> {
      {
        using Array = T[8];
        std::shared_ptr ptr = std::make_shared<Array>();
        for (int i = 0; i < 8; ++i) {
          T* p = std::addressof(ptr[i]);
          assert(reinterpret_cast<std::uintptr_t>(p) % alignof(T) == 0);
        }
      }
      {
        using Array = T[8][3];
        std::shared_ptr ptr = std::make_shared<Array>();
        for (int i = 0; i < 8; ++i) {
          for (int j = 0; j < 3; ++j) {
            T* p = std::addressof(ptr[i][j]);
            assert(reinterpret_cast<std::uintptr_t>(p) % alignof(T) == 0);
          }
        }
      }
      {
        using Array = T[8][3][2];
        std::shared_ptr ptr = std::make_shared<Array>();
        for (int i = 0; i < 8; ++i) {
          for (int j = 0; j < 3; ++j) {
            for (int k = 0; k < 2; ++k) {
              T* p = std::addressof(ptr[i][j][k]);
              assert(reinterpret_cast<std::uintptr_t>(p) % alignof(T) == 0);
            }
          }
        }
      }
    };

    struct Empty { };
    check_alignment.operator()<Empty>();
    check_alignment.operator()<OverAligned>();
    check_alignment.operator()<MaxAligned>();

    // test non corner cases as well while we're at it
    struct Foo { int i; char c; };
    check_alignment.operator()<int>();
    check_alignment.operator()<Foo>();
  }

  // Make sure that we destroy all the elements constructed so far when an exception
  // is thrown. Also make sure that we do it in reverse order of construction.
#ifndef TEST_HAS_NO_EXCEPTIONS
  {
    struct Sentinel : ThrowOnConstruction, DestroyInReverseOrder { };

    // Without passing an initial value
    {
      using Array = Sentinel[8];
      for (int i = 0; i < 8; ++i) {
        ThrowOnConstruction::throw_after(i);
        DestroyInReverseOrder::reset();
        try {
          std::shared_ptr<Array> ptr = std::make_shared<Array>();
          assert(false);
        } catch (ThrowOnConstruction::exception const&) {
          assert(DestroyInReverseOrder::alive() == 0);
        }
      }
    }
    {
      using Array = Sentinel[8][3];
      for (int i = 0; i < 8 * 3; ++i) {
        ThrowOnConstruction::throw_after(i);
        DestroyInReverseOrder::reset();
        try {
          std::shared_ptr<Array> ptr = std::make_shared<Array>();
          assert(false);
        } catch (ThrowOnConstruction::exception const&) {
          assert(DestroyInReverseOrder::alive() == 0);
        }
      }
    }
    {
      using Array = Sentinel[8][3][2];
      for (int i = 0; i < 8 * 3 * 2; ++i) {
        ThrowOnConstruction::throw_after(i);
        DestroyInReverseOrder::reset();
        try {
          std::shared_ptr<Array> ptr = std::make_shared<Array>();
          assert(false);
        } catch (ThrowOnConstruction::exception const&) {
          assert(DestroyInReverseOrder::alive() == 0);
        }
      }
    }

    // Passing an initial value
    {
      using Array = Sentinel[8];
      for (int i = 0; i < 8; ++i) {
        DestroyInReverseOrder::reset();
        ThrowOnConstruction::reset();
        Sentinel init;
        ThrowOnConstruction::throw_after(i);
        try {
          std::shared_ptr<Array> ptr = std::make_shared<Array>(init);
          assert(false);
        } catch (ThrowOnConstruction::exception const&) {
          assert(DestroyInReverseOrder::alive() == 1);
        }
      }
    }
    {
      using Array = Sentinel[8][3];
      for (int i = 0; i < 8 * 3; ++i) {
        DestroyInReverseOrder::reset();
        ThrowOnConstruction::reset();
        Sentinel init[3] = {};
        ThrowOnConstruction::throw_after(i);
        try {
          std::shared_ptr<Array> ptr = std::make_shared<Array>(init);
          assert(false);
        } catch (ThrowOnConstruction::exception const&) {
          assert(DestroyInReverseOrder::alive() == 3);
        }
      }
    }
    {
      using Array = Sentinel[8][3][2];
      for (int i = 0; i < 8 * 3 * 2; ++i) {
        DestroyInReverseOrder::reset();
        ThrowOnConstruction::reset();
        Sentinel init[3][2] = {};
        ThrowOnConstruction::throw_after(i);
        try {
          std::shared_ptr<Array> ptr = std::make_shared<Array>(init);
          assert(false);
        } catch (ThrowOnConstruction::exception const&) {
          assert(DestroyInReverseOrder::alive() == 3 * 2);
        }
      }
    }
  }
#endif // TEST_HAS_NO_EXCEPTIONS

  // Make sure the version without an initialization argument works even for non-movable types
  {
    using Array = NonMovable[8][3];
    std::shared_ptr<Array> ptr = std::make_shared<Array>();
    (void)ptr;
  }

  // Make sure std::make_shared handles badly-behaved types properly
  {
    using Array = operator_hijacker[3];
    std::shared_ptr<Array> p1 = std::make_shared<Array>();
    std::shared_ptr<Array> p2 = std::make_shared<Array>(operator_hijacker());
    assert(p1 != nullptr);
    assert(p2 != nullptr);
  }

  // Check that we SFINAE-away for invalid arguments
  {
    struct T { };
    static_assert( CanMakeShared<T[8]>);
    static_assert( CanMakeShared<T[8], T>);
    static_assert(!CanMakeShared<T[8], T, int>); // too many arguments
  }

  return 0;
}
