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
#include <reckless/policy_log.hpp>
#include <reckless/file_writer.hpp>
#include <reckless/crash_handler.hpp>

using log_t = reckless::policy_log<
    reckless::indent<4>,       // 4 spaces of indent
    ' ',                       // Field separator
    reckless::timestamp_field  // Then timestamp field
    >;

reckless::file_writer writer("log.txt");
log_t g_log(&writer);

int main()
{
    // This shows how to install / uninstall a crash handler and how
    // it is ensured that the log queue is flushed on a crash. See manual
    // for more information.
    reckless::install_crash_handler(&g_log);
    for(int i=0; i!=16; i++) {
        g_log.write("Hello World!");
    }
    char* p = 0;
    *p = 'X';
    reckless::uninstall_crash_handler();
    return 0;
}
