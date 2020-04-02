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
#ifndef RECKLESS_DETAIL_LOCKLESS_CV_HPP
#define RECKLESS_DETAIL_LOCKLESS_CV_HPP

#include <reckless/detail/platform.hpp>

namespace reckless {
namespace detail {

// This provides the functionality of a condition variable for lockless code.
// Normally a condition variable uses a mutex to ensure that the condition
// doesn't have time to change between checking the condition and waiting. This
// prevents the risk of race conditions that occurs when using events, whereby
//
// 1. Thread 1 checks the condition and it is not in the desired state.
// 2. Thread 1 is preempted by thread 2, which changes the condition and
//    signals the event.
// 3. Thread, 3 which is waiting on the event starts executing and resets the
//    event.
// 4. Thread 1 is scheduled again and begins to wait for the event even though
//    the condition is now already in the desired state. It might stall here
//    indefinitely waiting for the event to be signaled again.
//
// Since we don't want to take a lock before checking the condition, we provide
// an API to read the change-notification count instead. The intended usage is
// 1. Call notify_count() and save the count.
// 2. Check the condition of the shared data.
// 3. If the shared data does not fulfill the desired condition, call wait()
//    and provide the notify count. The wait will occur only if the notify
//    count is still holds the expected value. If not, the call completes
//    immediately and execution restarts at 1.
//
// The read in step 1 has acquire semantics, guaranteeing that any writes to the
// shared data done by another thread will occur before it notifies any waiters.
// The notify_all() operation increases the count using corresponding release
// semantics.
//
// Note that, just like with ordinary condition variables, a completed wait()
// does not guarantee that the desired condition is fulfilled.
class lockless_cv {
public:
    lockless_cv() : notify_count_(0) {}

    unsigned notify_count() const
    {
        return atomic_load_acquire(&notify_count_);
    }

    void notify_all();
    void wait(unsigned expected_notify_count);
    void wait(unsigned expected_notify_count, unsigned milliseconds);

private:
    unsigned notify_count_;
};

}   // namespace detail
}   // namespace reckless

#endif // RECKLESS_DETAIL_LOCKLESS_CV_HPP
