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
#include "reckless/file_writer.hpp"
#include "posix_error_category.hpp"

#include <system_error>

#include <sys/stat.h>   // open
#include <sys/types.h>  // open, lseek
#include <fcntl.h>      // open
#include <errno.h>      // errno
#include <unistd.h>     // lseek, close

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

reckless::file_writer::~file_writer()
{
    if(fd_ != -1) {
        while(-1 == close(fd_)) {
            if(errno != EINTR)
                break;
        }
    }
}

reckless::file_writer::file_writer(char const* path) :
    fd_writer(open_file(path))
{
}
