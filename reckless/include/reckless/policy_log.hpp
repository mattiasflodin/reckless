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
#include <utility>  // forward
#include <cstring>  // memset
#include <cstdlib>  // size_t

namespace reckless {
// TODO some way of allocating a specific input buffer size for a thread that
// needs extra space.

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
        strftime(p, 25, "%Y-%m-%d %H:%M:%S.", &tm);
        sprintf(p+20, "%03u ", static_cast<unsigned>(tv_.tv_usec)/1000u);
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
        auto p = pbuffer->reserve(1);
        *p = '\n';
        pbuffer->commit(1);
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
    policy_log()
    {
    }

    policy_log(writer* pwriter,
            std::size_t output_buffer_max_capacity = 0,
            std::size_t shared_input_queue_size = 0,
            std::size_t thread_input_buffer_size = 0) :
        basic_log(pwriter,
                 output_buffer_max_capacity,
                 shared_input_queue_size,
                 thread_input_buffer_size)
    {
    }

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
