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
#ifndef RECKLESS_WRITER_HPP
#define RECKLESS_WRITER_HPP

#include <cstdlib>  // size_t
#include <system_error>     // error_code, error_condition

// TODO synchronous log for wrapping a channel and calling the formatter immediately. Or, just add a bool to basic_log?

namespace reckless {

// TODO this is a bit vague, rename to e.g. log_target or someting?
class writer {
public:
    enum errc
    {
        temporary_failure = 1,
        permanent_failure = 2
    };
    static std::error_category const& error_category();

    virtual ~writer() = 0;
    virtual std::size_t write(void const* pbuffer, std::size_t count,
            std::error_code& ec) noexcept = 0;
};

inline std::error_condition make_error_condition(writer::errc ec)
{
    return std::error_condition(ec, writer::error_category());
}

inline std::error_code make_error_code(writer::errc ec)
{
    return std::error_code(ec, writer::error_category());
}
}   // namespace reckless

namespace std
{
    template <>
    struct is_error_condition_enum<reckless::writer::errc> : public true_type {};
}
#endif  // RECKLESS_WRITER_HPP
