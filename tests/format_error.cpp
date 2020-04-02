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
#include "memory_writer.hpp"
#include "eol.hpp"
#include <reckless/policy_log.hpp>

#include <vector>
#include <cassert>
#include <stdexcept>
#include <exception>    // rethrow_exception

#include <iostream>

memory_writer<std::string> g_writer;
reckless::policy_log<> g_log;

struct Object {
    Object() : copy_count_(0)
    {
    }
    Object(Object const& rhs)
    {
        copy_count_ = rhs.copy_count_ + 1;
        if(copy_count_ == 2)
            throw std::runtime_error("runtime error");
    }

    unsigned copy_count_;
};

char const* format(reckless::output_buffer* poutput, char const* fmt, Object)
{
    if(*fmt != 's')
        return nullptr;
    poutput->write('X');
    return fmt+1;
}

void format_error(reckless::output_buffer* poutput, std::exception_ptr const& pexception, std::type_info const&)
{
    poutput->write("<exception caught when calling formatter: ");
    try {
        std::rethrow_exception(pexception);
    } catch(std::exception const& e) {
        poutput->write(e.what());
    } catch(...) {
    }
#if defined(__unix__)
    poutput->write(">\n");
#elif defined(_WIN32)
    poutput->write(">\r\n");
#endif
}

int main()
{
    g_log.open(&g_writer);
    g_log.write("Hello");
    Object object;
    g_log.write("%s", object);
    g_log.write("World!");
    g_log.close();
    std::cout << g_writer.container;
    assert(g_writer.container == eol("Hello\nWorld!\n"));

    g_writer.container.clear();
    g_log.open(&g_writer);
    g_log.format_error_callback(format_error);
    g_log.write("Hello");
    g_log.write("%s", object);
    g_log.write("World!");
    g_log.close();
    std::cout << std::endl << g_writer.container;
    assert(g_writer.container ==
        eol("Hello\n"
        "<exception caught when calling formatter: runtime error>\n"
        "World!\n"));
    return 0;
}
