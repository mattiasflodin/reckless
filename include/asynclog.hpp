#ifndef ASYNCLOG_ASYNCLOG_HPP
#define ASYNCLOG_ASYNCLOG_HPP

#include "asynclog/detail/spsc_event.hpp"
#include "asynclog/detail/utility.hpp"
#include "asynclog/detail/branch_hints.hpp"
#include "asynclog/detail/input.hpp"
#include "asynclog/detail/frame.hpp"
#include "asynclog/detail/log_base.hpp"

namespace asynclog {

template <typename T>
char const* invoke_format(output_buffer* pbuffer, char const* pformat, T&& v)
{
    typedef decltype(format(pbuffer, pformat, std::forward<T>(v))) return_type;
    static_assert(std::is_convertible<return_type, char const*>::value,
        "return type of format<T> must be char const*");
    return format(pbuffer, pformat, std::forward<T>(v));
}

class default_formatter {
public:
    static void format(output_buffer* pbuffer, char const* pformat);

    template <typename T, typename... Args>
    static void format(output_buffer* pbuffer, char const* pformat,
            T&& value, Args&&... args)
    {
        pformat = next_specifier(pbuffer, pformat);
        if(not pformat)
            return;

        // TODO can we invoke free format() using argument-dependent lookup
        // without causing infinite recursion on this member function, without
        // this intermediary kludge?
        char const* pnext_format = invoke_format(pbuffer, pformat,
                std::forward<T>(value));
        if(pnext_format)
            pformat = pnext_format;
        else
            append_percent(pbuffer);
        return default_formatter::format(pbuffer, pformat,
                std::forward<Args>(args)...);
    }

private:
    static void append_percent(output_buffer* pbuffer);
    static char const* next_specifier(output_buffer* pbuffer,
            char const* pformat);
};

template <class Formatter=default_formatter>
class log : private detail::log_base {
public:
    log()
    {
    }

    // TODO is it right to just do g_page_size/sizeof(commit_extent) if we want
    // the buffer to use up one page? There's likely more overhead in the
    // buffer.
    // TODO all these calls to get_page_size are redundant, can it be cached somehow? Same for open() below.
    log(writer* pwriter,
            std::size_t input_frame_alignment = detail::get_cache_line_size(),
            std::size_t output_buffer_max_capacity = detail::get_page_size(),
            std::size_t shared_input_queue_size = detail::get_page_size()/sizeof(detail::commit_extent),
            std::size_t thread_input_buffer_size = detail::get_page_size()) :
        log_base(pwriter,
                 input_frame_alignment,
                 output_buffer_max_capacity,
                 shared_input_queue_size,
                 thread_input_buffer_size)
    {
        // TODO move assertions into log_base
        assert(detail::is_power_of_two(input_frame_alignment));
        // input_frame_alignment must at least match that of a function pointer
        assert(input_frame_alignment >= sizeof(detail::dispatch_function_t*));
        // We need the requirement below to ensure that, after alignment, there
        // will either be 0 free bytes available in the input buffer, or
        // enough to fit a function pointer. This simplifies the code a bit.
        assert(input_frame_alignment >= alignof(detail::dispatch_function_t*));
    }

    void open(writer* pwriter, 
            std::size_t input_frame_alignment = detail::get_cache_line_size(),
            std::size_t output_buffer_max_capacity = detail::get_page_size(),
            std::size_t shared_input_queue_size = detail::get_page_size()/sizeof(detail::commit_extent),
            std::size_t thread_input_buffer_size = detail::get_page_size())
    {
        log_base::open(pwriter, input_frame_alignment,
                output_buffer_max_capacity, shared_input_queue_size,
                thread_input_buffer_size);
    }

    void close()
    {
        log_base::close();
    }

    template <typename... Args>
    void write(Args&&... args)
    {
        assert(initialized_.load(std::memory_order_acquire()));
        assert(output_thread_.get_id() != std::thread::id());
        using namespace detail;
        using argument_binder = bind_args<Args...>;
        // fails in gcc 4.7.3
        //using bound_args = typename argument_binder::type;
        using bound_args = typename bind_args<Args...>::type;
        std::size_t const frame_size = argument_binder::frame_size;
        using frame = detail::frame<Formatter, frame_size, bound_args>;

        char* pframe = allocate_input_frame(frame_size);
        frame::store_args(pframe, std::forward<Args>(args)...);
    }


    void commit()
    {
        // TODO move asserts into log_base
        assert(initialized_.load(std::memory_order_acquire()));
        assert(output_thread_.get_id() != std::thread::id());
        log_base::commit();
    }

private:
    log(log const&);  // not defined
    log& operator=(log const&); // not defined
};

// TODO synchronous_channel for wrapping a channel and calling the formatter immediately

class file_writer : public writer {
public:
    file_writer(char const* path);
    ~file_writer();
    Result write(void const* pbuffer, std::size_t count);
private:
    int fd_;
};

char const* format(output_buffer* pbuffer, char const* pformat, char v);
char const* format(output_buffer* pbuffer, char const* pformat, signed char v);
char const* format(output_buffer* pbuffer, char const* pformat, unsigned char v);
char const* format(output_buffer* pbuffer, char const* pformat, wchar_t v);
char const* format(output_buffer* pbuffer, char const* pformat, char16_t v);
char const* format(output_buffer* pbuffer, char const* pformat, char32_t v);

char const* format(output_buffer* pbuffer, char const* pformat, short v);
char const* format(output_buffer* pbuffer, char const* pformat, unsigned short v);
char const* format(output_buffer* pbuffer, char const* pformat, int v);
char const* format(output_buffer* pbuffer, char const* pformat, unsigned int v);
char const* format(output_buffer* pbuffer, char const* pformat, long v);
char const* format(output_buffer* pbuffer, char const* pformat, unsigned long v);
char const* format(output_buffer* pbuffer, char const* pformat, long long v);
char const* format(output_buffer* pbuffer, char const* pformat, unsigned long long v);

char const* format(output_buffer* pbuffer, char const* pformat, float v);
char const* format(output_buffer* pbuffer, char const* pformat, double v);
char const* format(output_buffer* pbuffer, char const* pformat, long double v);

char const* format(output_buffer* pbuffer, char const* pformat, char const* v);
char const* format(output_buffer* pbuffer, char const* pformat, std::string const& v);

}   // namespace asynclog

#endif    // ASYNCLOG_ASYNCLOG_HPP
