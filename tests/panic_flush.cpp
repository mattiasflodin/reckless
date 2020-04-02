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
#include <memory>
#include <cstdio>
#include <thread>

#ifdef RECKLESS_ENABLE_TRACE_LOG
#include <performance_log/trace_log.hpp>
#endif
#include <iostream> // cout

#if defined(__unix__)
#include <signal.h>
#elif defined(_WIN32)
#include <Windows.h>
#endif

reckless::policy_log<> g_log;
volatile int t1_count=0;
volatile int t2_count=0;

#if defined(__unix__)
void sigsegv_handler(int)
{
    int t1c = t1_count;
    int t2c = t2_count;
    g_log.start_panic_flush();
    std::printf("Last log entry for t1 should be >=%d. Last log entry for t2 should be >=%d\n", t1c, t2c);
    g_log.await_panic_flush();
}
#elif defined(_WIN32)
LONG WINAPI exception_filter(_EXCEPTION_POINTERS*)
{
    int t1c = t1_count;
    int t2c = t2_count;
    g_log.start_panic_flush();
    std::printf("Last log entry for t1 should be >=%d. Last log entry for t2 should be >=%d\n", t1c, t2c);
    g_log.await_panic_flush();
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

int main()
{
#if defined(__unix__)
    struct sigaction act;
    std::memset(&act, 0, sizeof(act));
    act.sa_handler = &sigsegv_handler;
    sigfillset(&act.sa_mask);
    act.sa_flags = SA_RESETHAND;
    sigaction(SIGSEGV, &act, nullptr);
#elif defined(_WIN32)
    SetUnhandledExceptionFilter(&exception_filter);
#endif

    reckless::file_writer writer("log.txt");
    g_log.open(&writer);

    std::thread thread([]()
    {
        for(int i=0;i!=1000000;++i) {
            g_log.write("t2: %d", i);
            t2_count = i;
        }
    });

    for(int i=0;i!=100000;++i) {
        g_log.write("t1: %d", i);
        t1_count = i;
    }
    char* p = nullptr;
    *p = 0;
    return 0;
}
