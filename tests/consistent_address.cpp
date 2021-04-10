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

// There was a bug in the pointer arithmetic in basic_log that would sometimes
// cause a different virtual address (but the same physical address) than was
// used during construction to be used for destroying an object submitted to
// the log. This test case reproduces that bug.

#include <reckless/policy_log.hpp>
#include <reckless/file_writer.hpp>

#include <cstdio>
#include <cstdlib>

struct holder {
    void* pmy_address;
    holder()
    {
        pmy_address = this;
    }
    holder(const holder&)
    {
        pmy_address = this;
    }
    ~holder()
    {
        if (this != pmy_address) {
            std::fprintf(stderr, "inconsistent object address; expected %p "
                "but got %p\n", pmy_address, this);
            std::fflush(stderr);
            std::abort();
        }
    }
};

char const* format(reckless::output_buffer* p, char const* fmt, holder const&)
{
    return fmt+1;
}

int main()
{
    reckless::file_writer writer("log.txt");
    reckless::policy_log<> log(&writer);
    for(int i=0; i!=500000; i++) {
        log.write("%s", holder());
    }
}
