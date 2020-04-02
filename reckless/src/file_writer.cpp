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
#include "reckless/file_writer.hpp"

#include <system_error>
#include <cassert>
#include <limits>   // numeric_limits

#if defined(__unix__)
#include <sys/stat.h>   // open
#include <sys/types.h>  // open, lseek
#include <fcntl.h>      // open
#include <errno.h>      // errno
#include <unistd.h>     // lseek, close

#elif defined(_WIN32)

#define NOMINMAX
#include <Windows.h>

#endif

#if defined(__unix__)
namespace {
int open_file(char const* path)
{
    auto full_access =
        S_IRUSR | S_IWUSR |
        S_IRGRP | S_IWGRP |
        S_IROTH | S_IWOTH;
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, full_access);
    if(fd == -1)
        throw std::system_error(errno, std::system_category());
    return fd;
}

}

reckless::file_writer::file_writer(char const* path) :
    fd_writer(open_file(path))
{
}

reckless::file_writer::~file_writer()
{
    if(fd_ != -1) {
        while(-1 == close(fd_)) {
            if(errno != EINTR)
                break;
        }
    }
}

#elif defined(_WIN32)
namespace {
    template <class F, typename T>
    HANDLE createfile_generic(F CreateFileX, T const* path)
    {
        // From NtCreateFile documentation:
        // (https://msdn.microsoft.com/en-us/library/bb432380.aspx)
        // If only the FILE_APPEND_DATA and SYNCHRONIZE flags are set, the caller
        // can write only to the end of the file, and any offset information on
        // writes to the file is ignored. However, the file is automatically
        // extended as necessary for this type of write operation.
        // TODO what happens if the user deletes the file while we are writing?
        HANDLE h = CreateFileX(path,
            FILE_APPEND_DATA | SYNCHRONIZE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if(h == INVALID_HANDLE_VALUE)
            throw std::system_error(GetLastError(), std::system_category());
        return h;
    }
}
reckless::file_writer::file_writer(char const* path) :
    fd_writer(createfile_generic(CreateFileA, path))
{
}

reckless::file_writer::file_writer(wchar_t const* path) :
    fd_writer(createfile_generic(CreateFileW, path))
{
}

reckless::file_writer::~file_writer()
{
    CloseHandle(handle_);
}

#endif
