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
#ifndef RECKLESS_OUTPUT_BUFFER_HPP
#define RECKLESS_OUTPUT_BUFFER_HPP

#include "detail/branch_hints.hpp"
#include "detail/spsc_event.hpp"

#include <cstddef>  // size_t
#include <new>      // bad_alloc
#include <cstring>  // strlen, memcpy
#include <functional>   // function
#include <mutex>
#include <system_error> // system_error, error_code
#include <cassert>

namespace reckless {
class writer;
class output_buffer;

enum class error_policy {
    ignore,
    notify_on_recovery,
    block,
    fail_immediately
    // TODO: policy to invoke callback from background thread
};

// Thrown if output_buffer::reserve() is used to allocate more than can fit in
// the output buffer during formatting of a single input frame. If this happens
// then you need to either enlarge the output buffer or reduce the amount of
// data produced by your formatter.
class excessive_output_by_frame : public std::bad_alloc {
public:
    char const* what() const noexcept override;
};

// Thrown if output_buffer::flush fails. This inherits from bad_alloc because
// it makes sense in the context where the formatter calls
// output_buffer::reserve(), and a flush intended to create more space in the
// buffer fails. In that context, it is essentially an allocation error. The
// formatter may catch this if it wishes, but it is expected that most will
// just let it fall through to the worker thread, where it will be dealt with
// appropriately.
//
// TODO we need to make flush() private, clients can't go around calling it in
// the formatter because if it fails and the exception reaches the worker
// thread then we'll end up assuming the buffer ran out of space and something
// was lost.
class flush_error : public std::bad_alloc {
public:
    flush_error(std::error_code const& error_code) :
        error_code_(error_code)
    {
    }
    char const* what() const noexcept override;
    std::error_code const& code() const
    {
        return error_code_;
    }

private:
    std::error_code error_code_;
};

using writer_error_callback_t = std::function<void (output_buffer* pbuffer, std::error_code ec, unsigned lost_record_count)>;

class output_buffer {
public:
    output_buffer();
    // TODO hide functions that are not relevant to the client, e.g. move
    // assignment, empty(), flush etc?
    output_buffer(writer* pwriter, std::size_t max_capacity);
    ~output_buffer();

    char* reserve(std::size_t size)
    {
        std::size_t remaining = pbuffer_end_ - pcommit_end_;
        if(detail::likely(size <= remaining))
            return pcommit_end_;
        else
            return reserve_slow_path(size);
    }

    void commit(std::size_t size)
    {
        pcommit_end_ += size;
    }

    void write(void const* buf, std::size_t count);

    void write(char const* s)
    {
        write(s, std::strlen(s));
    }

    void write(char c)
    {
        char* p = reserve(1);
        *p = c;
        commit(1);
    }

protected:
    void reset() noexcept;
    // throw bad_alloc if unable to malloc() the buffer.
    void reset(writer* pwriter, std::size_t max_capacity);

    // Put a watermark indicating where the last complete output frame ends.
    void frame_end()
    {
        pframe_end_ = pcommit_end_;
    }
    // Notify that an input frame was lost because of a flush error.
    void lost_frame()
    {
        ++lost_input_frames_;
        // Undo everything that has been written during the current input frame.
        pcommit_end_ = pframe_end_;
    }

    bool has_complete_frame() const
    {
        return pframe_end_ != pbuffer_;
    }

    // Need to make flush() public because of g++ bug 66957
    // <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66957>
#ifdef __GNUC__
public:
#endif
    void flush();
#ifdef __GNUC__
protected:
#endif

    // Must not write to the log since it may cause a deadlock. May not throw
    // exceptions.
    void writer_error_callback(writer_error_callback_t callback = writer_error_callback_t())
    {
        std::lock_guard<std::mutex> lk(writer_error_callback_mutex_);
        writer_error_callback_ = move(callback);
    }

    error_policy temporary_error_policy() const
    {
        return temporary_error_policy_.load(std::memory_order_relaxed);
    }

    void temporary_error_policy(error_policy ep)
    {
        temporary_error_policy_.store(ep, std::memory_order_relaxed);
    }

    error_policy permanent_error_policy() const
    {
        return permanent_error_policy_.load(std::memory_order_relaxed);
    }

    void permanent_error_policy(error_policy ep)
    {
        assert(ep != error_policy::notify_on_recovery
                && ep != error_policy::block);
        permanent_error_policy_.store(ep, std::memory_order_relaxed);
    }

    spsc_event shared_input_queue_full_event_; // FIXME rename to something that indicates this is used for all "notifications" to the worker thread

    std::atomic<error_policy> temporary_error_policy_{error_policy::ignore};
    std::atomic<error_policy> permanent_error_policy_{error_policy::fail_immediately};
    std::error_code error_code_;    // error code for error state
    std::atomic_bool error_flag_{false};   // error state
    std::atomic_bool panic_flush_{false};

private:
    output_buffer(output_buffer const&) = delete;
    output_buffer& operator=(output_buffer const&) = delete;

    char* reserve_slow_path(std::size_t size);

    writer* pwriter_ = nullptr;
    char* pbuffer_ = nullptr;
    char* pframe_end_ = nullptr;
    char* pcommit_end_ = nullptr;
    char* pbuffer_end_ = nullptr;
    unsigned lost_input_frames_ = 0;
    std::error_code initial_error_;         // Keeps track of the first error that caused lost_input_frames_ to become non-zero.
    std::mutex writer_error_callback_mutex_;
    writer_error_callback_t writer_error_callback_;
};

}

#endif  // RECKLESS_OUTPUT_BUFFER_HPP
