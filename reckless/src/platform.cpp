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
#include <reckless/detail/platform.hpp>

#if defined(__unix__)
#include <pthread.h>    // pthread_setname_np, pthread_self
#endif
#if defined(__linux__)
#include <unistd.h> // sysconf
#endif
#if defined(_WIN32)
#include <Windows.h>    // GetSystemInfo
#endif

namespace reckless {
namespace detail {

unsigned get_page_size()
{
#if defined(__linux__)
    long sz = sysconf(_SC_PAGESIZE);
    return static_cast<unsigned>(sz);
#elif defined(_WIN32)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwPageSize;
#else
    static_assert(false, "get_page_size() is not implemented for this OS");
#endif
}

void set_thread_name(char const* name)
{
#if defined(__unix__)
    pthread_setname_np(pthread_self(), name);

#elif defined(_WIN32)
    // How to: Set a Thread Name in Native Code
    // https://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx

    DWORD const MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push,8)
    struct THREADNAME_INFO
    {
        DWORD dwType; // Must be 0x1000.
        LPCSTR szName; // Pointer to name (in user addr space).
        DWORD dwThreadID; // Thread ID (-1=caller thread).
        DWORD dwFlags; // Reserved for future use, must be zero.
    };
#pragma pack(pop)

    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = name;
    info.dwThreadID = static_cast<DWORD>(-1);
    info.dwFlags = 0;
    __try{
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR),
            (ULONG_PTR*)&info);
    }
    __except (EXCEPTION_EXECUTE_HANDLER){
    }
#endif  // _WIN32
}


unsigned const page_size = get_page_size();

}   // detail
}   // reckless
