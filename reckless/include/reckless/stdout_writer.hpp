/* This file is part of reckless logging
 * Copyright 2015-2020 Mattias Flodin <git@codepentry.com>
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
#ifndef RECKLESS_STDOUT_WRITER_HPP
#define RECKLESS_STDOUT_WRITER_HPP

#include "detail/fd_writer.hpp"

namespace reckless {

#if defined(_WIN32)
namespace detail {
    unsigned long const RECKLESS_STD_OUTPUT_HANDLE = static_cast<unsigned long>(-11);
    unsigned long const RECKLESS_STD_ERROR_HANDLE = static_cast<unsigned long>(-12);
    extern "C" {
        void* __stdcall GetStdHandle(unsigned long nStdHandle);
    }
}   // namespace detail
#endif

class stdout_writer : public detail::fd_writer {
public:
#if defined(__unix__)
    stdout_writer() : fd_writer(1) { }
#elif defined(_WIN32)
    stdout_writer() : fd_writer(detail::GetStdHandle(detail::RECKLESS_STD_OUTPUT_HANDLE)) { }
#endif
};

class stderr_writer : public detail::fd_writer {
public:
#if defined(__unix__)
    stderr_writer() : fd_writer(2) { }
#elif defined(_WIN32)
    stderr_writer() : fd_writer(detail::GetStdHandle(detail::RECKLESS_STD_ERROR_HANDLE)) { }
#endif
};

}   // namespace reckless

#endif  // RECKLESS_STDOUT_WRITER_HPP

