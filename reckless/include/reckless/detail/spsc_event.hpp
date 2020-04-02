/* This file is part of reckless logging
 * Copyright 2015-2020 Mattias Flodin <git@codepentry.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef RECKLESS_DETAIL_SPSC_EVENT_HPP
#define RECKLESS_DETAIL_SPSC_EVENT_HPP
#include <atomic>
#include <system_error>

#if defined(__linux__)
#include <linux/futex.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <time.h>
#include <sys/time.h>

namespace reckless {
namespace detail {
// TODO what makes this single-producer single-consumer? We aren't using it
// that way, since multiple threads are both waiting and signaling, so it
// should really be mpmc_event.
class spsc_event {
public:
    spsc_event() : signal_(0)
    {
    }

    void signal()
    {
        // TODO possible performance improvement: If we know nobody is waiting,
        // then we don't need to signal the futex.
        atomic_exchange_explicit(&signal_, 1, std::memory_order_release);
        sys_futex(&signal_, FUTEX_WAKE, 1, nullptr, nullptr, 0);
    }

    void wait()
    {
        int signal = atomic_exchange_explicit(&signal_, 0, std::memory_order_acquire);
        while(not signal) {
            // TODO may be beneficial to just put a cpu pause here before
            // reading the value again, i.e. a spinlock kind of construction.
            sys_futex(&signal_, FUTEX_WAIT, 0, nullptr, nullptr, 0);
            signal = atomic_exchange_explicit(&signal_, 0, std::memory_order_acquire);
        }
    }

    bool wait(unsigned milliseconds)
    {
        int signal = atomic_exchange_explicit(&signal_, 0, std::memory_order_acquire);
        if(signal)
            return true;
        struct timespec start;
        if(0 != clock_gettime(CLOCK_MONOTONIC, &start))
            throw std::system_error(errno, std::system_category());

        unsigned elapsed_ms = 0;
        struct timespec timeout = {0, 0};

        do {
            unsigned remaining_ms = milliseconds - elapsed_ms;
            timeout.tv_sec = remaining_ms/1000;
            timeout.tv_nsec = static_cast<long>(remaining_ms%1000)*1000000;
            sys_futex(&signal_, FUTEX_WAIT, 0, &timeout, nullptr, 0);
            signal = atomic_exchange_explicit(&signal_, 0, std::memory_order_acquire);
            if(signal)
                return true;

            struct timespec now;
            if(0 != clock_gettime(CLOCK_MONOTONIC, &now))
                throw std::system_error(errno, std::system_category());

            elapsed_ms = 1000*static_cast<int>(now.tv_sec - start.tv_sec);
            elapsed_ms += (now.tv_nsec - start.tv_nsec)/1000000;
        } while(elapsed_ms < milliseconds);
        return false;
    }

private:
    int sys_futex(void *addr1, int op, int val1, struct timespec const *timeout,
            void *addr2, int val3)
    {
        return syscall(SYS_futex, addr1, op, val1, timeout, addr2, val3);
    }

    int atomic_exchange_explicit(int* pvalue, int new_value, std::memory_order)
    {
        // FIXME this can be replaced with __atomic builtins.
        int res = new_value;
        asm volatile("xchg %0, %1\n\t"
                : "+r"(res), "+m"(*pvalue)
                :
                : "memory", "cc");
        return res;
    }

    int signal_;
};

#elif defined(_WIN32)
#if !defined(_MSC_VER)
static_assert(false, "spsc_event is not implemented for Windows on this compiler")
#endif

#include <cassert>

namespace reckless {
namespace detail {

extern "C" {
    unsigned long __stdcall WaitForSingleObject(void* hHandle, unsigned long dwMilliseconds);
    int __stdcall SetEvent(void* hEvent);
}

class spsc_event {
public:
    spsc_event();
    ~spsc_event();
    void signal()
    {
        SetEvent(handle_);
    }

    void wait()
    {
        bool success = wait(0xFFFFFFFF);    // INFINITE
        (void)success;
        assert(success);
    }

    bool wait(unsigned milliseconds)
    {
        auto result = WaitForSingleObject(handle_, milliseconds);
        assert(result == 0 || result == 0x102L);    // WAIT_OBJECT_0 || WAIT_TIMEOUT
        return result == 0;
    }

private:
    void* handle_;
};

#else
static_assert(false, "spsc_event is not implemented for this OS")

#endif

}   // namespace detail
}   // namespace reckless

#endif // RECKLESS_DETAIL_SPSC_EVENT_HPP
