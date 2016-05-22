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
#include <reckless/writer.hpp>

namespace reckless {
    
namespace {
class error_category_t : public std::error_category {
public:
    char const* name() const noexcept override;
    std::error_condition default_error_condition(int code) const noexcept override;
    std::string message(int condition) const override;
};

}   // anonymous namespace

writer::~writer()
{
}

std::error_category const& writer::error_category()
{
    static error_category_t ec;
    return ec;
}

char const* error_category_t::name() const noexcept
{
    return "reckless::writer";
}

std::error_condition error_category_t::default_error_condition(int code) const noexcept
{
    return static_cast<writer::errc>(code);
}

std::string error_category_t::message(int condition) const
{
    switch(static_cast<writer::errc>(condition)) {
    case writer::temporary_failure:
        return "temporary failure while writing log";
    case writer::permanent_failure:
        return "permanent failure while writing log";
    }
    throw std::invalid_argument("invalid condition code");
}

}   // namespace reckless
