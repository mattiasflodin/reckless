#ifndef ASYNCLOG_ASYNCLOG_HPP
#define ASYNCLOG_ASYNCLOG_HPP

#include "asynclog/output_buffer.hpp"

#include "asynclog/detail/spsc_event.hpp"
#include "asynclog/detail/branch_hints.hpp"
#include "asynclog/detail/input.hpp"
#include "asynclog/detail/formatter.hpp"
#include "asynclog/detail/log_base.hpp"

#include <tuple>

namespace asynclog {

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
        char const* pnext_format = detail::invoke_custom_format(pbuffer, pformat,
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
    // TODO input_frame_alignment should probably be the last argument.
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
        assert(input_frame_alignment >= sizeof(detail::formatter_dispatch_function_t*));
        // We need the requirement below to ensure that, after alignment, there
        // will either be 0 free bytes available in the input buffer, or
        // enough to fit a function pointer. This simplifies the code a bit.
        assert(input_frame_alignment >= alignof(detail::formatter_dispatch_function_t*));
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
        using namespace detail;
        // TODO move asserts into log_base
        assert(is_open());
        typedef std::tuple<typename std::decay<Args>::type...> args_t;
        std::size_t const args_align = alignof(args_t);
        std::size_t const args_offset = (sizeof(formatter_dispatch_function_t*) + args_align-1)/args_align*args_align;
        std::size_t const frame_size = args_offset + sizeof(args_t);

        char* pframe = allocate_input_frame(frame_size);
        *reinterpret_cast<formatter_dispatch_function_t**>(pframe) =
            &formatter_dispatch<Formatter, typename std::decay<Args>::type...>;
        new (pframe + args_offset) args_t(std::forward<Args>(args)...);
    }


    void commit()
    {
        // TODO move asserts into log_base
        assert(is_open());
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
