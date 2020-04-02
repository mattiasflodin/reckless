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
#include "performance_log/performance_log.hpp"

#include <system_error>
#include <cassert>

#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>

void performance_log::detail::lock_memory(void const* addr, std::size_t len)
{
    if(0 != mlock(addr, len))
        throw std::system_error(errno, std::system_category());
}
void performance_log::detail::unlock_memory(void const* addr, std::size_t len)
{
    munlock(addr, len);
}

void performance_log::rdtscp_cpuid_clock::bind_cpu(int cpu)
{
    int nprocessors = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    assert(cpu < nprocessors);
    auto const size = CPU_ALLOC_SIZE(nprocessors);
    cpu_set_t* pcpuset = CPU_ALLOC(nprocessors);
    if(not pcpuset)
        throw std::bad_alloc();
    try {
        CPU_ZERO_S(size, pcpuset);
        CPU_SET_S(cpu, size, pcpuset);
        int res = pthread_setaffinity_np(pthread_self(), size, pcpuset);
        if(res != 0)
            throw std::system_error(res, std::system_category());
    } catch(...) {
        CPU_FREE(pcpuset);
        throw;
    }
}

void performance_log::rdtscp_cpuid_clock::unbind_cpu()
{
    int nprocessors = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    auto const size = CPU_ALLOC_SIZE(nprocessors);
    cpu_set_t* pcpuset = CPU_ALLOC(nprocessors);
    if(not pcpuset)
        throw std::bad_alloc();
    try {
        CPU_ZERO_S(size, pcpuset);
        for(int i=0; i!=nprocessors; ++i)
            CPU_SET_S(i, size, pcpuset);
        int res = pthread_setaffinity_np(pthread_self(), size,  pcpuset);
        if(res != 0)
            throw std::system_error(res, std::system_category());
    } catch(...) {
        CPU_FREE(pcpuset);
        throw;
    }
}

#elif defined(_WIN32)

#include <Windows.h>

void performance_log::detail::lock_memory(void const* addr, std::size_t len)
{
    if (!VirtualLock(const_cast<void*>(addr), len)) {
        auto err = GetLastError();
        throw std::system_error(err, std::system_category());
    }
}

void performance_log::detail::unlock_memory(void const* addr, std::size_t len)
{
    VirtualUnlock(const_cast<void*>(addr), len);
}

void performance_log::rdtscp_cpuid_clock::bind_cpu(int cpu)
{
    DWORD_PTR mask = 1;
    mask <<= cpu;
    if (!SetThreadAffinityMask(GetCurrentThread(), mask)) {
        auto err = GetLastError();
        throw std::system_error(err, std::system_category());
    }
}

void performance_log::rdtscp_cpuid_clock::unbind_cpu()
{
    DWORD_PTR mask = 0;
    mask -= 1;
    if (!SetThreadAffinityMask(GetCurrentThread(), mask)) {
        auto err = GetLastError();
        throw std::system_error(err, std::system_category());
    }
}

#endif

