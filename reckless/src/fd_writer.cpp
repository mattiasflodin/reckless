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
#include <reckless/detail/fd_writer.hpp>

#if defined(__unix__)
#include <errno.h>      // errno, EINTR
#include <unistd.h>     // write

#elif defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#include <cassert>
#endif

namespace {
    class error_category : public std::error_category {
    public:
        char const* name() const noexcept override
        {
            return "reckless::fd_writer";
        }
        std::error_condition default_error_condition(int code) const noexcept override
        {
            return std::system_category().default_error_condition(code);
        }
        bool equivalent(int code, std::error_condition const& condition) const noexcept override
        {
            if(condition.category() == reckless::writer::error_category())
                return file_writer_to_writer_category(code) == condition.value();
            else
                return std::system_category().equivalent(code, condition);
        }
        // This overload is called if we have an error_condition that belongs to
        // this error_category, and compare it to an error_code belonging to
        // another error_category. Since the error_category is in the anonymous
        // namespace, the only way that would happen is if you convert an
        // error_code that comes from here into an error_condition, then try to
        // compare it to an error_code from another category. It's a
        // pathological situation, but the correct way to implement it should be
        // to map the condition value from this category to
        // reckless::writer::error_category() if the error_code is in that
        // category, then do the comparison. It essentially reverses the
        // error_code / error_condition relationship.
        bool equivalent(std::error_code const& code, int condition) const noexcept override
        {
            if(code.category() == reckless::writer::error_category())
                return file_writer_to_writer_category(condition) == code.value();
            else
                return std::system_category().equivalent(code, condition);
        }
        std::string message(int condition) const override
        {
            return std::system_category().message(condition);
        }
    private:
        int file_writer_to_writer_category(int code) const
        {
#if defined(__unix__)
            switch(code) {
            case ENOSPC:
            case ENOBUFS:
            case EDQUOT:
            case EIO:
                return reckless::writer::temporary_failure;
            default:
                return reckless::writer::permanent_failure;
            }
#elif defined(_WIN32)
            switch(code) {
            case ERROR_BUSY:
            case ERROR_DISK_FULL:
            case ERROR_HANDLE_DISK_FULL:
            case ERROR_LOCK_VIOLATION:
            case ERROR_NOT_ENOUGH_MEMORY:
            case ERROR_NOT_ENOUGH_QUOTA:
            case ERROR_NOT_READY:
            case ERROR_OPERATION_ABORTED:
            case ERROR_OUTOFMEMORY:
            case ERROR_READ_FAULT:
            case ERROR_RETRY:
            case ERROR_SHARING_VIOLATION:
            case ERROR_WRITE_FAULT:
            case ERROR_WRITE_PROTECT:
                return reckless::writer::temporary_failure;
            default:
                return reckless::writer::permanent_failure;
            }
#endif
        }
    };

    error_category const& get_error_category()
    {
        static error_category cat;
        return cat;
    }

}

namespace reckless {
namespace detail {

#if defined(__unix__)
std::size_t fd_writer::write(void const* pbuffer, std::size_t count, std::error_code& ec) noexcept
{
    char const* p = static_cast<char const*>(pbuffer);
    char const* pend = p + count;
    ec.clear();
    while(p != pend) {
        ssize_t written = ::write(fd_, p, count);
        if(written == -1) {
            if(errno != EINTR) {
                ec.assign(errno, get_error_category());
                break;
            }
        } else {
            p += written;
        }
    }
    return p - static_cast<char const*>(pbuffer);
}

#elif defined(_WIN32)
std::size_t fd_writer::write(void const* pbuffer, std::size_t count, std::error_code& ec) noexcept
{
    DWORD written;
    assert(count < std::numeric_limits<DWORD>::max());
    if(WriteFile(handle_, pbuffer, static_cast<DWORD>(count), &written, NULL)) {
        assert(written == count);
        ec.clear();
        return count;
    } else {
        int err = GetLastError();
        ec.assign(err, get_error_category());
        return written;
    }
}

#endif

}   // namespace detail
}   // namespace reckless
