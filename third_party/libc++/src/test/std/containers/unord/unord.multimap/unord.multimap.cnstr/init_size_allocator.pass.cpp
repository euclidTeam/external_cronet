//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11

// <unordered_map>

// template <class Key, class T, class Hash = hash<Key>, class Pred = equal_to<Key>,
//           class Alloc = allocator<pair<const Key, T>>>
// class unordered_multimap

// unordered_multimap(initializer_list<value_type> il, size_type n,
//                    const allocator_type& alloc);

#include <unordered_map>
#include <string>
#include <set>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <iterator>

#include "test_macros.h"
#include "../../../check_consecutive.h"
#include "../../../test_compare.h"
#include "../../../test_hash.h"
#include "test_allocator.h"
#include "min_allocator.h"

template <class Allocator>
void test(const Allocator& alloc)
{
    typedef std::unordered_multimap<int, std::string,
                                    test_hash<int>,
                                    test_equal_to<int>,
                                    Allocator> C;
    typedef std::pair<int, std::string> P;
    C c({
            P(1, "one"),
            P(2, "two"),
            P(3, "three"),
            P(4, "four"),
            P(1, "four"),
            P(2, "four")
        },
        7,
        alloc);

    LIBCPP_ASSERT(c.bucket_count() == 7);
    assert(c.size() == 6);

    typedef std::pair<typename C::const_iterator, typename C::const_iterator> Eq;
    Eq eq = c.equal_range(1);
    assert(std::distance(eq.first, eq.second) == 2);
    std::multiset<std::string> s;
    s.insert("one");
    s.insert("four");
    CheckConsecutiveKeys<typename C::const_iterator>(c.find(1), c.end(), 1, s);

    eq = c.equal_range(2);
    assert(std::distance(eq.first, eq.second) == 2);
    s.insert("two");
    s.insert("four");
    CheckConsecutiveKeys<typename C::const_iterator>(c.find(2), c.end(), 2, s);

    eq = c.equal_range(3);
    assert(std::distance(eq.first, eq.second) == 1);
    typename C::const_iterator i = eq.first;
    assert(i->first == 3);
    assert(i->second == "three");

    eq = c.equal_range(4);
    assert(std::distance(eq.first, eq.second) == 1);
    i = eq.first;
    assert(i->first == 4);
    assert(i->second == "four");

    assert(static_cast<std::size_t>(std::distance(c.begin(), c.end())) == c.size());
    assert(std::fabs(c.load_factor() - (float)c.size() / c.bucket_count()) < FLT_EPSILON);
    assert(c.max_load_factor() == 1);
    assert(c.hash_function() == test_hash<int>());
    assert(c.key_eq() == test_equal_to<int>());
    assert(c.get_allocator() == alloc);
}

int main(int, char**)
{
    typedef std::pair<const int, std::string> V;
    test(test_allocator<V>(10));
    test(min_allocator<V>());
    test(explicit_allocator<V>());

    return 0;
}
