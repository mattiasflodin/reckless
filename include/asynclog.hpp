#ifndef ASYNCLOG_ASYNCLOG_HPP
#define ASYNCLOG_ASYNCLOG_HPP

#include "asynclog/output_buffer.hpp"

// TODO clean these includes up
#include "asynclog/detail/spsc_event.hpp"
#include "asynclog/detail/branch_hints.hpp"
#include "asynclog/detail/input.hpp"
#include "asynclog/detail/basic_log.hpp"

#include <tuple>

namespace asynclog {

// TODO some way of allocating a specific input buffer size for a thread that
// needs extra space.

class template_formatter {
public:
    static void format(output_buffer* pbuffer, char const* pformat);

    template <typename T, typename... Args>
    static void format(output_buffer* pbuffer, char const* pformat,
            T&& value, Args&&... args)
    {
        pformat = next_specifier(pbuffer, pformat);
        if(not pformat)
            return;

        char const* pnext_format = detail::invoke_custom_format(pbuffer,
                pformat, std::forward<T>(value));
        if(pnext_format)
            pformat = pnext_format;
        else
            append_percent(pbuffer);
        return template_formatter::format(pbuffer, pformat,
                std::forward<Args>(args)...);
    }

private:
    static void append_percent(output_buffer* pbuffer);
    static char const* next_specifier(output_buffer* pbuffer,
            char const* pformat);
};

class simple_log : public basic_log {
public:
    simple_log()
    {
    }

    simple_log(writer* pwriter,
            std::size_t output_buffer_max_capacity = 0,
            std::size_t shared_input_queue_size = 0,
            std::size_t thread_input_buffer_size = 0,
            std::size_t input_frame_alignment = 0) :
        basic_log(pwriter,
                 output_buffer_max_capacity,
                 shared_input_queue_size,
                 thread_input_buffer_size,
                 input_frame_alignment)
    {
    }

    template <typename... Args>
    void write(char const* fmt, Args&&... args)
    {
        basic_log::write<template_formatter>(fmt, std::forward<Args>(args)...);
    }
};

class timestamp_formatter {
public:
    template <typename... Args>
    static void format(output_buffer* pbuffer, timeval const& tv, 
        char const* pformat, Args&&... args)
    {
        // "YYYY-mm-dd HH:MM:SS.FFF " -> 24 chars
        // Need 25 chars since sprintf wants to add a NUL char.
        char* p = pbuffer->reserve(25);
        struct tm tm;
        localtime_r(&tv.tv_sec, &tm);
        strftime(p, 26, "%Y-%m-%d %H:%M:%S.", &tm);
        sprintf(p+21, "%3u ", static_cast<unsigned>(tv.tv_usec)/1000u);
        pbuffer->commit(24);
        template_formatter::format(pbuffer, pformat, std::forward<Args>(args)...);
    }
};

class timestamped_log : public basic_log {
public:
    timestamped_log()
    {
    }

    timestamped_log(writer* pwriter,
            std::size_t output_buffer_max_capacity = 0,
            std::size_t shared_input_queue_size = 0,
            std::size_t thread_input_buffer_size = 0,
            std::size_t input_frame_alignment = 0) :
        basic_log(pwriter,
                 input_frame_alignment,
                 output_buffer_max_capacity,
                 shared_input_queue_size,
                 thread_input_buffer_size)
    {
    }

    template <typename... Args>
    void write(char const* fmt, Args&&... args)
    {
        timeval tv;
        gettimeofday(&tv, nullptr);
        basic_log::write<timestamp_formatter>(tv, fmt, std::forward<Args>(args)...);
    }

private:
    int count_;
};

class severity_formatter {
public:
    template <typename... Args>
    static void format(output_buffer* pbuffer, char severity, char const* pformat,
             Args&&... args)
    {
        char* p = pbuffer->reserve(2);
        p[0] = severity;
        p[1] = ' ';
        pbuffer->commit(2);
        template_formatter::format(pbuffer, pformat, std::forward<Args>(args)...);
    }
};

class severity_log : public basic_log {
public:
    severity_log()
    {
    }

    severity_log(writer* pwriter,
            std::size_t output_buffer_max_capacity = 0,
            std::size_t shared_input_queue_size = 0,
            std::size_t thread_input_buffer_size = 0,
            std::size_t input_frame_alignment = 0) :
        basic_log(pwriter,
                 input_frame_alignment,
                 output_buffer_max_capacity,
                 shared_input_queue_size,
                 thread_input_buffer_size)
    {
    }

    template <typename... Args>
    void info(char const* fmt, Args&&... args)
    {
        basic_log::write<severity_formatter>('I', fmt, std::forward<Args>(args)...);
    }
    template <typename... Args>
    void warn(char const* fmt, Args&&... args)
    {
        basic_log::write<severity_formatter>('W', fmt, std::forward<Args>(args)...);
    }
    template <typename... Args>
    void err(char const* fmt, Args&&... args)
    {
        basic_log::write<severity_formatter>('E', fmt, std::forward<Args>(args)...);
    }
};

class silly_formatter {
public:
    template <typename... Args>
    static void format(output_buffer* pbuffer, int count, char const* pformat,
             Args&&... args)
    {
        // TODO invoke_custom_format should probably not be an implementation detail.
        detail::invoke_custom_format(pbuffer, "d", count);
        char* p = pbuffer->reserve(1);
        *p = ' ';
        pbuffer->commit(1);
        template_formatter::format(pbuffer, pformat, std::forward<Args>(args)...);
    }
};

class silly_log : public basic_log {
public:
    silly_log()
    {
    }

    silly_log(writer* pwriter,
            std::size_t output_buffer_max_capacity = 0,
            std::size_t shared_input_queue_size = 0,
            std::size_t thread_input_buffer_size = 0,
            std::size_t input_frame_alignment = 0) :
        basic_log(pwriter,
                 input_frame_alignment,
                 output_buffer_max_capacity,
                 shared_input_queue_size,
                 thread_input_buffer_size),
        count_(0)
    {
    }

    template <typename... Args>
    void write(char const* fmt, Args&&... args)
    {
        basic_log::write<silly_formatter>(count_, fmt, std::forward<Args>(args)...);
        ++count_;
    }

private:
    int count_;
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
