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
#ifndef RECKLESS_BASIC_LOG_HPP
#define RECKLESS_BASIC_LOG_HPP

#include <reckless/detail/spsc_event.hpp>
#include <reckless/detail/platform.hpp> // likely, RECKLESS_CACHE_LINE_SIZE
#include <reckless/detail/utility.hpp>  // index_sequence
#include <reckless/detail/mpsc_ring_buffer.hpp>
#include <reckless/output_buffer.hpp>

#include <thread>
#include <functional>
#include <tuple>
#include <system_error> // system_error, error_code
#include <exception>    // current_exception, exception_ptr
#include <typeinfo>     // type_info
#include <mutex>

#if defined(__unix__)
#include <pthread.h>    // pthread_self
#endif

namespace reckless {
namespace detail {
#if defined(_WIN32)
    extern "C" unsigned long __stdcall GetCurrentThreadId();
#endif

    enum class frame_status : char {
        // The frame is currently free for use.
        uninitialized,
        // Frame was allocated but then the error_flag_ check failed, leaving
        // the frame uninitialized and it should be skipped by using
        // frame_header::frame_size.
        failed_error_check,
        // Frame was allocated but then the constructor failed, leaving the
        // dispatch pointer valid but without initialized data. It should be
        // skipped by using frame_header::pdispatch_funtion and
        // dispatch_operation::get_typeid.
        failed_initialization,
        // Frame is initialized and valid.
        initialized,
        // Frame is a shutdown marker, telling the worker thread to finish up
        // and exit.
        shutdown_marker,
        // Frame is a panic-shutdown watermark, telling the worker thread that
        // it should process no more frames and just wait until the program
        // crashes.
        panic_shutdown_marker
    };

    enum dispatch_operation {
        // The dispatch function should call the formatter to write to the
        // output buffer.
        invoke_formatter,
        // The dispatch function should return frame size type information for
        // the formatter instead of calling it. This is used during error
        // handling.
        get_typeid
    };

    typedef std::size_t formatter_dispatch_function_t(dispatch_operation, void*, void*);

    struct frame_header {
        union {
            formatter_dispatch_function_t* pdispatch_function;  // valid when status is failed_initialization or initialized
            std::size_t frame_size; // valid when status == failed_error_check
        };
        frame_status status;
    };

    template <class Formatter, typename... Args>
    std::size_t input_frame_dispatch(dispatch_operation operation, void* arg1, void* arg2);

}

using format_error_callback_t = std::function<void (output_buffer*, std::exception_ptr const&, std::type_info const&)>;

class basic_log : private output_buffer {
public:
    basic_log();
    basic_log(writer* pwriter)
    {
        open(pwriter);
    }
    basic_log(writer* pwriter,
        std::size_t input_buffer_capacity,
        std::size_t output_buffer_capacity)
    {
        open(pwriter, input_buffer_capacity, output_buffer_capacity);
    }
    virtual ~basic_log();

    basic_log(basic_log const&) = delete;
    basic_log& operator=(basic_log const&) = delete;

    void open(writer* pwriter);
    void open(writer* pwriter,
        std::size_t input_buffer_capacity,
        std::size_t output_buffer_capacity);

    // Wait for the output worker to flush its remaining output queue, then shut
    // down the background thread and release all buffers. Writing to the log
    // after close() was called leads to undefined behavior. If the writer
    // returns any error (temporary or permanent) at this point, then the ec
    // parameter is set to the error code. Otherwise, ec.clear() is called.
    virtual void close(std::error_code& ec) noexcept;

    // Call close(error_code) and throw writer_error if it fails.
    virtual void close();

    // Wake up the output worker thread to cause immediate output of all queued
    // writes. Then block until the most recent write has been formatted in the
    // output buffer, and finally flush the output buffer. If the output buffer
    // flush generates an error, then that error is returned in ec. Otherwise
    // ec.clear() is called.
    virtual void flush(std::error_code& ec);

    // Call flush(error_code) and throw writer_error if it fails.
    virtual void flush();

    void format_error_callback(
            format_error_callback_t callback = format_error_callback_t())
    {
        std::lock_guard<std::mutex> lk(callback_mutex_);
        format_error_callback_ = move(callback);
    }

    using output_buffer::writer_error_callback;
    using output_buffer::temporary_error_policy;
    using output_buffer::permanent_error_policy;

    void start_panic_flush();
    void await_panic_flush();
    bool await_panic_flush(unsigned int miliseconds);

    // Provide access to the internal worker-thread object. The intent is to
    // allow platform-specific manipulation of the thread, such as setting
    // priority or affinity.
    std::thread& worker_thread()
    {
        return output_thread_;
    }

protected:
    template <class Formatter, typename... Args>
    void write(Args&&... args)
    {
        using namespace detail;
        typedef std::tuple<typename std::decay<Args>::type...> args_t;
        std::size_t const args_align = alignof(args_t);
        std::size_t const args_offset = (sizeof(frame_header) +
            args_align-1)/args_align*args_align;
        std::size_t const frame_size_unaligned = args_offset + sizeof(args_t);
        std::size_t const frame_size = (frame_size_unaligned +
            RECKLESS_CACHE_LINE_SIZE-1)/RECKLESS_CACHE_LINE_SIZE*
            RECKLESS_CACHE_LINE_SIZE;

#ifdef RECKLESS_DEBUG
        // If this assert triggers then you have tried to write to the log from
        // within the worker thread (from writer_error_callback?). That's bad
        // because it can lead to a deadlock. Instead, you need to format your
        // string yourself and write directly to the output_buffer.

#if defined(_POSIX_VERSION)
        assert(!pthread_equal(pthread_self(), output_worker_native_handle_));
#elif defined(_WIN32)
        assert(GetCurrentThreadId() == output_worker_native_id_);
#endif

#endif  // RECKLESS_DEBUG

        frame_header* pframe = push_input_frame(frame_size);
        //pframe->pdispatch_function = &detail::input_frame_dispatch<
        //        Formatter,
        //        typename std::decay<Args>::type...
        //    >;
        pframe->frame_size = frame_size;

        //void* pargs = static_cast<char*>(static_cast<void*>(pframe))
        //    + args_offset;
        // Let the compiler know that the placement-new call below
        // doesn't need to perform a null-pointer check.
        //assume(pargs != nullptr);

        // try {
        //     new (pargs) args_t(std::forward<Args>(args)...);
        // } catch(...) {
        //     atomic_store_release(&pframe->status, frame_status::failed_initialization);
        //     throw;
        // }
        // atomic_store_release(&pframe->status, frame_status::initialized);
        //new (pargs) args_t(std::forward<Args>(args)...);
        //atomic_store_release(&pframe->status, frame_status::initialized);
        atomic_store_release(&pframe->status, frame_status::failed_error_check);
        //atomic_store_release(&pframe->status, frame_status::failed_initialization);
    }

private:
    detail::frame_header* push_input_frame(std::size_t size);
    detail::frame_header* push_input_frame_blind(std::size_t frame_size);
    detail::frame_header* push_input_frame_slow_path(
        detail::frame_header * pframe, bool error, std::size_t size);

    void output_worker();
    std::size_t wait_for_input();
    detail::frame_status acquire_frame(void* pframe);
    std::size_t process_frame(void* pframe);
    std::size_t skip_frame(void* pframe);
    void clear_frame(void* pframe, std::size_t frame_size);

    void flush_output_buffer();

    [[noreturn]]
    void on_panic_flush_done();
    bool is_open()
    {
        return output_thread_.joinable();
    }

    detail::mpsc_ring_buffer input_buffer_;
    detail::spsc_event input_buffer_full_event_;
    detail::spsc_event input_buffer_empty_event_;

    std::mutex callback_mutex_;
    format_error_callback_t format_error_callback_; // access synchronized by callback_mutex_
    std::thread output_thread_;
    detail::spsc_event panic_flush_done_event_;

#if defined(_POSIX_VERSION)
    pthread_t output_worker_native_handle_;
#elif defined(_WIN32)
    unsigned long output_worker_native_id_;
#else
    static_assert(false, "threading not implemented for this platform")
#endif  // RECKLESS_DEBUG
};

class writer_error : public std::system_error {
public:
    writer_error(std::error_code ec) :
        system_error(ec)
    {
    }
    char const* what() const noexcept override;
};

inline detail::frame_header* basic_log::push_input_frame(
        std::size_t size)
{
    using namespace detail;
    // This is possibly useless micro-optimization but after all, making this
    // path fast is the whole point of the library so I'll indulge myself.
    // Instead of having one branch for checking the result of
    // input_buffer_.push and another for checking the error flag, we combine
    // both checks into one. That means we have to mark the allocated input
    // frame as failed_error_check if it succeeds but the error check does not.
    auto pframe = static_cast<frame_header*>(input_buffer_.push(size));
    auto error = atomic_load_acquire(&error_flag_);
    std::uint64_t no_error = ~static_cast<std::uint64_t>(error);
    no_error &= reinterpret_cast<std::uintptr_t>(pframe);
    if(likely(no_error != 0))
        return pframe;
    else
        return push_input_frame_slow_path(pframe, error, size);
}

inline detail::frame_header* basic_log::push_input_frame_blind(
        std::size_t size)
{
    using namespace detail;
    auto pframe = static_cast<frame_header*>(input_buffer_.push(size));
    if(likely(pframe != nullptr))
        return pframe;
    else
        return push_input_frame_slow_path(nullptr, false, size);
}

namespace detail {

template <class Formatter, typename... Args, std::size_t... Indexes>
void formatter_dispatch_helper(output_buffer* poutput, std::tuple<Args...>&& args, index_sequence<Indexes...>)
{
    Formatter::format(poutput, std::forward<Args>(std::get<Indexes>(args))...);
}

template <class Formatter, typename... Args>
std::size_t input_frame_dispatch(dispatch_operation operation, void* arg1, void* arg2)
{
    using namespace detail;
    typedef std::tuple<Args...> args_t;
    std::size_t const args_align = alignof(args_t);
    std::size_t const status_offset = sizeof(formatter_dispatch_function_t*);
    std::size_t const args_offset = (status_offset + sizeof(frame_status) +
        args_align-1)/args_align*args_align;
    std::size_t const frame_size_unaligned = args_offset + sizeof(args_t);
    std::size_t const frame_size = (frame_size_unaligned +
        RECKLESS_CACHE_LINE_SIZE-1)/RECKLESS_CACHE_LINE_SIZE*
        RECKLESS_CACHE_LINE_SIZE;

    typename make_index_sequence<sizeof...(Args)>::type indexes;

    if(likely(operation == invoke_formatter)) {
        auto poutput = static_cast<output_buffer*>(arg1);
        auto pinput = static_cast<char*>(arg2);
        struct args_owner {
            args_owner(args_t& args) :
                args(args)
            {
            }
            ~args_owner()
            {
                // We need to do this from a destructor in case calling the
                // formatter throws an exception. We can't just do it in a catch
                // clause because we want uncaught_exception to return true during
                // the call.
                args.~args_t();
            }
            args_t& args;
        };

        args_owner args(*reinterpret_cast<args_t*>(pinput + args_offset));
        formatter_dispatch_helper<Formatter>(poutput, move(args.args), indexes);
        return frame_size;
    } else {
        // operation == get_typeid
        *static_cast<std::type_info const**>(arg1) = &typeid(args_t);
        return frame_size;
    }
}

}   // namespace detail
}   // namespace reckless

#endif  // RECKLESS_BASIC_LOG_HPP
