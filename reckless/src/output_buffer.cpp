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
#include <reckless/output_buffer.hpp>
#include <reckless/writer.hpp>
#include <reckless/detail/utility.hpp>

#include <cstdlib>      // malloc, free
#include <cassert>
#include <sys/mman.h>   // madvise()

namespace reckless {

char const* excessive_output_by_frame::what() const noexcept
{
    return "excessive output by frame";
}

char const* flush_error::what() const noexcept
{
    return "flush error";
}

using detail::likely;
using detail::unlikely;

output_buffer::output_buffer()
{
}

output_buffer::output_buffer(writer* pwriter, std::size_t max_capacity)
{
    reset(pwriter, max_capacity);
}

void output_buffer::reset() noexcept
{
    assert(!panic_flush_);
    std::free(pbuffer_);
    pwriter_ = nullptr;
    pbuffer_ = nullptr;
    pcommit_end_ = nullptr;
    pbuffer_end_ = nullptr;
    lost_input_frames_ = 0;
}

void output_buffer::reset(writer* pwriter, std::size_t max_capacity)
{
    using namespace detail;
    auto pbuffer = static_cast<char*>(std::malloc(max_capacity));
    if(!pbuffer)
        throw std::bad_alloc();
    std::free(pbuffer_);
    pbuffer_ = pbuffer;

    pwriter_ = pwriter;
    pframe_end_ = pbuffer_;
    pcommit_end_ = pbuffer_;
    pbuffer_end_ = pbuffer_ + max_capacity;
    auto page = detail::get_page_size();
    madvise(pbuffer_ + page, max_capacity - page, MADV_DONTNEED);
}

output_buffer::~output_buffer()
{
    if(panic_flush_)
        return;
    std::free(pbuffer_);
}

// FIXME I think this code is wrong. Review and check it against the invariants
// regarding error state etc.
void output_buffer::write(void const* buf, std::size_t count)
{
    // TODO this could be smarter by writing from the client-provided
    // buffer instead of copying the data.
    auto const buffer_size = pbuffer_end_ - pbuffer_;
    
    char const* pinput = static_cast<char const*>(buf);
    auto remaining_input = count;
    auto available_buffer = static_cast<std::size_t>(pbuffer_end_ - pcommit_end_);
    while(unlikely(remaining_input > available_buffer)) {
        std::memcpy(pcommit_end_, pinput, available_buffer);
        pinput += available_buffer;
        remaining_input -= available_buffer;
        available_buffer = buffer_size;
        pcommit_end_ = pbuffer_end_;
        flush();
    }
    
    std::memcpy(pcommit_end_, pinput, remaining_input);
    pcommit_end_ += remaining_input;
}

void output_buffer::flush()
{
    // TODO keep track of a high watermark, i.e. max value of pcommit_end_.
    // Clear every second or some such. Use madvise to release unused memory.
    
    // TODO we must honor the return value of write here. Also, since
    // it's user-provided code we should handle exceptions. The same
    // goes for any calls to formatter functions.
 
    // If there is a temporary error for long enough that the buffer gets full
    // and we have to throw away data, but we resume writing later, then we do
    // not want to end up with half-written frames in the middle of the log
    // file. So, we only write data up until the end of the last complete input
    // frame.
    std::size_t remaining = pframe_end_ - pbuffer_;
    unsigned block_time_ms = 0;
    while(true) {
        std::error_code error;
        std::size_t written;
        if(remaining == 0)
            return;
        try {
            // FIXME the crash mentioned below happens if you have g_log as a
            // global object and have a writer with local scope (e.g. in
            // main()), *even if you do not write to the log after the writer
            // goes out of scope*, because there can be stuff lingering in the
            // async queue. This makes the error pretty obscure, and we should
            // guard against it. Perhaps by taking the writer as a shared_ptr,
            // or at least by leaving a huge warning in the documentation.

            // NOTE if you get a crash here, it could be because your log object has a
            // longer lifetime than the writer (i.e. the writer has been destroyed
            // already).
            written = pwriter_->write(pbuffer_, remaining, error);
        } catch(...) {
            // It is a fatal error for the writer to throw an exception,
            // because we can't tell how much data was written to the target
            // before the exception occurred. Errors should be reported via the
            // error code parameter.
            // TODO assign a more specific error code to this so the client can
            // know what went wrong.
            error.assign(writer::permanent_failure, writer::error_category());
            written = 0;
        }
        if(likely(!error))
            assert(written == remaining);   // A successful writer must write *all* data.
        else
            assert(written <= remaining);   // A failing writer may write no data, some data, or all data (but no more than that).

        // Discard the data that was written, preserve data that remains. We could
        // avoid this copy by using a circular buffer, but in practice it should be
        // rare to have any data remaining in the buffer. It only happens if the
        // buffer fills up entirely (forcing us to flush in the middle of a frame)
        // or if there is an error in the writer.
        std::size_t remaining_data = (pcommit_end_ - pbuffer_) - written;
        std::memmove(pbuffer_, pbuffer_+written, remaining_data);
        pframe_end_ -= written;
        pcommit_end_ -= written;

        if(likely(!error)) {
            error_code_.clear();
            error_flag_.store(false, std::memory_order_release);
            if(likely(!lost_input_frames_)) {
                return;
            } else {
                // Frames were discarded because of earlier errors in
                // notify_on_recovery mode. Now that the writer is working and
                // there is space in the buffer, we can notify the callback
                // function about lost frames.
                //
                // The callback may put additional data in the output buffer. To
                // ensure that we do not leave data hanging around indefinitely,
                // we need to make sure that the extra data is also written.
                // This is particularly important when we're flushing as part of
                // shutting down the logger, as we could lose output if we don't
                // flush all of it.
                //
                // To accomplish the additional writes we fall through after
                // invoking the callback, and allow control to flow to the top
                // of the while loop and issue another write.
                auto lif = lost_input_frames_;
                lost_input_frames_ = 0;
                writer_error_callback_t callback;
                {
                    std::lock_guard<std::mutex> lk(writer_error_callback_mutex_);
                    callback = writer_error_callback_;
                }
                if(callback) {
                    try {
                        callback(this, initial_error_, lif);
                    } catch(...) {
                        // It's an error for the callback to throw an exception.
                        assert(false);
                    }
                    initial_error_.clear();
                    frame_end();
                }
                remaining = pframe_end_ - pbuffer_; // Update byte-remaining count.
            }
        } else {
            error_policy ep;
            if(error == writer::temporary_failure)
                ep = temporary_error_policy_.load(std::memory_order_relaxed);
            else
                ep = permanent_error_policy_.load(std::memory_order_relaxed);

            switch(ep) {
            case error_policy::ignore:
                throw flush_error(error);
            case error_policy::notify_on_recovery:
                // We will notify the client about this once the writer
                // starts working again.
                if(!initial_error_)
                    initial_error_ = error;
                throw flush_error(error);
            case error_policy::block:
                // To give the client the appearance of blocking, we need to
                // poll the writer, i.e. check periodically whether writing is
                // now working, until it starts working again. We don't remove
                // anything from the input queue while this happens, hence any
                // client threads that are writing log events will start
                // blocking once the input queue fills up. We use
                // shared_input_queue_full_event_ for an exponentially
                // increasing wait time between polls. That way we can check
                // the panic-flush flag early, which will be set in case the
                // program crashes.
                //
                // If the program crashes while the writer is failing (not an
                // unlikely scenario since circumstances are already ominous),
                // then we have a dilemma. We could keep on blocking, but then
                // we are withholding a crashing program from generating a core
                // dump until the writer starts working. Or we could just throw
                // the input queue away and pretend we're done with the panic
                // flush, so the program can die in peace. But then we will
                // lose log data that might be vital to determining the cause
                // of the crash. I've chosen the latter option, because I think
                // it's not likely that the log data will ever make it past the
                // writer anyway, even if we do keep on blocking.
                shared_input_queue_full_event_.wait(block_time_ms);
                if(panic_flush_)
                    throw flush_error(error);
                block_time_ms += std::max(1u, block_time_ms/4);
                block_time_ms = std::min(block_time_ms, 1000u);
                break;
            case error_policy::fail_immediately:
                if(!error_flag_.load(std::memory_order_relaxed)) {
                    error_code_ = error;
                    error_flag_.store(true, std::memory_order_release);
                }
                throw flush_error(error);
            }
        }
    }
}

char* output_buffer::reserve_slow_path(std::size_t size)
{
    std::size_t frame_size = (pcommit_end_ - pframe_end_) + size;
    std::size_t buffer_size = pbuffer_end_ - pbuffer_;
    if(unlikely(frame_size > buffer_size))
        throw excessive_output_by_frame();
    flush();
    return pcommit_end_;
}

}   // namespace reckless
