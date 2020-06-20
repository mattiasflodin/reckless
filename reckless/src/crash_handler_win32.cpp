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

#include <reckless/basic_log.hpp>
#include <reckless/crash_handler.hpp>

#include <cassert>
#include <vector>

#include <Windows.h>
#include <TlHelp32.h>

namespace reckless {

namespace {
std::vector<basic_log*> g_logs;
LPTOP_LEVEL_EXCEPTION_FILTER g_old_exception_filter = nullptr;

LONG WINAPI exception_filter(_EXCEPTION_POINTERS *ExceptionInfo)
{
    for(basic_log* plog : g_logs)
        plog->start_panic_flush();
    for(basic_log* plog : g_logs)
        plog->await_panic_flush();
    if (g_old_exception_filter == nullptr)
        return EXCEPTION_CONTINUE_SEARCH;
    else
        return g_old_exception_filter(ExceptionInfo);
}

}   // anonymous namespace

void install_crash_handler(std::initializer_list<basic_log*> logs)
{
    assert(logs.size() != 0);
    assert(g_logs.empty());
    g_logs.assign(begin(logs), end(logs));

    g_old_exception_filter = SetUnhandledExceptionFilter(&exception_filter);
}

void uninstall_crash_handler()
{
    assert(!g_logs.empty());
    SetUnhandledExceptionFilter(g_old_exception_filter);
    g_old_exception_filter = nullptr;
    g_logs.clear();
}

}   // namespace reckless
