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

template <class Formatter>
class log : private detail::log_base {
public:
    // TODO is it right to just do g_page_size/sizeof(commit_extent) if we want
    // the buffer to use up one page? There's likely more overhead in the
    // buffer.
    log(writer* pwriter,
            std::size_t input_frame_alignment,
            std::size_t output_buffer_max_capacity = detail::get_page_size(),
            std::size_t shared_input_queue_size = detail::get_page_size()/sizeof(detail::commit_extent),
            std::size_t thread_input_buffer_size = detail::get_page_size()) :
        log_base(pwriter,
                 input_frame_alignment,
                 output_buffer_max_capacity,
                 shared_input_queue_size,
                 thread_input_buffer_size)
    {
        // FIXME need an assert to make sure input_frame_alignment is power of
        // two.
        assert(detail::is_power_of_two(input_frame_alignment));
    }

    ~log()
    {
    }

    template <typename... Args>
    void write(Args&&... args)
    {
        using namespace detail;
        using argument_binder = bind_args<Args...>;
        // fails in gcc 4.7.3
        //using bound_args = typename argument_binder::type;
        using bound_args = typename bind_args<Args...>::type;
        std::size_t const frame_size = argument_binder::frame_size;
        using frame = detail::frame<Formatter, frame_size, bound_args>;

        auto pib = pthread_input_buffer_.get();
        auto mask = input_frame_alignment_mask_;
        auto frame_size_aligned = (frame_size + mask) & ~mask;
        char* pframe = pib->allocate_input_frame(frame_size_aligned);
        frame::store_args(pframe, std::forward<Args>(args)...);
    }
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
