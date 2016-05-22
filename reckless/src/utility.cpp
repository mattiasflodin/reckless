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
#include "reckless/detail/utility.hpp"

#include <unistd.h>

namespace {
std::size_t get_cache_line_size()
{
    long sz = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
    return sz;
}
}   // anonymous namespace

namespace reckless {
namespace detail {
    
unsigned const cache_line_size = get_cache_line_size();

std::size_t get_page_size()
{
    long sz = sysconf(_SC_PAGESIZE);
    return sz;
}

void prefetch(void const* ptr, std::size_t size)
{
    char const* p = static_cast<char const*>(ptr);
    unsigned i = 0;
    unsigned const stride = cache_line_size;
    while(i < size) {
        __builtin_prefetch(p + i);
        i += stride;
    }
}

}   // namespace detail
}   // namespace reckless
