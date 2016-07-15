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
#ifndef RECKLESS_POLICY_LOG_HPP
#define RECKLESS_POLICY_LOG_HPP

#include <reckless/basic_log.hpp>
#include <reckless/template_formatter.hpp>
#include <reckless/detail/platform.hpp> // RECKLESS_TLS
#include <utility>  // forward
#include <cstring>  // memset
#include <cstdlib>  // size_t

namespace reckless {

#if defined(_WIN32)
namespace detail {
extern "C" {
    void __stdcall GetSystemTimeAsFileTime(void* lpSystemTimeAsFileTime);
    int __stdcall FileTimeToLocalFileTime(void const* lpFileTime, void* lpLocalFileTime);
    int __stdcall FileTimeToSystemTime(void const* lpFileTime, void* lpSystemTime);
}
}
#endif

class timestamp_field {
public:
#if defined(__unix__)
    timestamp_field()
    {
        // TODO "POSIX.1-2008 marks gettimeofday() as obsolete,
        // recommending the use of clock_gettime(2) instead." But
        // maybe this is faster on linux? (see vsyscalls)
        gettimeofday(&tv_, nullptr);
    }

    bool format(output_buffer* pbuffer)
    {
        struct tm tm;
        localtime_r(&tv_.tv_sec, &tm);

        template_formatter::format(pbuffer,
            "%.4d-%.2d-%.2d %.2d:%.2d:%.2d.%.3d",
            tm.tm_year + 1900,
            tm.tm_mon + 1,
            tm.tm_mday,
            tm.tm_hour,
            tm.tm_min,
            tm.tm_sec,
            static_cast<unsigned>(tv_.tv_usec)/1000u);
        return true;
    }

private:
    timeval tv_;

#elif defined(_WIN32)

    timestamp_field()
    {
        reckless::detail::GetSystemTimeAsFileTime(&ft_);
    }

    bool format(output_buffer* pbuffer)
    {
#pragma pack(push, 8)
        struct SYSTEMTIME {
            unsigned short wYear;
            unsigned short wMonth;
            unsigned short wDayOfWeek;
            unsigned short wDay;
            unsigned short wHour;
            unsigned short wMinute;
            unsigned short wSecond;
            unsigned short wMilliseconds;
        };
#pragma pack(pop)
        filetime ft_local;
        reckless::detail::FileTimeToLocalFileTime(&ft_, &ft_local);
        SYSTEMTIME st;
        reckless::detail::FileTimeToSystemTime(&ft_local, &st);
        template_formatter::format(pbuffer,
            "%.4d-%.2d-%.2d %.2d:%.2d:%.2d.%.3d",
            st.wYear,
            st.wMonth,
            st.wDay,
            st.wHour,
            st.wMinute,
            st.wSecond,
            st.wMilliseconds);
        return true;
    }

private:
#pragma pack(push, 8)
    struct filetime {
        unsigned long dwLowDateTime;
        unsigned long dwHighDateTime;
    };
#pragma pack(pop)
    filetime ft_;
#else
    static_assert(false, "timestamp_field is not implemented for this OS")
#endif
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
    static RECKLESS_TLS unsigned level_;
};

class no_indent
{
public:
    no_indent()
    {
    }

    void apply(output_buffer*)
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
#if defined(_WIN32)
        auto p = pbuffer->reserve(2);
        p[0] = '\r';
        p[1] = '\n';
        pbuffer->commit(2);
#else
        auto p = pbuffer->reserve(1);
        *p = '\n';
        pbuffer->commit(1);
#endif
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
    static void format_fields(output_buffer*)
    {
    }
};

template <class IndentPolicy = no_indent, char FieldSeparator = ' ', class... HeaderFields>
class policy_log : public basic_log {
public:
    using basic_log::basic_log;

    template <typename... Args>
    void write(char const* fmt, Args&&... args)
    {
        basic_log::write<policy_formatter<IndentPolicy, FieldSeparator, HeaderFields...>>(
                HeaderFields()...,
                IndentPolicy(),
                fmt,
                std::forward<Args>(args)...);
    }
};

}   // namespace reckless

#endif  // RECKLESS_POLICY_LOG_HPP
