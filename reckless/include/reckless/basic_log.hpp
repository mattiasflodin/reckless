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

#include <reckless/detail/thread_input_buffer.hpp>
#include <reckless/detail/spsc_event.hpp>
#include <reckless/detail/branch_hints.hpp> // likely
#include <reckless/detail/optional.hpp>
#include <reckless/output_buffer.hpp>

#include <boost_1_56_0/lockfree/queue.hpp>

#include <thread>
#include <functional>
#include <tuple>
#include <system_error> // system_error, error_code
#include <exception>    // current_exception, exception_ptr
#include <typeinfo>     // type_info
#include <mutex>

#include <pthread.h>    // pthread_key_t

namespace reckless {
namespace detail {
    template <class Formatter, typename... Args>
    std::size_t input_frame_dispatch(dispatch_operation operation, void* arg1, void* arg2);
}

using format_error_callback_t = std::function<void (output_buffer*, std::exception_ptr const&, std::type_info const&)>;

// TODO generic_log better name?
class basic_log : private output_buffer {
public:
    basic_log();
    // FIXME shared_input_queue_size seems like the least interesting of these
    // and should be moved to the end.
    basic_log(writer* pwriter,
            std::size_t output_buffer_max_capacity = 0,
            std::size_t shared_input_queue_size = 0,
            std::size_t thread_input_buffer_size = 0);
    virtual ~basic_log();

    basic_log(basic_log const&) = delete;
    basic_log& operator=(basic_log const&) = delete;

    virtual void open(writer* pwriter,
            std::size_t output_buffer_max_capacity = 0,
            std::size_t shared_input_queue_size = 0,
            std::size_t thread_input_buffer_size = 0);

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

    void format_error_callback(format_error_callback_t callback = format_error_callback_t())
    {
        std::lock_guard<std::mutex> lk(callback_mutex_);
        format_error_callback_ = move(callback);
    }

    using output_buffer::writer_error_callback;
    using output_buffer::temporary_error_policy;
    using output_buffer::permanent_error_policy;

    void panic_flush();

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
        std::size_t const args_offset = (sizeof(formatter_dispatch_function_t*) + args_align-1)/args_align*args_align;
        std::size_t const frame_size = args_offset + sizeof(args_t);

#ifdef RECKLESS_DEBUG
        // If this assert triggers then you have tried to write to the log from
        // within the worker thread (from writer_error_callback?). That's bad
        // because it can lead to a deadlock. Instead, you need to format your
        // string yourself and write directly to the output_buffer.
        assert(!pthread_equal(pthread_self(), output_worker_native_handle_));
#endif

        auto pbuffer = get_input_buffer();
        auto marker = pbuffer->allocation_marker();
        char* pframe = pbuffer->allocate_input_frame(frame_size);
        *reinterpret_cast<formatter_dispatch_function_t**>(pframe) =
            &detail::input_frame_dispatch<Formatter, typename std::decay<Args>::type...>;

        try {
            new (pframe + args_offset) args_t(std::forward<Args>(args)...);
            // TODO ideally send_log_entries would be called in a separate
            // commit() or flush() function, but then we have to call
            // get_input_buffer() twice which bloats the code at the call site. But
            // if we make get_input_buffer() protected (i.e. move
            // thread_input_buffer from the detail namespace) then we can delegate
            // the call to get_input_buffer to the derived class, which could then
            // call write multiple times followed by commit() if it wants to
            // without having to fetch the TLS variable every time.
            send_log_entries({pbuffer, pbuffer->input_end()});
        } catch(...) {
            pbuffer->revert_allocation(marker);
            throw;
        }
    }

private:
    void output_worker();
    void send_log_entries(detail::commit_extent const& ce);
    void send_log_entries_blind(detail::commit_extent const& ce);
    void flush_output_buffer();
    void halt_on_panic_flush();

    void reset_shared_input_queue(std::size_t node_count);
    detail::thread_input_buffer* get_input_buffer()
    {
        detail::thread_input_buffer* p = static_cast<detail::thread_input_buffer*>(pthread_getspecific(thread_input_buffer_key_));
        if(detail::likely(p != nullptr)) {
            return p;
        } else {
            return init_input_buffer();
        }
    }
    detail::thread_input_buffer* init_input_buffer();
    void on_panic_flush_done() __attribute__((noreturn));
    bool is_open()
    {
        return output_thread_.joinable();
    }

    typedef boost_1_56_0::lockfree::queue<detail::commit_extent, boost_1_56_0::lockfree::fixed_sized<true>> shared_input_queue_t;

    //typedef detail::thread_object<detail::thread_input_buffer, std::size_t, std::size_t> thread_input_buffer_t;
    //thread_input_buffer_t pthread_input_buffer_;

    std::experimental::optional<shared_input_queue_t> shared_input_queue_;
    spsc_event shared_input_queue_full_event_;
    spsc_event shared_input_consumed_event_;
    pthread_key_t thread_input_buffer_key_;
    std::size_t thread_input_buffer_size_ = 0;
    output_buffer output_buffer_;

    std::mutex callback_mutex_;
    format_error_callback_t format_error_callback_; // access synchronized by callback_mutex_
    std::thread output_thread_;
    spsc_event panic_flush_done_event_;
    bool panic_flush_ = false;

#ifdef RECKLESS_DEBUG
    pthread_t output_worker_native_handle_;
#endif
};

class writer_error : public std::system_error {
public:
    writer_error(std::error_code ec) :
        system_error(ec)
    {
    }
    char const* what() const noexcept override;
};

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
    std::size_t const args_offset = (sizeof(formatter_dispatch_function_t*) + args_align-1)/args_align*args_align;
    std::size_t const frame_size = args_offset + sizeof(args_t);
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
    } else if(operation == get_typeid) {
        *static_cast<std::type_info const**>(arg1) = &typeid(args_t);
        return frame_size;
    }
    unreachable();
}

}   // namespace detail
}   // namespace reckless

#endif  // RECKLESS_BASIC_LOG_HPP
