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
#include <reckless/basic_log.hpp>

#include <vector>
#include <ciso646>

#include <unistd.h>     // sleep

using reckless::detail::likely;
using reckless::detail::unlikely;

namespace reckless {

namespace {
// Since logger threads do not normally signal any event (unless the queue
// fills up), we have to poll the input buffer. We use an exponential back off
// to not use too many CPU cycles and allow other threads to run, but we don't
// wait for more than one second.
unsigned max_input_buffer_poll_period_ms = 1000u;
unsigned input_buffer_poll_period_inverse_growth_factor = 4;
}   // anonymous namespace

char const* writer_error::what() const noexcept
{
    return "writer error";
}

basic_log::basic_log()
{
}

basic_log::~basic_log()
{
    if(is_open()) {
        std::error_code error;
        close2(error);
    }
}

void basic_log::open2(writer* pwriter)
{
    open2(pwriter, 0, 0);
}

void basic_log::open2(writer* pwriter,
    std::size_t input_buffer_capacity,
    std::size_t output_buffer_capacity)
{
    assert(!is_open());

    // The typical disk block size these days is 4 KiB (see
    // https://en.wikipedia.org/wiki/Advanced_Format). We'll make it twice
    // that just in case it grows larger, and to hide some of the effects of
    // misalignment.
    unsigned const assumed_disk_sector_size = 8192;

    if(input_buffer_capacity == 0)
        input_buffer_capacity = detail::get_page_size();
    if(output_buffer_capacity == 0)
        output_buffer_capacity =  assumed_disk_sector_size;

    input_buffer_.reserve(input_buffer_capacity);
    output_buffer::reset(pwriter, output_buffer_capacity);
    output_thread_ = std::thread(std::mem_fn(&basic_log::output_worker2), this);
}

void basic_log::close2(std::error_code& ec) noexcept
{
    using namespace detail;
    assert(is_open());

    frame_header* pframe = push_input_frame_blind(RECKLESS_CACHE_LINE_SIZE);
    atomic_store_relaxed(&pframe->status, frame_status::shutdown_marker);
    input_buffer_full_event_.signal();

    // We're going to assume that join() will not throw here, since all the
    // documented error conditions would be the result of a bug.
    output_thread_.join();
    assert(input_buffer_.size() == 0);

    output_buffer::reset();
    input_buffer_.reserve(0);

    if(error_flag_.load(std::memory_order_acquire))
        ec = error_code_;
    else
        ec.clear();

    assert(!is_open());
}

void basic_log::close2()
{
    std::error_code error;
    close2(error);
    if(error)
        throw writer_error(error);
}

void basic_log::flush(std::error_code& ec)
{
    struct formatter {
        static void format(output_buffer* poutput, detail::spsc_event* pevent,
                std::error_code* perror)
        {
            // Need downcast to get access to protected members.
            auto const plog = static_cast<basic_log*>(poutput);
            try {
                if(plog->has_complete_frame())
                    poutput->flush();
                perror->clear();
            } catch(flush_error const& e) {
                *perror = e.code();
            }
            pevent->signal();
        }
    };
    detail::spsc_event event;
    write2<formatter>(&event, &ec);
    input_buffer_full_event_.signal();
    event.wait();
}

void basic_log::flush()
{
    std::error_code error;
    basic_log::flush(error);
    if(error)
        throw writer_error(error);
}

void basic_log::panic_flush()
{
    using namespace detail;
    frame_header* pframe = push_input_frame_blind(RECKLESS_CACHE_LINE_SIZE);
    atomic_store_relaxed(&pframe->status, frame_status::panic_shutdown_marker);
    input_buffer_full_event_.signal();
    panic_flush_done_event_.wait();
}

#if defined(RECKLESS_NO_INLINE_INPUT_BUFFER)
RECKLESS_IMPLEMENT_PUSH_INPUT_FRAME()
#endif

detail::frame_header* basic_log::push_input_frame_slow_path(std::size_t size)
{
    using namespace detail;
    frame_header* pframe;
    do {
        input_buffer_full_event_.signal();
        // TODO: This wait releases *one* thread. If another thread is also
        // waiting to push data to the input buffer and it is selected for
        // release, then this thread will "miss" the signal and stay here
        // waiting. However, we are guaranteed to eventually be released
        // (modulo starvation issues) because if another thread gets the ticket
        // then it will push an entry to the queue. When the queue gets cleared
        // then this thread will get another shot at receiving the event.
        //
        // Eventually this might be changed into an alternative event object
        // that releases all threads, but it's not clear if that will actually
        // improve performance, since it causes "thundering herd"-like issues
        // instead. All threads will then try to push events on the shared queue
        // simultaneously which could lead to cache-line ping pong and hurt
        // performance (but maybe queue entries could be made so they end up in
        // different cache lines?)
        input_buffer_empty_event_.wait();
        pframe = static_cast<frame_header*>(input_buffer_.push(size));
    } while(!pframe);
    return pframe;
}

void basic_log::output_worker2()
{
    using namespace detail;

#ifdef RECKLESS_DEBUG
    output_worker_native_handle_ = pthread_self();
#endif
    pthread_setname_np(pthread_self(), "reckless output worker");

    frame_status status = frame_status::uninitialized;
    while(likely(status < frame_status::shutdown_marker)) {
        auto pframe = static_cast<char*>(input_buffer_.front());
        auto batch_size = wait_for_input();
        auto pbatch_end = pframe + batch_size;

        while(pframe != pbatch_end) {
            status = acquire_frame(pframe);

            std::size_t frame_size;
            if(likely(status == frame_status::initialized))
                frame_size = process_frame(pframe);
            else if(status < frame_status::shutdown_marker)
                frame_size = skip_frame(pframe);
            else
                frame_size = RECKLESS_CACHE_LINE_SIZE;

            release_frame(pframe, frame_size);
            pframe += frame_size;
        }
        input_buffer_.pop_release(batch_size);
    }

    if(status == frame_status::panic_shutdown_marker) {
        // We are in panic-flush mode and reached the shutdown marker. That
        // means we are done.
        on_panic_flush_done();  // never returns
    }

    if(output_buffer::has_complete_frame()) {
        // Can't do much if there is a flush error here since we are
        // shutting down. The error code will be checked by close() when
        // the worker thread finishes.
        // FIXME if the error policy is e.g. block then we may never leave here
        // and if it's ignore then the error won't be seen. Can we change the
        // policy during exit?
        flush_output_buffer();
    }
}

std::size_t basic_log::wait_for_input()
{
    auto size = input_buffer_.size();
    if(likely(size)) {
        // It's not exactly *likely* that there is input in the buffer, but we
        // want this to be a "hot path" so that we perform our best when there
        // is a lot of load.
        return size;
    }

    // Poll the input buffer until something comes in.
    unsigned wait_time_ms = 0;
    while(true) {
        // The output buffer is flushed at least once before checking for more
        // input. This makes sure that data gets sent to the writer immediately
        // whenever there's a pause in incoming log messages. If this flush
        // fails due to a temporary error then there may still be data
        // lingering, so we need to keep trying to flush with each iteration as
        // long as data remains in the output buffer.
        if(output_buffer::has_complete_frame()) {
            input_buffer_empty_event_.signal();
            flush_output_buffer();
            size = input_buffer_.size();
            if(size != 0)
                return size;
        }

        input_buffer_empty_event_.signal();
        input_buffer_full_event_.wait(wait_time_ms);
        size = input_buffer_.size();
        if(size != 0)
            return size;

        wait_time_ms += std::max(1u,
            wait_time_ms/input_buffer_poll_period_inverse_growth_factor);
        wait_time_ms = std::min(wait_time_ms, max_input_buffer_poll_period_ms);
    }
}

detail::frame_status basic_log::acquire_frame(void* pframe)
{
    using namespace detail;
    auto ppdispatch = static_cast<formatter_dispatch_function_t**>(pframe);
    auto pstatus = static_cast<frame_status*>(static_cast<void*>(ppdispatch+1));

    auto status = atomic_load_acquire(pstatus);
    if(likely(status != frame_status::uninitialized))
        return status;

    // Poll the frame status until it is no longer uninitialized. We don't need
    // to be as sophisticated as in wait_for_input and try to flush the output
    // buffer etc, since we know another thread is just in the process of
    // putting data in the frame. If we have to wait more than a millisecond
    // here then something has probably gone very wrong.
    unsigned wait_time_ms = 0;
    while(true) {
        input_buffer_full_event_.wait(wait_time_ms);
        status = atomic_load_acquire(pstatus);
        if(status != frame_status::uninitialized)
            return status;

        wait_time_ms += std::max(1u,
            wait_time_ms/input_buffer_poll_period_inverse_growth_factor);
        wait_time_ms = std::min(wait_time_ms, max_input_buffer_poll_period_ms);
    }
}

std::size_t basic_log::process_frame(void* pframe)
{
    using namespace detail;
    auto pdispatch = *static_cast<formatter_dispatch_function_t**>(pframe);

    try {
        auto frame_size = (*pdispatch)(invoke_formatter,
                static_cast<output_buffer*>(this), pframe);
        output_buffer::frame_end();
        return frame_size;
    } catch(flush_error const&) {
        // A flush error occurs here if there was not enough space in
        // the output buffer and it had to be flushed to make room, but
        // the flush failed. This means that we lose the frame.
        output_buffer::lost_frame();
        std::type_info const* pti;
        return (*pdispatch)(get_typeid, &pti, nullptr);
    } catch(...) {
        output_buffer::revert_frame();
        std::type_info const* pti;
        auto frame_size = (*pdispatch)(get_typeid, &pti, nullptr);
        std::lock_guard<std::mutex> lk(callback_mutex_);
        if(format_error_callback_) {
            try {
                format_error_callback_(this, std::current_exception(),
                        *pti);
            } catch(...) {
            }
        }
        return frame_size;
    }
}

std::size_t basic_log::skip_frame(void* pframe)
{
    using namespace detail;
    auto pdispatch = *static_cast<formatter_dispatch_function_t**>(pframe);
    std::type_info const* pti;
    return (*pdispatch)(get_typeid, &pti, nullptr);
}

void basic_log::release_frame(void* pframe, std::size_t frame_size)
{
    using namespace detail;
    for(std::size_t offset=0; offset!=frame_size;
            offset += RECKLESS_CACHE_LINE_SIZE)
    {
        void* psegment = static_cast<char*>(pframe) + offset;
        auto ppdispatch = static_cast<formatter_dispatch_function_t**>(
                psegment);
        auto pstatus = static_cast<frame_status*>(static_cast<void*>(ppdispatch+1));
        atomic_store_relaxed(pstatus, frame_status::uninitialized);
    }
}

void basic_log::flush_output_buffer()
{
    try {
        output_buffer::flush();
    } catch(flush_error const&) {
        // flush_error is only thrown to unwind the stack from
        // output_buffer::reserve()/flush() so that the current
        // formatting operation can be aborted. No need to do anything
        // at this point. The only time we care about flush_error is
        // when it happens during formatting of an input frame,
        // because then we need to account for a lost frame.
    }
}

void basic_log::on_panic_flush_done()
{
    if(output_buffer::has_complete_frame()) {
        // We get one chance to flush what remains in the output buffer. If it
        // fails now then we'll just have to live with that and crash.
        try {
            output_buffer::flush();
        } catch(...) {
        };
    }

    panic_flush_done_event_.signal();
    // Sleep and wait for death.
    while(true)
        sleep(3600);
}

}   // namespace reckless
