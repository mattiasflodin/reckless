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
#include <reckless/detail/mpsc_ring_buffer.hpp>
#include <new>    // bad_alloc

#if defined(__linux__)
#include <sys/mman.h>   // mmap
#include <sys/ipc.h>    // shmget/shmat
#include <sys/shm.h>    // shmget/shmat
#include <sys/stat.h>   // S_IRUSR/S_IWUSR
#include <reckless/detail/utility.hpp>  // get_page_size

#include <errno.h>
#elif defined(_WIN32)
#include <Windows.h>
#endif

namespace {
    template <typename T>
    T round_nearest_power_of_2(T v, typename std::enable_if<sizeof(T) == 4>::type* = nullptr)
    {
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v++;
        return v;
    }

    template <typename T>
    T round_nearest_power_of_2(T v, typename std::enable_if<sizeof(T) == 8>::type* = nullptr)
    {
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v |= v >> 32;
        v++;
        return v;
    }

    std::size_t round_capacity(std::size_t capacity)
    {
#if defined(__linux__)
        std::size_t const granularity = reckless::detail::get_page_size();
#elif defined(_WIN32)
        std::size_t const granularity = 64*1024;
#endif
        auto n_segments = (capacity + granularity - 1)/granularity;
        n_segments = round_nearest_power_of_2(n_segments);
        return n_segments*granularity;
    }
}   // anonymous namespace

namespace reckless {
namespace detail {

void mpsc_ring_buffer::init(std::size_t capacity)
{
    if(capacity == 0) {
        rewind();
        pbuffer_start_ = nullptr;
        capacity_ = 0;
        return;
    }

    capacity = round_capacity(capacity);

#if defined(__linux__)
    int shm = shmget(IPC_PRIVATE, capacity, IPC_CREAT | S_IRUSR | S_IWUSR);
    if(shm == -1)
        throw std::bad_alloc();

    void* pbase = nullptr;
    while(!pbase) {
        pbase = mmap(nullptr, 2*capacity, PROT_NONE,
                MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if(MAP_FAILED == pbase)
            throw std::bad_alloc();
        munmap(pbase, 2*capacity);
        pbase = shmat(shm, pbase, 0);
        if(pbase) {
            if((void*)-1 == shmat(shm, static_cast<char*>(pbase) + capacity, 0))
            {
                shmdt(pbase);
                pbase = nullptr;
            }
        }
    }
    shmctl(shm, IPC_RMID, nullptr);

#elif defined(_WIN32)
    HANDLE mapping = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr,
        PAGE_READWRITE, static_cast<DWORD>((capacity >> 31)>>1),
        static_cast<DWORD>(capacity & 0xffffffff), nullptr);
    if(mapping == NULL)
        throw std::bad_alloc();

    void* pbase = nullptr;
    while(!pbase) {
        pbase = VirtualAlloc(0, 2*capacity, MEM_RESERVE, PAGE_NOACCESS);
        if (!pbase) {
            CloseHandle(mapping);
            throw std::bad_alloc();
        }
        VirtualFree(pbase, 0, MEM_RELEASE);
        pbase = MapViewOfFileEx(mapping, FILE_MAP_ALL_ACCESS, 0, 0, capacity,
                pbase);
        if(pbase) {
            if(!MapViewOfFileEx(mapping, FILE_MAP_ALL_ACCESS, 0, 0, capacity,
                                static_cast<char*>(pbase) + capacity))
            {
                UnmapViewOfFile(pbase);
                pbase = nullptr;
            }
        }
    }
    CloseHandle(mapping);
#endif

    rewind();
    std::memset(pbase, 0, capacity);
    pbuffer_start_ = static_cast<char*>(pbase);
    capacity_ = capacity;
}

void mpsc_ring_buffer::destroy()
{
    if(!pbuffer_start_)
        return;
#if defined(__linux__)
    shmdt(pbuffer_start_ + capacity_);
    shmdt(pbuffer_start_);

#elif defined(_WIN32)
    UnmapViewOfFile(pbuffer_start_ + capacity_);
    UnmapViewOfFile(pbuffer_start_);
#endif
}

}   // namespace detail
}   // namespace reckless
