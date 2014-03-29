#ifndef ASYNCLOG_ASYNCLOG_HPP
#define ASYNCLOG_ASYNCLOG_HPP

#include "spsc_event.hpp"
#include <boost/lockfree/queue.hpp>

namespace asynclog {

class output_buffer {
public:
    output_buffer(writer* pwriter, std::size_t max_capacity);
    ~output_buffer();
    char* reserve(std::size_t size);
    void commit(std::size_t size)
    {
        pcommit_end_ += size;
    }
    void flush();

private:
    writer* pwriter_;
    char* pbuffer_;
    char* pcommit_end_;
    char* pbuffer_end_;
};

template <typename T>
char const* invoke_format(output_buffer* pbuffer, char const* pformat, T&& v)
{
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
        char const* next_pformat = invoke_format(pbuffer, pformat,
                std::forward<T>(value));
        if(new_pformat)
            pformat = next_pformat;
        else
            append_percent(pbuffer);
        return formatter::format(pbuffer, pformat,
                std::forward<Args>(args)...);
    }

private:
    static void append_percent(output_buffer* pbuffer);
    static char const* next_specifier(output_buffer* pbuffer,
            char const* pformat);
};

template <class Formatter, std::size_t SharedInputQueueSize = ASYNCLOG_ARCH_PAGE_SIZE/sizeof(detail::commit_extent), std::size_t ThreadInputBufferSize = ASYNCLOG_ARCH_PAGE_SIZE, InputFrameAlignment, >
class channel {
public:
    template <typename... Args>
    static void write(Args&&... args)
    {
        using namespace dlog::detail;
        using argument_binder = bind_args<Args...>;
        // fails in gcc 4.7.3
        //using bound_args = typename argument_binder::type;
        using bound_args = typename bind_args<Args...>::type;
        std::size_t const frame_size = argument_binder::frame_size;
        std::size_t const frame_size_aligned = (frame_size+FRAME_ALIGNMENT-1)/FRAME_ALIGNMENT*FRAME_ALIGNMENT;
        using frame = detail::frame<Formatter, frame_size_aligned, bound_args>;

        auto pib = get_thread_input_buffer();
        char* pframe = pib->allocate_input_frame(frame_size_aligned);
        frame::store_args(pframe, std::forward<Args>(args)...);
    }
};

// TODO synchronous_channel for wrapping a channel and calling the formatter immediately

typedef logger<default_formatter> default_channel;

class writer {
public:
    enum Result
    {
        SUCCESS,
        ERROR_TRY_LATER,
        ERROR_GIVE_UP
    };
    virtual ~writer() = 0;
    virtual Result write(void const* pbuffer, std::size_t count) = 0;
};

class file_writer : public writer {
public:
    file_writer(char const* path);
    ~file_writer();
    Result write(void const* pbuffer, std::size_t count);
private:
    int fd_;
};

void initialize(writer* pwriter);
void initialize(writer* pwriter, std::size_t max_output_buffer_size);
void cleanup();

//class initializer {
//public:
//    initializer(writer* pwriter)
//    {
//        initialize(pwriter);
//    }
//    initializer(writer* pwriter, std::size_t max_capacity)
//    {
//        initialize(pwriter, max_capacity);
//    }
//    ~initializer()
//    {
//        cleanup();
//    }
//};

template <typename... Args>
void write(char const* fmt, Args&&... args)
{
    default_channel::write(fmt, std::forward(args)...);
}

void commit();

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
