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
// Example of a formatter for a user-defined type.
#include <reckless/policy_log.hpp>
#include <reckless/file_writer.hpp>
#include <reckless/ntoa.hpp>
    
reckless::file_writer writer("log.txt");
reckless::policy_log<> g_log(&writer);

struct Coordinate
{
    int x;
    int y;
};

char const* format(reckless::output_buffer* pbuffer, char const* fmt, Coordinate const& c)
{
    pbuffer->write("Hello ");
    if(*fmt == 'd')
        reckless::template_formatter::format(pbuffer, "(%d, %d)", c.x, c.y);
    else if(*fmt == 'x')
        reckless::template_formatter::format(pbuffer, "(%#x, %x)", c.x, c.y);
    else
        return nullptr;
    return fmt+1;
}

int main()
{
    Coordinate c{13, 17};
    g_log.write("%d", c);
    g_log.write("%x", c);

    return 0;
}

