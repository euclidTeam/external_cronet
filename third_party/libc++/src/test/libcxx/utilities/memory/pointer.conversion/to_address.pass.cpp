//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// <memory>

// template <class T> constexpr T* __to_address(T* p) noexcept;
// template <class Ptr> constexpr auto __to_address(const Ptr& p) noexcept;

#include <memory>
#include <cassert>
#include <utility>

#include "test_macros.h"

struct Irrelevant;

struct P1 {
    using element_type = Irrelevant;
    TEST_CONSTEXPR explicit P1(int *p) : p_(p) { }
    TEST_CONSTEXPR int *operator->() const { return p_; }
    int *p_;
};

struct P2 {
    using element_type = Irrelevant;
    TEST_CONSTEXPR explicit P2(int *p) : p_(p) { }
    TEST_CONSTEXPR P1 operator->() const { return p_; }
    P1 p_;
};

struct P3 {
    TEST_CONSTEXPR explicit P3(int *p) : p_(p) { }
    int *p_;
};

template<>
struct std::pointer_traits<P3> {
    static TEST_CONSTEXPR int *to_address(const P3& p) { return p.p_; }
};

struct P4 {
    TEST_CONSTEXPR explicit P4(int *p) : p_(p) { }
    int *operator->() const;  // should never be called
    int *p_;
};

template<>
struct std::pointer_traits<P4> {
    static TEST_CONSTEXPR int *to_address(const P4& p) { return p.p_; }
};

struct P5 {
    using element_type = Irrelevant;
    int const* const& operator->() const;
};

struct P6 {};

template<>
struct std::pointer_traits<P6> {
    static int const* const& to_address(const P6&);
};

// Taken from a build breakage caused in Clang
namespace P7 {
    template<typename T> struct CanProxy;
    template<typename T>
    struct CanQual {
        CanProxy<T> operator->() const { return CanProxy<T>(); }
    };
    template<typename T>
    struct CanProxy {
        const CanProxy<T> *operator->() const { return nullptr; }
    };
} // namespace P7

namespace P8 {
    template<class T>
    struct FancyPtrA {
        using element_type = Irrelevant;
        T *p_;
        TEST_CONSTEXPR FancyPtrA(T *p) : p_(p) {}
        T& operator*() const;
        TEST_CONSTEXPR T *operator->() const { return p_; }
    };
    template<class T>
    struct FancyPtrB {
        T *p_;
        TEST_CONSTEXPR FancyPtrB(T *p) : p_(p) {}
        T& operator*() const;
    };
} // namespace P8

template<class T>
struct std::pointer_traits<P8::FancyPtrB<T> > {
    static TEST_CONSTEXPR T *to_address(const P8::FancyPtrB<T>& p) { return p.p_; }
};

struct Incomplete;
template<class T> struct Holder { T t; };


TEST_CONSTEXPR_CXX14 bool test() {
    int i = 0;
    ASSERT_NOEXCEPT(std::__to_address(&i));
    assert(std::__to_address(&i) == &i);
    P1 p1(&i);
    ASSERT_NOEXCEPT(std::__to_address(p1));
    assert(std::__to_address(p1) == &i);
    P2 p2(&i);
    ASSERT_NOEXCEPT(std::__to_address(p2));
    assert(std::__to_address(p2) == &i);
    P3 p3(&i);
    ASSERT_NOEXCEPT(std::__to_address(p3));
    assert(std::__to_address(p3) == &i);
    P4 p4(&i);
    ASSERT_NOEXCEPT(std::__to_address(p4));
    assert(std::__to_address(p4) == &i);

    ASSERT_SAME_TYPE(decltype(std::__to_address(std::declval<int const*>())), int const*);
    ASSERT_SAME_TYPE(decltype(std::__to_address(std::declval<P5>())), int const*);
    ASSERT_SAME_TYPE(decltype(std::__to_address(std::declval<P6>())), int const*);

    P7::CanQual<int>* p7 = nullptr;
    assert(std::__to_address(p7) == nullptr);
    ASSERT_SAME_TYPE(decltype(std::__to_address(p7)), P7::CanQual<int>*);

    Holder<Incomplete> *p8_nil = nullptr;  // for C++03 compatibility
    P8::FancyPtrA<Holder<Incomplete> > p8a = p8_nil;
    assert(std::__to_address(p8a) == p8_nil);
    ASSERT_SAME_TYPE(decltype(std::__to_address(p8a)), decltype(p8_nil));

    P8::FancyPtrB<Holder<Incomplete> > p8b = p8_nil;
    assert(std::__to_address(p8b) == p8_nil);
    ASSERT_SAME_TYPE(decltype(std::__to_address(p8b)), decltype(p8_nil));

    int p9[2] = {};
    assert(std::__to_address(p9) == p9);
    ASSERT_SAME_TYPE(decltype(std::__to_address(p9)), int*);

    const int p10[2] = {};
    assert(std::__to_address(p10) == p10);
    ASSERT_SAME_TYPE(decltype(std::__to_address(p10)), const int*);

    return true;
}

int main(int, char**) {
    test();
#if TEST_STD_VER >= 14
    static_assert(test(), "");
#endif
    return 0;
}
