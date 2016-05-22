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
#ifndef RECKLESS_DETAIL_INPUT_HPP
#define RECKLESS_DETAIL_INPUT_HPP

#include <reckless/detail/spsc_event.hpp>
#include <reckless/output_buffer.hpp>
#include <reckless/detail/utility.hpp>    // is_power_of_two

namespace reckless {

class basic_log;

namespace detail {

enum dispatch_operation {
    invoke_formatter,
    get_typeid
};
typedef std::size_t formatter_dispatch_function_t(dispatch_operation, void*, void*);
// TODO these checks need to be done at runtime now
//static_assert(alignof(dispatch_function_t*) <= RECKLESS_FRAME_ALIGNMENT,
//        "RECKLESS_FRAME_ALIGNMENT must at least match that of a function pointer");
//// We need the requirement below to ensure that, after alignment, there
//// will either be 0 free bytes available in the circular buffer, or
//// enough to fit a dispatch pointer. This simplifies the code a bit.
//static_assert(sizeof(dispatch_function_t*) <= FRAME_ALIGNMENT,
//        "RECKLESS_FRAME_ALIGNMENT must at least match the size of a function pointer");
formatter_dispatch_function_t* const WRAPAROUND_MARKER = reinterpret_cast<
    formatter_dispatch_function_t*>(0);

class thread_input_buffer {
public:
    static thread_input_buffer* create(std::size_t size)
    {
        std::size_t full_size = sizeof(thread_input_buffer) + size - sizeof(formatter_dispatch_function_t*);
        char* buf = new char[full_size];
        try {
            return new (buf) thread_input_buffer(size);
        } catch(...) {
            delete [] buf;
            throw;
        }
    }

    static void destroy(thread_input_buffer* p)
    {
        p->~thread_input_buffer();
        delete [] static_cast<char*>(static_cast<void*>(p));
    }

    // return marker to be used for revert_allocation()
    char const* allocation_marker() const
    {
        return pinput_end_;
    }

    // return pointer to allocated input frame, move input_end() forward.
    char* allocate_input_frame(std::size_t size);

    // revert allocation made by allocate_input_frame. Can only be used to
    // revert the last allocation attempt made, and must use marker returned by
    // allocation_marker() right before the last allocation.
    void revert_allocation(char const* marker)
    {
        pinput_end_ = const_cast<char*>(marker);
    }
    // return pointer to following input frame
    char* discard_input_frame(std::size_t size);
    char* wraparound();
    char* input_start() const
    {
        return pinput_start_.load(std::memory_order_relaxed);
    }
    char* input_end() const
    {
        return pinput_end_;
    }
    void signal_input_consumed();

    bool input_consumed_flag;

private:
    thread_input_buffer(std::size_t size);
    ~thread_input_buffer();

    char* advance_frame_pointer(char* p, std::size_t distance);
    void wait_input_consumed();
    bool is_aligned(void* p) const
    {
        return (reinterpret_cast<std::uintptr_t>(p) & frame_alignment_mask()) == 0;
    }
    bool is_aligned(std::size_t v) const
    {
        return (v & frame_alignment_mask()) == 0;
    }

    static constexpr std::uintptr_t frame_alignment_mask()
    {
        static_assert(is_power_of_two(alignof(formatter_dispatch_function_t*)), "expected sizeof function pointer to be power of two");
        return alignof(formatter_dispatch_function_t*)-1;
    }

    char* buffer_start()
    {
        return static_cast<char*>(static_cast<void*>(&buffer_start_));
    }

    spsc_event input_consumed_event_;
    std::size_t size_;                // number of chars in buffer

    std::atomic<char*> pinput_start_; // moved forward by output thread, read by logger::write (to determine free space left)
    char* pinput_end_;                // moved forward by logger::write, never read by anyone else
    formatter_dispatch_function_t* buffer_start_;
};

struct commit_extent {
    thread_input_buffer* pinput_buffer;
    char* pcommit_end;
};

}
}

#endif  // RECKLESS_DETAIL_INPUT_HPP
