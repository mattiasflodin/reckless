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
#include <reckless/detail/lockless_cv.hpp>

#if defined(__linux__)

#include <linux/futex.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <time.h>
#include <sys/time.h>

namespace reckless {
namespace detail {

namespace {

int sys_futex(void *addr1, int op, int val1, struct timespec const *timeout,
        void *addr2, int val3)
{
    return syscall(SYS_futex, addr1, op, val1, timeout, addr2, val3);
}

}   // anonymous namespace

void lockless_cv::notify_all()
{
    atomic_fetch_add_release(&notify_count_, 1);
    sys_futex(&notify_count_, FUTEX_WAKE_PRIVATE, 0x7fffffff,
        nullptr, nullptr, 0);
}

void lockless_cv::wait(unsigned expected_notify_count)
{
    sys_futex(&notify_count_, FUTEX_WAIT_PRIVATE, expected_notify_count,
        nullptr, 0, 0);
}

void lockless_cv::wait(unsigned expected_notify_count, unsigned milliseconds)
{
    struct timespec timeout = {0, 0};
    timeout.tv_sec = milliseconds/1000;
    timeout.tv_nsec = static_cast<long>(milliseconds%1000)*1000000;

    sys_futex(&notify_count_, FUTEX_WAIT_PRIVATE,
        expected_notify_count, &timeout, 0, 0);
}

}   // namespace detail
}   // namespace reckless

#elif defined(_WIN32)

#include <Windows.h>

namespace reckless {
namespace detail {

void lockless_cv::notify_all()
{
    atomic_fetch_add_release(&notify_count_, 1);
    WakeByAddressAll(&notify_count_);
}

void lockless_cv::wait(unsigned expected_notify_count)
{
    WaitOnAddress(&notify_count_, &expected_notify_count, sizeof(unsigned), INFINITE);
}

void lockless_cv::wait(unsigned expected_notify_count, unsigned milliseconds)
{
    WaitOnAddress(&notify_count_, &expected_notify_count, sizeof(unsigned), milliseconds);
}

}   // namespace detail
}   // namespace reckless

#else
    static_assert(false, "lockless_cv is not implemented for this platform")
#endif
