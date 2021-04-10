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
#include <reckless/detail/trace_log.hpp>

#include <reckless/basic_log.hpp>
#include <reckless/detail/platform.hpp>

#include <vector>
#include <algorithm>    // max, min
#include <thread>       // sleep_for
#include <sstream>      // ostringstream
#include <chrono>       // hours

using reckless::detail::likely;

namespace reckless {

namespace {
// Since logger threads do not normally signal any event (unless the queue
// fills up), we have to poll the input buffer. We use an exponential back off
// to not use too many CPU cycles and allow other threads to run, but we don't
// wait for more than one second.
unsigned max_input_buffer_poll_period_ms = 1000u;
unsigned input_buffer_poll_period_inverse_growth_factor = 4;

#ifdef RECKLESS_ENABLE_TRACE_LOG
struct output_worker_start_event :
    public detail::timestamped_trace_event
{
    std::string format() const
    {
        return timestamped_trace_event::format() + " output_worker_start";
    }
};

struct wait_for_input_start_event :
    public detail::timestamped_trace_event
{
    std::string format() const
    {
        return timestamped_trace_event::format() + " wait_for_input start";
    }
};

struct wait_for_input_finish_event :
    public detail::timestamped_trace_event
{
    std::string format() const
    {
        return timestamped_trace_event::format() + " wait_for_input finish";
    }
};

struct input_buffer_full_wait_start_event : public detail::timestamped_trace_event {
    std::string format() const
    {
        return timestamped_trace_event::format() +
            " input_buffer_full_wait start";
    }
};

struct input_buffer_full_wait_finish_event : public detail::timestamped_trace_event {
    std::string format() const
    {
        return timestamped_trace_event::format()
            + " input_buffer_full_wait finish";
    }
};

struct process_batch_start_event : public detail::timestamped_trace_event {
    std::size_t batch_size;

    process_batch_start_event(std::size_t size) :
        batch_size(size)
    {
    }

    std::string format() const
    {
        std::ostringstream ostr;
        ostr << timestamped_trace_event::format() << " process_batch start " <<
            batch_size;
        return ostr.str();
    }
};

struct process_batch_finish_event : public detail::timestamped_trace_event {
    std::string format() const
    {
        return timestamped_trace_event::format()
            + " process_batch finish";
    }
};

struct process_frame_start_event : public detail::timestamped_trace_event {
    std::string format() const
    {
        std::ostringstream ostr;
        ostr << timestamped_trace_event::format() << " process_frame start ";
        return ostr.str();
    }
};

struct process_frame_finish_event : public detail::timestamped_trace_event {
    std::string format() const
    {
        return timestamped_trace_event::format()
            + " process_frame finish";
    }
};


struct format_frame_start_event : public detail::timestamped_trace_event {
    std::string format() const
    {
        return timestamped_trace_event::format() +
            " format_frame start";
    }
};

struct format_frame_finish_event : public detail::timestamped_trace_event {
    std::string format() const
    {
        return timestamped_trace_event::format()
            + " format_frame finish";
    }
};

#endif  // RECKLESS_ENABLE_TRACE_LOG

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

    // We used to use the page size for input buffer capacity.
    // However, after introducing the new ring buffer for Windows
    // support, it is impossible to have a size less than 64 KiB on
    // Windows due to granularity constraints. Additionally, since we
    // are storing the entire input frame in this buffer and have
    // stricter alignment requests, we need a larger buffer to fit a
    // reasonable amount of frames.  So, for better performance and
    // more similar behavior between Unix/Windows, we set this to 64
    // KiB.
    //
    // The downside to this fairly big size is that during load the
    // latency of log calls may become erratic, as a large queue means
    // there will be a long wait for it to flush if it happens to fill
    // up.
    if(input_buffer_capacity == 0)
        input_buffer_capacity = 64*1024;

    // We always write the contents of the output buffer whenever we
    // have depleted the input queue, but if it fills up before that
    // happens then we have to flush all finished frames to disk.
    // Ideally this should not happen, as benchmarks have shown that
    // it reduces performance (perhaps due to the memmove() call
    // taking some time or maybe just because of additional
    // transitions to the kernel). On the other hand, if the caller
    // manages to maintain a balance between input rate and the rate
    // at which log lines are formatted, so that the input queue never
    // depletes and always has more content, then the output buffer
    // needs to have a maximum size or it will eventually eat all
    // available memory.
    //
    // The buffer currently has a fixed size and we set it based on an
    // assumption on how much space will be used if the entire input
    // queue is full of log entries. We assume an input frame will use
    // up one cache line and that a log line will be 80 bytes in size.
    if(output_buffer_capacity == 0) {
        auto assumed_count = (input_buffer_capacity+RECKLESS_CACHE_LINE_SIZE-1)/
            RECKLESS_CACHE_LINE_SIZE;
        output_buffer_capacity = assumed_count * 80;
    }

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
    while(true) {
        auto notify_count = input_buffer_empty_event_.notify_count();
        pframe = static_cast<frame_header*>(input_buffer_.push(size));
        error = atomic_load_acquire(&error_flag_);
        if (pframe != nullptr || error)
            break;

        atomic_increment_fetch_relaxed(&input_buffer_full_count_);
        input_buffer_full_event_.signal();
        RECKLESS_TRACE(input_buffer_full_wait_start_event);
        input_buffer_empty_event_.wait(notify_count);
        RECKLESS_TRACE(input_buffer_full_wait_finish_event);
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

    set_thread_name("reckless output worker");

    frame_status status = frame_status::uninitialized;
    while(likely(status < frame_status::shutdown_marker)) {
        auto batch_size = wait_for_input();

        atomic_store_relaxed(&input_buffer_high_watermark_,
            std::max(input_buffer_high_watermark_, batch_size));
        RECKLESS_TRACE(process_batch_start_event, batch_size);

        auto batch_start = input_buffer_.read_position();
        auto batch_end = batch_start + batch_size;
        auto read_position = batch_start;

        bool panic_flush = false;
        do
        {
            while(read_position != batch_end &&
                 likely(status < frame_status::shutdown_marker))
            {
                void* pframe = input_buffer_.address(read_position);
                status = acquire_frame(pframe);

                std::size_t frame_size;
                if(likely(status == frame_status::initialized))
                    frame_size = process_frame(pframe);
                else if(status == frame_status::failed_error_check)
                    frame_size = static_cast<frame_header*>(pframe)->frame_size;
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
                read_position += frame_size;
            }
            assert(read_position == batch_end);

            // Return memory to the input buffer to be used by other threads,
            // but only if we are not in a panic-flush state.
            // See start_panic_flush() for more information.
            panic_flush = atomic_load_relaxed(&panic_flush_);
            if(likely(!panic_flush)) {
                input_buffer_.pop_release(batch_size);
            } else {
                // As a consequence of not returning memory to the input buffer,
                // on the next batch iteration input_buffer_.front() is going
                // to return exactly the same position that we already
                // processed, meaning we will hang waiting for it to become
                // initialized. So instead of continuing normally we just update
                // the batch end to reflect the current size of the buffer
                // (which is going to end up equal to the full capacity of the
                // buffer), let pframe remain at its current position, and loop
                // around until we reach the panic_shutdown_marker frame.
                batch_size = input_buffer_.size();
                batch_end = batch_start + batch_size;
            }
        } while(unlikely(panic_flush));
        RECKLESS_TRACE(process_batch_finish_event);
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
    auto size = input_buffer_.size();
    if(likely(size != 0)) {
        // It's not exactly *likely* that there is input in the buffer, but we
        // want this to be a "hot path" so that we perform our best when there
        // is a lot of load.
        return size;
    }

    RECKLESS_TRACE(wait_for_input_start_event);
    // Poll the input buffer until something comes in.
    unsigned wait_time_ms = 0;
    while(true) {
        input_buffer_empty_event_.notify_all();

        // The output buffer is flushed at least once before waiting for more
        // input. This makes sure that data gets sent to the writer immediately
        // whenever there's a pause in incoming log messages. If this flush
        // fails due to a temporary error then there may still be data
        // lingering, so we need to keep trying to flush with each iteration as
        // long as data remains in the output buffer.
        if(output_buffer::has_complete_frame()) {
            flush_output_buffer();
            // The flush acts as a wait, so check the input buffer
            // again before waiting on the event.
            size = input_buffer_.size();
            if(size != 0)
                break;
        }

        input_buffer_full_event_.wait(wait_time_ms);
        size = input_buffer_.size();
        if(size != 0)
            break;

        wait_time_ms += std::max(1u,
            wait_time_ms/input_buffer_poll_period_inverse_growth_factor);
        wait_time_ms = std::min(wait_time_ms, max_input_buffer_poll_period_ms);
    }
    RECKLESS_TRACE(wait_for_input_finish_event);
    return size;
}

detail::frame_status basic_log::acquire_frame(void* pframe)
{
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
    //RECKLESS_TRACE(process_frame_start_event);
    using namespace detail;
    auto pheader = static_cast<frame_header*>(pframe);
    auto pdispatch = pheader->pdispatch_function;

    std::size_t frame_size;
    try {
        frame_size = (*pdispatch)(invoke_formatter,
                static_cast<output_buffer*>(this), pframe);
        output_buffer::frame_end();
    } catch(flush_error const&) {
        // A flush error occurs here if there was not enough space in
        // the output buffer and it had to be flushed to make room, but
        // the flush failed. This means that we lose the frame.
        output_buffer::lost_frame();
        std::type_info const* pti;
        frame_size = (*pdispatch)(get_typeid, &pti, nullptr);
    } catch(...) {
        output_buffer::revert_frame();
        std::type_info const* pti;
        frame_size = (*pdispatch)(get_typeid, &pti, nullptr);
        std::lock_guard<std::mutex> lk(callback_mutex_);
        if(format_error_callback_) {
            try {
                format_error_callback_(this, std::current_exception(),
                        *pti);
            } catch(...) {
            }
        }
    }

    //RECKLESS_TRACE(process_frame_finish_event);
    return frame_size;
}

std::size_t basic_log::skip_frame(void* pframe)
{
    using namespace detail;
    auto pdispatch = static_cast<frame_header*>(pframe)->pdispatch_function;
    std::type_info const* pti;
    return (*pdispatch)(get_typeid, &pti, nullptr);
}

void basic_log::clear_frame(void* pframe, std::size_t frame_size)
{
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
        std::this_thread::sleep_for(std::chrono::hours(1));
}

}   // namespace reckless
