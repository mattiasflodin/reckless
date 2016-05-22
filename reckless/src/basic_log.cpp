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

void destroy_thread_input_buffer(void* p)
{
    using reckless::detail::thread_input_buffer;
    thread_input_buffer* pbuffer = static_cast<thread_input_buffer*>(p);
    thread_input_buffer::destroy(pbuffer);
}

}   // anonymous namespace

char const* writer_error::what() const noexcept
{
    return "writer error";
}

basic_log::basic_log()
{
    if(0 != pthread_key_create(&thread_input_buffer_key_, &destroy_thread_input_buffer))
        throw std::bad_alloc();
}

basic_log::basic_log(writer* pwriter,
        std::size_t output_buffer_max_capacity,
        std::size_t shared_input_queue_size,
        std::size_t thread_input_buffer_size)
{
    if(0 != pthread_key_create(&thread_input_buffer_key_, &destroy_thread_input_buffer))
        throw std::bad_alloc();
    open(pwriter, output_buffer_max_capacity, shared_input_queue_size, thread_input_buffer_size);
}

basic_log::~basic_log()
{
    halt_on_panic_flush();
    if(is_open()) {
        std::error_code error;
        close(error);
    }
    auto result = pthread_key_delete(thread_input_buffer_key_);
    assert(result == 0);
}

void basic_log::open(writer* pwriter,
        std::size_t output_buffer_max_capacity,
        std::size_t shared_input_queue_size,
        std::size_t thread_input_buffer_size)
{
    // The typical disk block size these days is 4 KiB (see
    // https://en.wikipedia.org/wiki/Advanced_Format). We'll make it twice
    // that just in case it grows larger, and to hide some of the effects of
    // misalignment.
    unsigned const ASSUMED_DISK_SECTOR_SIZE = 8192;
    if(output_buffer_max_capacity == 0 or shared_input_queue_size == 0
            or thread_input_buffer_size == 0)
    {
        if(output_buffer_max_capacity == 0)
            output_buffer_max_capacity = ASSUMED_DISK_SECTOR_SIZE;
        // TODO is it right to just do g_page_size/sizeof(commit_extent) if we want
        // the buffer to use up one page? There's likely more overhead in the
        // buffer.
        std::size_t page_size = detail::get_page_size();
        if(shared_input_queue_size == 0)
            shared_input_queue_size = page_size / sizeof(detail::commit_extent);
        if(thread_input_buffer_size == 0)
            thread_input_buffer_size = ASSUMED_DISK_SECTOR_SIZE;
    }
    assert(!is_open());
    reset_shared_input_queue(shared_input_queue_size);
    thread_input_buffer_size_ = thread_input_buffer_size;
    output_buffer::reset(pwriter, output_buffer_max_capacity);
    output_thread_ = std::thread(std::mem_fn(&basic_log::output_worker), this);
}

void basic_log::close(std::error_code& ec) noexcept
{
    using namespace detail;
    assert(is_open());

    send_log_entries_blind({nullptr, nullptr});
    shared_input_queue_full_event_.signal();

    // We're going to assume that join() will not throw here, since all the
    // documented error conditions would be the result of a bug.
    output_thread_.join();
    assert(shared_input_queue_->empty());
    
    output_buffer::reset();
    thread_input_buffer_size_ = 0;
    shared_input_queue_ = std::experimental::nullopt;
    assert(!is_open());

    if(error_flag_.load(std::memory_order_acquire))
        ec = error_code_;
    else
        ec.clear();
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
        static void format(output_buffer* poutput, spsc_event* pevent,
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
    spsc_event event;
    write<formatter>(&event, &ec);
    shared_input_queue_full_event_.signal();
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
    panic_flush_ = true;
    shared_input_queue_full_event_.signal();
    panic_flush_done_event_.wait();
}

void basic_log::output_worker()
{
    using namespace detail;

#ifdef RECKLESS_DEBUG
    output_worker_native_handle_ = pthread_self();
#endif
    pthread_setname_np(pthread_self(), "reckless output worker");

    // TODO if possible we should call signal_input_consumed() whenever the
    // output buffer is flushed, so threads aren't kept waiting indefinitely if
    // the queue never clears up.
    std::vector<thread_input_buffer*> touched_input_buffers;
    touched_input_buffers.reserve(std::max(
                8u, 2*std::thread::hardware_concurrency()));

    while(true) {
        commit_extent ce;
        if(unlikely(not shared_input_queue_->pop(ce))) {
            // The shared input queue is empty, no new log messages to process.
            // It's not exactly unlikely that this will happen, but we want
            // this to be a "cold path" so that we perform best when there is a
            // lot of load.
            if(unlikely(panic_flush_)) {
                // We are in panic-flush mode and the queue is empty. That
                // means we are done.
                on_panic_flush_done();  // never returns
            }

            // For all messages that we processed during this iteration, notify
            // the originating threads that we have cleared their thread-local
            // queues (they may be waiting for the queue to clear up). Then
            // flush the output buffer.
            shared_input_consumed_event_.signal();
            for(thread_input_buffer* pinput_buffer : touched_input_buffers)
                pinput_buffer->signal_input_consumed();
            for(thread_input_buffer* pbuffer : touched_input_buffers)
                pbuffer->input_consumed_flag = false;
            touched_input_buffers.clear();
            if(output_buffer::has_complete_frame())
                flush_output_buffer();

            // Loop until something comes in to the shared queue. Since logger
            // threads do not normally signal any event (unless the queue fills
            // up), we have to poll the queue. We use an exponential back off
            // to not use too many CPU cycles and allow other threads to run,
            // but we don't wait for more than one second.
            unsigned wait_time_ms = 0;
            while(not shared_input_queue_->pop(ce)) {
                shared_input_consumed_event_.signal();
                shared_input_queue_full_event_.wait(wait_time_ms);
                wait_time_ms += std::max(1u, wait_time_ms/4u);
                wait_time_ms = std::min(wait_time_ms, 1000u);

                // If the flush above failed due to a temporary error then there
                // may still be data lingering, so we'll need to keep trying to
                // flush while we wait for more data.
                if(output_buffer::has_complete_frame())
                    flush_output_buffer();
            }
        }

        if(not ce.pinput_buffer) {
            // A null input-buffer pointer is the termination signal. Finish up
            // and exit the worker thread.
            if(unlikely(panic_flush_))
                on_panic_flush_done();  // never returns
            if(output_buffer::has_complete_frame()) {
                // Can't do much if there is a flush error here since we are
                // shutting down. The error code will be checked by close() when
                // the worker thread finishes.
                flush_output_buffer();
            }
            return;
        }

        // For this thread-local input buffer, run through all available log
        // entries and invoke their formatter to generate data for the output
        // buffer.
        char* pinput_start = ce.pinput_buffer->input_start();
        while(pinput_start != ce.pcommit_end) {
            auto pdispatch = *reinterpret_cast<formatter_dispatch_function_t**>(pinput_start);
            if(WRAPAROUND_MARKER == pdispatch) {
                pinput_start = ce.pinput_buffer->wraparound();
                pdispatch = *reinterpret_cast<formatter_dispatch_function_t**>(pinput_start);
            }

            std::size_t frame_size;

            try {
                // Call formatter.
                frame_size = (*pdispatch)(invoke_formatter, static_cast<output_buffer*>(this), pinput_start);
            } catch(flush_error const&) {
                // A flush error occurs here if there was not enough space in
                // the output buffer and it had to be flushed to make room, but
                // the flush failed. This means that we lose the frame.
                std::type_info const* pti;
                frame_size = (*pdispatch)(get_typeid, &pti, nullptr);
                output_buffer::lost_frame();
            } catch(...) {
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

            output_buffer::frame_end();
            pinput_start = ce.pinput_buffer->discard_input_frame(frame_size);

            // Mark the originating thread's input buffer as having been
            // accessed, i.e. if the thread is waiting for more space to become
            // available then it should be notified once we are done processing
            // log entries. We don't do this in panic-flush mode since it will
            // unnecessarily risk a heap allocation.
            if(likely(not panic_flush_)) {
                if(not ce.pinput_buffer->input_consumed_flag) {
                    touched_input_buffers.push_back(ce.pinput_buffer);
                    ce.pinput_buffer->input_consumed_flag = true;
                }
            }
        }
    }
}

void basic_log::send_log_entries(detail::commit_extent const& ce)
{
    halt_on_panic_flush();
    if(likely(not error_flag_.load(std::memory_order_acquire)))
        send_log_entries_blind(ce);
    else
        throw writer_error(error_code_);
}


void basic_log::send_log_entries_blind(detail::commit_extent const& ce)
{
    if(likely(shared_input_queue_->push(ce)))
        return;

    do {
        shared_input_queue_full_event_.signal();
        // TODO: This wait releases *one* thread. If another thread is also
        // waiting to push data to the shared queue and it is selected for
        // release, then this thread will "miss" the signal and stay here
        // waiting. However, we are guaranteed to eventually be released (modulo
        // starvation issues) because if another thread gets the ticket then it
        // will push an entry to the queue. When the queue gets cleared then
        // this thread will get another shot at receiving the event.
        //
        // Eventually this might be changed into an alternative event object
        // that releases all threads, but it's not clear if that will actually
        // improve performance, since it causes "thundering herd"-like issues
        // instead. All threads will then try to push events on the shared queue
        // simultaneously which could lead to cache-line ping pong and hurt
        // performance (but maybe queue entries could be made so they end up in
        // different cache lines?)
        shared_input_consumed_event_.wait();
    } while(not shared_input_queue_->push(ce));
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

void basic_log::halt_on_panic_flush()
{
    if(unlikely(panic_flush_))
    {
        // Another visitor! Stay a while; stay forever!  When we are in panic
        // mode because of a crash, we want to flush all the log entries that
        // were produced before the crash, but no more than that. We could put a
        // watermark in the queue and whatnot, but really, when there's a crash
        // in progress we will just confound the problem if we keep trying to
        // push stuff on the queue and muck about with the heap.  Better to just
        // suspend anything that tries, and wait for the kill.
        while(true)
            sleep(3600);
    }
}

void basic_log::reset_shared_input_queue(std::size_t node_count)
{
    // boost's lockfree queue has no move constructor and provides no reserve()
    // function when you use fixed_sized policy. So we'll just explicitly
    // destroy the current queue and create a new one with the desired size. We
    // are guaranteed that the queue is nullopt since this is only called when
    // opening the log.
    assert(!shared_input_queue_);
    shared_input_queue_.emplace(node_count);
}

detail::thread_input_buffer* basic_log::init_input_buffer()
{
    auto p = detail::thread_input_buffer::create(thread_input_buffer_size_);
    try {
        int result = pthread_setspecific(thread_input_buffer_key_, p);
        if(likely(result == 0))
            return p;
        else if(result == ENOMEM)
            throw std::bad_alloc();
        else
            throw std::system_error(result, std::system_category());
    } catch(...) {
        detail::thread_input_buffer::destroy(p);
        throw;
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
