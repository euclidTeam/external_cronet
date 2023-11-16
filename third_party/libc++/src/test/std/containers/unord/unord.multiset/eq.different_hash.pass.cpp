//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// <unordered_set>

// template <class Key, class T, class Hash, class Pred, class Alloc>
// bool
// operator==(const unordered_multiset<Key, T, Hash, Pred, Alloc>& x,
//            const unordered_multiset<Key, T, Hash, Pred, Alloc>& y);
//
// template <class Key, class T, class Hash, class Pred, class Alloc>
// bool
// operator!=(const unordered_multiset<Key, T, Hash, Pred, Alloc>& x,
//            const unordered_multiset<Key, T, Hash, Pred, Alloc>& y);

// Implements paper: http://wg21.link/p0809

#include <cassert>
#include <cstddef>
#include <functional>
#include <limits>
#include <unordered_set>

template <class T>
std::size_t hash_identity(T val) {
  return val;
}
template <class T>
std::size_t hash_neg(T val) {
  return std::numeric_limits<T>::max() - val;
}
template <class T>
std::size_t hash_scale(T val) {
  return val << 1;
}
template <class T>
std::size_t hash_even(T val) {
  return val & 1 ? 1 : 0;
}
template <class T>
std::size_t hash_same(T /*val*/) {
  return 1;
}

template <class T>
std::size_t hash_identity(T* val) {
  return *val;
}
template <class T>
std::size_t hash_neg(T* val) {
  return std::numeric_limits<T>::max() - *val;
}
template <class T>
std::size_t hash_scale(T* val) {
  return *val << 1;
}
template <class T>
std::size_t hash_even(T* val) {
  return *val & 1 ? 1 : 0;
}

template <class T, std::size_t N>
void test(T (&vals)[N]) {
  using Hash = std::size_t (*)(T);
  using C    = std::unordered_multiset<T, Hash, std::equal_to<T> >;

  C c1(std::begin(vals), std::end(vals), 0, hash_identity);
  C c2(std::begin(vals), std::end(vals), 0, hash_neg);
  C c3(std::begin(vals), std::end(vals), 0, hash_scale);
  C c4(std::begin(vals), std::end(vals), 0, hash_even);
  C c5(std::begin(vals), std::end(vals), 0, hash_same);

  assert(c1 == c1);
  assert(c1 == c2);
  assert(c1 == c3);
  assert(c1 == c4);
  assert(c1 == c5);

  assert(c2 == c1);
  assert(c2 == c2);
  assert(c2 == c3);
  assert(c2 == c4);
  assert(c2 == c5);

  assert(c3 == c1);
  assert(c3 == c2);
  assert(c3 == c3);
  assert(c3 == c4);
  assert(c3 == c5);

  assert(c4 == c1);
  assert(c4 == c2);
  assert(c4 == c3);
  assert(c4 == c4);
  assert(c4 == c5);

  assert(c5 == c1);
  assert(c5 == c2);
  assert(c5 == c3);
  assert(c5 == c4);
  assert(c5 == c5);
}

int main(int, char**) {
  {
    std::size_t vals[] = {
        // clang-format off
        1,
        2, 2,
        3, 3, 3,
        4, 4, 4, 4,
        5, 5, 5, 5, 5,
        6, 6, 6, 6, 6, 6,
        7, 7, 7, 7, 7, 7, 7,
        8, 8, 8, 8, 8, 8, 8, 8,
        9, 9, 9, 9, 9, 9, 9, 9, 9,
        10
        // clang-format on
    };
    test(vals);
  }
  {
    bool vals[] = {true, false};
    test(vals);
  }
  {
    char* vals[] = {(char*)("a"), (char*)("b"), (char*)("cde")};
    test(vals);
  }

  return 0;
}
