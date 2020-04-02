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
#ifndef RECKLESS_FD_WRITER_HPP
#define RECKLESS_FD_WRITER_HPP

#include <reckless/writer.hpp>

namespace reckless {
namespace detail {

class fd_writer : public writer {
public:
#if defined(__unix__)
    fd_writer(int fd) : fd_(fd) {}
#elif defined(_WIN32)
    fd_writer(void* handle) : handle_(handle) {}
#endif

    std::size_t write(void const* pbuffer, std::size_t count, std::error_code& ec) noexcept override;

#if defined(__unix__)
    int fd_;
#elif defined(_WIN32)
    void* handle_;
#endif
};

}   // namespace detail
}   // namespace reckless

#endif  // RECKLESS_FD_WRITER_HPP
