/* This file is part of reckless logging
 * Copyright 2015, 2016 Mattias Flodin <git@codepentry.com>
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
#include <reckless/detail/thread_input_buffer.hpp>
#include <reckless/detail/utility.hpp>
#include <cassert>

reckless::detail::thread_input_buffer::thread_input_buffer(std::size_t size) :
    input_consumed_flag(false),
    size_(size),
    pinput_start_(buffer_start()),
    pinput_end_(buffer_start())
{
}

reckless::detail::thread_input_buffer::~thread_input_buffer()
{
    // FIXME I don't think we are properly waiting until the output
    // worker has cleared this from touched_input_buffers. Which means
    // it can call signal_input_consumed on a dangling pointer. We need
    // to either get rid of touched_input_buffer or make sure we wait
    // for the right thing here.
    //
    // Actually, even if we don't have touched_input_buffers, there is a race
    // between the call to discard_input_frame() and signal_input_consumed().
    // If this dtor runs between them, then signal_input_consumed will be
    // called on a dangling pointer.
 
    // Wait for the output thread to consume all the contents of the buffer
    // before we release it. Both write() and wait_input_consumed should
    // create full memory barriers, so no need for strict memory ordering in
    // this load.
    while(pinput_start_.load(std::memory_order_relaxed) != pinput_end_)
        wait_input_consumed();
}

char* reckless::detail::thread_input_buffer::discard_input_frame(std::size_t size)
{
    auto mask = frame_alignment_mask();
    size = (size + mask) & ~mask;
    // We can use relaxed memory ordering everywhere here because there is
    // nothing being written of interest that the pointer update makes visible;
    // all it does is *discard* data, not provide any new data (besides,
    // signaling the event is likely to create a full memory barrier anyway).
    auto p = pinput_start_.load(std::memory_order_relaxed);
    p = advance_frame_pointer(p, size);
    pinput_start_.store(p, std::memory_order_relaxed);
    return p;
}

char* reckless::detail::thread_input_buffer::wraparound()
{
#ifndef NDEBUG
    auto p = pinput_start_.load(std::memory_order_relaxed);
    auto marker = *reinterpret_cast<formatter_dispatch_function_t**>(p);
    assert(WRAPAROUND_MARKER == marker);
#endif
    pinput_start_.store(buffer_start(), std::memory_order_relaxed);
    return buffer_start();
}

// Moves an input-buffer pointer forward by the given distance while
// maintaining the invariant that:
//
// * p is aligned by FRAME_ALIGNMENT
// * p never points at the end of the buffer; it always wraps around to the
//   beginning of the circular buffer.
//
// The distance must never be so great that the pointer moves *past* the end of
// the buffer. To do so would be an error in our context, since no input frame
// is allowed to be discontinuous.
char* reckless::detail::thread_input_buffer::advance_frame_pointer(char* p, std::size_t distance)
{
    assert(is_aligned(p));
    assert(is_aligned(distance));
    p += distance;
    assert(size_ >= static_cast<std::size_t>(p - buffer_start()));
    auto remaining = size_ - (p - buffer_start());
    if(remaining == 0)
        p = buffer_start();
    return p;
}

// TODO inline this function? Maybe not considering that fixme.
void reckless::detail::thread_input_buffer::wait_input_consumed()
{
    // FIXME we need to think about what to do here, should we signal
    // g_shared_input_queue_full_event to force the output thread to wake up?
    // We probably should, or we could sit here for a full second.
    input_consumed_event_.wait();
}

void reckless::detail::thread_input_buffer::signal_input_consumed()
{
    input_consumed_event_.signal();
}

char* reckless::detail::thread_input_buffer::allocate_input_frame(std::size_t size)
{
    // Conceptually, we have the invariant that
    //   pinput_start_ <= pinput_end_,
    // and the memory area after pinput_end is free for us to use for
    // allocating a frame. However, the fact that it's a circular buffer means
    // that:
    // 
    // * The area after pinput_end is actually non-contiguous, wraps around
    //   at the end of the buffer and ends at pinput_start.
    //   
    // * Except, when pinput_end itself has fallen over the right edge and we
    //   have the case pinput_end <= pinput_start. Then the *used* memory is
    //   non-contiguous, and the free memory is contiguous (it still starts at
    //   pinput_end and ends at pinput_start modulo circular buffer size).
    //   
    // All of this code is easier to understand by simply drawing the buffer on
    // a paper than by reading the comment text.
    auto mask = frame_alignment_mask();
    size = (size + mask) & ~mask;

    // We can't write a frame that is larger than the entire capacity of the
    // input buffer. If you hit this assert then you either need to write a
    // smaller log entry, or you need to make the input buffer larger.
    assert(size < size_-1);
 
    while(true) {
        auto pinput_end = pinput_end_;
        assert(static_cast<std::size_t>(pinput_end - buffer_start()) < size_);
        assert(is_aligned(pinput_end));

        // Even if we get an "old" value for pinput_start_ here, that's OK
        // because other threads will never cause the amount of available
        // buffer space to shrink. So either there is enough buffer space and
        // we're done, or there isn't and we'll wait for an input-consumption
        // event which creates a full memory barrier and hence gives us an
        // updated value for pinput_start_. So memory_order_relaxed should be
        // fine here.
        auto pinput_start = pinput_start_.load(std::memory_order_relaxed);
        std::ptrdiff_t free = pinput_start - pinput_end;
        if(free == 0) {
            // If free == 0 then pinput_start == pinput_end, i.e. the entire
            // buffer is unused. But pinput_start may still point to the middle
            // of the buffer, making the free space appear non-contiguous. If
            // neither segment contains enough space for the input frame then
            // we might end up waiting for space to become available forever
            // (c.f. handling of non-contiguous case below), since there is no
            // actual input data available for the worker thread to consume.
            //
            // This clause exists to handle that specific case. Because the
            // background thread currently has no data to consume, we know that
            // it is not going to touch pinput_start_. We can use this
            // situation to "rewind" the pointers to the beginning of the
            // buffer, to make the entire contiguous buffer available to us.
            //
            // The memory ordering can be relaxed because there are no
            // dependent writes. Also the background thread does not need to
            // see an updated value until the input frame has been put on the
            // shared queue, and doing so requires at least a release
            // synchronization.
            pinput_start = buffer_start();
            pinput_start_.store(pinput_start, std::memory_order_relaxed);
            pinput_end_ = advance_frame_pointer(pinput_start, size);
            return pinput_start;
        } else if(free > 0) {
            // Free space is contiguous.
            // Technically, there is enough room if size == free. But the
            // problem with using the free space in this situation is that when
            // we increase pinput_end_ by size, we end up with pinput_start_ ==
            // pinput_end_. Now, given that state, how do we know if the buffer
            // is completely filled or empty? So, it's easier to just check for
            // size < free instead of size <= free, and pretend we're out
            // of space if size == free. Same situation applies in the else
            // clause below.
            if(likely(static_cast<std::ptrdiff_t>(size) < free)) {
                pinput_end_ = advance_frame_pointer(pinput_end, size);
                return pinput_end;
            } else {
                // Not enough room. Wait for the output thread to consume some
                // input.
                wait_input_consumed();
            }
        } else {
            // Free space is non-contiguous.
            // TODO should we use an end pointer instead of a size_?
            std::size_t free1 = size_ - (pinput_end - buffer_start());
            if(likely(size < free1)) {
                // There's enough room in the first segment.
                pinput_end_ = advance_frame_pointer(pinput_end, size);
                return pinput_end;
            } else {
                std::size_t free2 = pinput_start - buffer_start();
                if(likely(size < free2)) {
                    // We don't have enough room for a continuous input frame
                    // in the first segment (at the end of the circular
                    // buffer), but there is enough room in the second segment
                    // (at the beginning of the buffer). To instruct the output
                    // thread to skip ahead to the second segment, we need to
                    // put a marker value at the current position. We're
                    // supposed to be guaranteed enough room for the wraparound
                    // marker because frame alignment is at least the size of
                    // the marker.
                    *reinterpret_cast<formatter_dispatch_function_t**>(pinput_end_) =
                        WRAPAROUND_MARKER;
                    pinput_end_ = advance_frame_pointer(buffer_start(), size);
                    return buffer_start();
                } else {
                    // Not enough room. Wait for the output thread to consume
                    // some input.
                    wait_input_consumed();
                }
            }
        }
    }
}

