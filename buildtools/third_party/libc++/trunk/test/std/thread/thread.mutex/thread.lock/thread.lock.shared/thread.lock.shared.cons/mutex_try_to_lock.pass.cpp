//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// UNSUPPORTED: no-threads
// UNSUPPORTED: c++03, c++11
// ALLOW_RETRIES: 2

// XFAIL: availability-shared_mutex-missing

// <shared_mutex>

// template <class Mutex> class shared_lock;

// shared_lock(mutex_type& m, try_to_lock_t);

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

#include "make_test_thread.h"
#include "test_macros.h"

std::shared_timed_mutex m;

typedef std::chrono::system_clock Clock;
typedef Clock::time_point time_point;
typedef Clock::duration duration;
typedef std::chrono::milliseconds ms;
typedef std::chrono::nanoseconds ns;


// Thread sanitizer causes more overhead and will sometimes cause this test
// to fail. To prevent this we give Thread sanitizer more time to complete the
// test.
#if !defined(TEST_IS_EXECUTED_IN_A_SLOW_ENVIRONMENT)
ms Tolerance = ms(200);
#else
ms Tolerance = ms(200 * 5);
#endif

void f()
{
    time_point t0 = Clock::now();
    {
        std::shared_lock<std::shared_timed_mutex> lk(m, std::try_to_lock);
        assert(lk.owns_lock() == false);
    }
    {
        std::shared_lock<std::shared_timed_mutex> lk(m, std::try_to_lock);
        assert(lk.owns_lock() == false);
    }
    {
        std::shared_lock<std::shared_timed_mutex> lk(m, std::try_to_lock);
        assert(lk.owns_lock() == false);
    }
    while (true)
    {
        std::shared_lock<std::shared_timed_mutex> lk(m, std::try_to_lock);
        if (lk.owns_lock())
            break;
        std::this_thread::yield();
    }
    time_point t1 = Clock::now();
    ns d = t1 - t0 - ms(250);
    assert(d < Tolerance);  // within tolerance
}

int main(int, char**)
{
    m.lock();
    std::vector<std::thread> v;
    for (int i = 0; i < 5; ++i)
        v.push_back(support::make_test_thread(f));
    std::this_thread::sleep_for(ms(250));
    m.unlock();
    for (auto& t : v)
        t.join();

  return 0;
}
