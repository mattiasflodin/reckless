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
#ifndef RECKLESS_SEVERITY_LOG_HPP
#define RECKLESS_SEVERITY_LOG_HPP

#include <reckless/policy_log.hpp>

namespace reckless {
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
            std::size_t thread_input_buffer_size = 0) :
        basic_log(pwriter,
                 output_buffer_max_capacity,
                 shared_input_queue_size,
                 thread_input_buffer_size)
    {
    }

    template <typename... Args>
    void debug(char const* fmt, Args&&... args)
    {
        write('D', fmt, std::forward<Args>(args)...);
    }
    template <typename... Args>
    void info(char const* fmt, Args&&... args)
    {
        write('I', fmt, std::forward<Args>(args)...);
    }
    template <typename... Args>
    void warn(char const* fmt, Args&&... args)
    {
        write('W', fmt, std::forward<Args>(args)...);
    }
    template <typename... Args>
    void error(char const* fmt, Args&&... args)
    {
        write('E', fmt, std::forward<Args>(args)...);
    }

private:
    template <typename... Args>
    void write(char severity, char const* fmt, Args&&... args)
    {
        basic_log::write<policy_formatter<IndentPolicy, FieldSeparator, HeaderFields...>>(
                detail::construct_header_field<HeaderFields>(severity)...,
                IndentPolicy(),
                fmt,
                std::forward<Args>(args)...);
    }
};

}   // namespace reckless

#endif  // RECKLESS_SEVERITY_LOG_HPP
