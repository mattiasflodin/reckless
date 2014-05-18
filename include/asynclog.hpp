#ifndef ASYNCLOG_ASYNCLOG_HPP
#define ASYNCLOG_ASYNCLOG_HPP

#include "asynclog/output_buffer.hpp"

// TODO clean these includes up
#include "asynclog/detail/spsc_event.hpp"
#include "asynclog/detail/branch_hints.hpp"
#include "asynclog/detail/input.hpp"
#include "asynclog/detail/basic_log.hpp"

#include <tuple>

#include <cstring>

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

class timestamp_field {
public:
    timestamp_field()
    {
        gettimeofday(&tv_, nullptr);
    }

    bool format(output_buffer* pbuffer)
    {
        // "YYYY-mm-dd HH:MM:SS.FFF " -> 24 chars
        // Need 25 chars since sprintf wants to add a NUL char.
        char* p = pbuffer->reserve(25);
        struct tm tm;
        localtime_r(&tv_.tv_sec, &tm);
        strftime(p, 26, "%Y-%m-%d %H:%M:%S.", &tm);
        sprintf(p+21, "%03u ", static_cast<unsigned>(tv_.tv_usec)/1000u);
        pbuffer->commit(24);

        return true;
    }

private:
    timeval tv_;
};

class scoped_indent
{
public:
    scoped_indent()
    {
        ++level_;
    }
    ~scoped_indent()
    {
        --level_;
    }

    static unsigned level()
    {
        return level_;
    }

private:
    static __thread unsigned level_;
};

class no_indent
{
public:
    no_indent()
    {
    }

    void apply(output_buffer* pbuffer)
    {
    }
};

template <unsigned Multiplier, char Character = ' '>
class indent
{
public:
    indent() : level_(scoped_indent::level())
    {
    }

    void apply(output_buffer* pbuffer)
    {
        auto n = Multiplier*level_;
        char* p = pbuffer->reserve(n);
        std::memset(p, Character, n);
        pbuffer->commit(n);
    }

private:
    unsigned level_;
};

template <class IndentPolicy, char Separator, class... Fields>
class policy_formatter {
public:
    template <typename... Args>
    static void format(output_buffer* pbuffer, Fields&&... fields,
        IndentPolicy indent, char const* pformat, Args&&... args)
    {
        format_fields(pbuffer, fields...);
        indent.apply(pbuffer);
        template_formatter::format(pbuffer, pformat, std::forward<Args>(args)...);
    }

private:
    template <class Field, class... Remaining>
    static void format_fields(output_buffer* pbuffer, Field&& field, Remaining&&... remaining)
    {
        field.format(pbuffer);
        char* p = pbuffer->reserve(1);
        *p = Separator;
        pbuffer->commit(1);
        format_fields(pbuffer, std::forward<Remaining>(remaining)...);
    }
    static void format_fields(output_buffer* pbuffer)
    {
    }
};

#if 0
template <class IndentPolicy, class... HeaderFields>
class policy_log : public SeverityPolicy<policy_formatter<IndentPolicy, HeaderFields...>> {
public:
    policy_log()
    {
    }

    policy_log(writer* pwriter,
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
    void write(Args&&... args)
    {
        basic_log::write<policy_formatter<IndentPolicy, Fields...>>(HeaderFields()..., std::forward<Args>(args)...);
    }
};
#endif

class severity_field {
public:
    severity_field(char severity) : severity_(severity) {}

    void format(output_buffer* poutput_buffer) const
    {
        char* p = poutput_buffer->reserve(1);
        *p = severity_;
        poutput_buffer->commit(1);
    }

private:
    char severity_;
};

namespace detail {
    template <class HeaderField>
    HeaderField construct_header_field(char)
    {
         return HeaderField();
    }

    template <>
    inline severity_field construct_header_field<severity_field>(char severity)
    {
         return severity_field(severity);
    }
}

template <class IndentPolicy, char FieldSeparator, class... HeaderFields>
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
    void warn(Args&&... args)
    {
        basic_log::write<policy_formatter<IndentPolicy, FieldSeparator, HeaderFields...>>(
                detail::construct_header_field<HeaderFields>('W')...,
                IndentPolicy(),
                std::forward<Args>(args)...);
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
