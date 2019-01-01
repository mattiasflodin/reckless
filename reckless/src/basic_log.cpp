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
#include <reckless/detail/platform.hpp>
#include <reckless/detail/trace_log.hpp>

#include <vector>
#include <algorithm>    // max, min
#include <thread>       // sleep_for
#include <chrono>       // hours

#include <performance_log.hpp>

using reckless::detail::likely;

namespace reckless {

namespace {
// Since logger threads do not normally signal any event (unless the queue
// fills up), we have to poll the input buffer. We use an exponential back off
// to not use too many CPU cycles and allow other threads to run, but we don't
// wait for more than one second.
unsigned max_input_buffer_poll_period_ms = 1000u;
unsigned input_buffer_poll_period_inverse_growth_factor = 4;

struct output_worker_start_event
{
    std::string format() const
    {
        return "output_worker_start";
    }
};

struct wait_for_input_event
{
    std::string format() const
    {
        return "wait_for_input";
    }
};

struct acquire_frame_event
{
    std::string format() const
    {
        return "acquire_frame";
    }
};

struct process_frame_event
{
    std::string format() const
    {
        return "process_frame";
    }
};

struct skip_frame_event
{
    std::string format() const
    {
        return "skip_frame";
    }
};

struct clear_frame_event
{
    std::string format() const
    {
        return "clear_frame";
    }
};

struct flush_output_buffer_start_event
{
    std::string format() const
    {
        return "flush_output_buffer_start";
    }
};

struct flush_output_buffer_finish_event
{
    std::string format() const
    {
        return "flush_output_buffer_finish";
    }
};

struct wait_input_buffer_full_event
{
    std::string format() const
    {
        return "wait_input_buffer_full";
    }
};

struct wait_input_buffer_full_done_event
{
    std::string format() const
    {
        return "wait_input_buffer_full_done";
    }
};

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
        close(error);
    }
}

void basic_log::open(writer* pwriter)
{
    open(pwriter, 0, 0);
}

void basic_log::open(writer* pwriter,
    std::size_t input_buffer_capacity,
    std::size_t output_buffer_capacity)
{
    assert(!is_open());

    // The typical disk block size these days is 4 KiB (see
    // https://en.wikipedia.org/wiki/Advanced_Format). We'll make it twice
    // that just in case it grows larger, and to hide some of the effects of
    // misalignment.
    unsigned const assumed_disk_sector_size = 8192;

    // We used to use the page size for input buffer capacity. However, after
    // introducing the new ring buffer for Windows support, it is impossible to
    // have a size less than 64 KiB on Windows due to granularity constraints.
    // Additionally, since we are storing the entire input frame in this buffer
    // and have stricter alignment requests, we need a larger buffer to fit
    // a reasonable amount of frames.  So, for better performance and more
    // similar behavior between Unix/Windows, we set this to 64 KiB.
    if(input_buffer_capacity == 0)
        input_buffer_capacity = 64*1024;
    if(output_buffer_capacity == 0)
        output_buffer_capacity =  assumed_disk_sector_size;

    input_buffer_.reserve(input_buffer_capacity);
    output_buffer::reset(pwriter, output_buffer_capacity);
    output_thread_ = std::thread(std::mem_fn(&basic_log::output_worker), this);
}

void basic_log::close(std::error_code& ec) noexcept
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

    if(atomic_load_acquire(&error_flag_))
        ec = error_code_;
    else
        ec.clear();

    assert(!is_open());
}

void basic_log::close()
{
    std::error_code error;
    close(error);
    if(error)
        throw writer_error(error);
}

void basic_log::flush(std::error_code& ec)
{
    struct formatter {
        static void format(output_buffer* poutput, detail::spsc_event* pevent,
                std::error_code* perror)
        {
            // Need downcast to get access to protected members in
            // output_buffer.
            auto const plog = static_cast<basic_log*>(poutput);
            try {
                if(plog->has_complete_frame())
                    plog->output_buffer::flush();
                perror->clear();
            } catch(flush_error const& e) {
                *perror = e.code();
            }
            pevent->signal();
        }
    };
    detail::spsc_event event;
    write<formatter>(&event, &ec);
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

void basic_log::start_panic_flush()
{
    using namespace detail;
    frame_header* pframe = push_input_frame_blind(RECKLESS_CACHE_LINE_SIZE);
    // To reduce interference from other running threads that write to the log
    // during a panic flush, we set panic_flush_ = true. This stops the
    // background thread from releasing consumed memory to the buffer. Then we
    // call input_buffer_.deplete(), which will eat up all available memory
    // from the buffer, effectively preventing all other threads from pushing
    // more data to the buffer. Instead, they'll sit nicely and block until
    // the flush is done.

    // I'm uncertain about the theoretical correctness of this, but at least
    // I'm pretty sure it will work on x86/x64, since Intel has a strongly-
    // ordered cache coherency model. The thing we don't want to happen is that
    // the store to panic_flush_ (below) becomes visible to the background
    // thread before the input frame is allocated (above). Since panic_flush_
    // signals to the background thread that it should no longer free up space
    // in the buffer, that could mean that if the buffer is full then the
    // allocation above will block indefinitely.
    //
    // It seems to me that what I need is a memory ordering that is the opposite
    // of memory_order_release: store operations *below* the barrier may not be
    // moved above it.
    //
    // Anyway, until we run into problems in practice, I think it will be
    // sufficient to tell the *compiler* that it may not move anything
    // above this barrier.
    std::atomic_signal_fence(std::memory_order_seq_cst);
    atomic_store_relaxed(&panic_flush_, true);
    input_buffer_.deplete();

    atomic_store_release(&pframe->status, frame_status::panic_shutdown_marker);
    input_buffer_full_event_.signal();
}

void basic_log::await_panic_flush()
{
    panic_flush_done_event_.wait();
}

bool basic_log::await_panic_flush(unsigned int milliseconds)
{
    return panic_flush_done_event_.wait(milliseconds);
}

detail::frame_header* basic_log::push_input_frame_slow_path(
    detail::frame_header* pframe, bool error, std::size_t size)
{
    using namespace detail;
    while(pframe == nullptr && !error) {
        input_buffer_full_event_.signal();
        // TODO: This wait releases *one* thread. If another thread is also
        // waiting to push data to the input buffer and it is selected for
        // release, then this thread will miss the signal and stay here
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
        error = atomic_load_acquire(&error_flag_);
    }
    if(!error) {
        return pframe;
    } else {
        if(pframe) {
            pframe->frame_size = size;
            atomic_store_release(&pframe->status, frame_status::failed_error_check);
        }
        throw writer_error(error_code_);
    }
}

void basic_log::output_worker()
{
    using namespace detail;
    RECKLESS_TRACE(output_worker_start_event);

    // This code is compiled into a static library, whereas write() is inlined
    // and compiled in the client application's environment.
    // We don't know if the client application defines RECKLESS_DEBUG or not,
    // so we have to assume it does and make sure to set the worker-thread ID,
    // lest write() will misbehave.
#if defined(__unix__)
    output_worker_native_handle_ = pthread_self();
#elif defined(_WIN32)
    output_worker_native_id_ = GetCurrentThreadId();
#endif
    //performance_log::rdtscp_cpuid_clock::bind_cpu(1);

    set_thread_name("reckless output worker");

    frame_status status = frame_status::uninitialized;
    while(likely(status < frame_status::shutdown_marker)) {
        auto batch_size = wait_for_input();
        auto pbatch_start = static_cast<char*>(input_buffer_.front());
        auto pbatch_end = pbatch_start + batch_size;
        auto pframe = pbatch_start;

        bool panic_flush = false;
        do
        {
            while(pframe != pbatch_end &&
                 likely(status < frame_status::shutdown_marker))
            {
                status = acquire_frame(pframe);

                std::size_t frame_size;
                if(likely(status == frame_status::initialized))
                    frame_size = process_frame(pframe);
                else if(status == frame_status::failed_error_check)
                    frame_size = char_cast<frame_header*>(pframe)->frame_size;
                else if(status == frame_status::failed_initialization)
                    frame_size = skip_frame(pframe);
                else if(status == frame_status::shutdown_marker)
                    frame_size = RECKLESS_CACHE_LINE_SIZE;
                else {
                    assert(status == frame_status::panic_shutdown_marker);
                    // We are in panic-flush mode and reached the shutdown marker. That
                    // means we are done.
                    on_panic_flush_done();  // never returns
                }

                clear_frame(pframe, frame_size);
                pframe += frame_size;
            }
            assert(pframe == pbatch_end);

            // Return memory to the input buffer to be used by other threads,
            // but only if we are not in a panic-flush state.
            // See start_panic_flush() for more information.
            panic_flush = atomic_load_relaxed(&panic_flush_);
            if(likely(!panic_flush)) {
                input_buffer_.pop_release(batch_size);
            } else {
                // As a consequence of not returning memory to the input buffer,
                // on the next batch iteration input_buffer_.front() is going
                // to return exactly the same frame address that we already
                // processed, meaning we will hang waiting for it to become
                // initialized. So instead of continuing normally we just update
                // the batch end to reflect the current size of the buffer
                // (which is going to end up equal to the full capacity of the
                // buffer), let pframe remain at its current position, and loop
                // around until we reach the panic_shutdown_marker frame.
                batch_size = input_buffer_.size();
                pbatch_end = pbatch_start + batch_size;
            }
        } while(unlikely(panic_flush));
    }

    if(output_buffer::has_complete_frame()) {
        // Can't do much here if there is a flush error here since we are
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
    RECKLESS_TRACE(wait_for_input_event);
    auto size = input_buffer_.size();
    if(likely(size != 0)) {
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
        RECKLESS_TRACE(wait_input_buffer_full_event);
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
    RECKLESS_TRACE(acquire_frame_event);
    using namespace detail;
    auto pheader = static_cast<frame_header*>(pframe);
    auto status = atomic_load_acquire(&pheader->status);
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
        status = atomic_load_acquire(&pheader->status);
        if(status != frame_status::uninitialized)
            return status;

        wait_time_ms += std::max(1u,
            wait_time_ms/input_buffer_poll_period_inverse_growth_factor);
        wait_time_ms = std::min(wait_time_ms, max_input_buffer_poll_period_ms);
    }
}

std::size_t basic_log::process_frame(void* pframe)
{
    RECKLESS_TRACE(process_frame_event);
    using namespace detail;
    auto pheader = static_cast<frame_header*>(pframe);
    auto pdispatch = pheader->pdispatch_function;

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
    RECKLESS_TRACE(skip_frame_event);
    using namespace detail;
    auto pdispatch = static_cast<frame_header*>(pframe)->pdispatch_function;
    std::type_info const* pti;
    return (*pdispatch)(get_typeid, &pti, nullptr);
}

void basic_log::clear_frame(void* pframe, std::size_t frame_size)
{
    RECKLESS_TRACE(clear_frame_event);
    using namespace detail;
    auto pcframe = static_cast<char*>(pframe);
    for(std::size_t offset=0; offset!=frame_size;
            offset += RECKLESS_CACHE_LINE_SIZE)
    {
        auto pheader = char_cast<frame_header*>(pcframe + offset);
        atomic_store_relaxed(&pheader->status, frame_status::uninitialized);
    }
}

void basic_log::flush_output_buffer()
{
    RECKLESS_TRACE(flush_output_buffer_start_event);
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
    RECKLESS_TRACE(flush_output_buffer_finish_event);
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
        std::this_thread::sleep_for(std::chrono::hours(1));
}

}   // namespace reckless
