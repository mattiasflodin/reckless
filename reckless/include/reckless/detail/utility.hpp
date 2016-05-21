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
#ifndef RECKLESS_DETAIL_UTILITY_HPP
#define RECKLESS_DETAIL_UTILITY_HPP

#include <cstddef>  // size_t
#include <cstdint>  // uintptr_t

namespace reckless {
namespace detail {

#if defined(_WIN32)
#include <Windows.h>    // InterlockedCompareExchangeNoFence64
#endif

#ifndef RECKLESS_CACHE_LINE_SIZE
#define RECKLESS_CACHE_LINE_SIZE 64
#endif

// Chapter 7,  Part 3A - System Programming Guide of Intel's processor manuals:
// 8.1.1 Guaranteed Atomic Operations
// The Pentium processor (and newer processors since) guarantees that the following
// additional memory operations will always be carried out atomically :
// * Reading or writing a quadword aligned on a 64-bit boundary
//
// On IA-32 the VC++ stdlib goes through many hoops with interlocked Ors, loops etc
// to achieve atomicity when it really isn't necessary.
template <typename T>
T atomic_load_relaxed(T const* pvalue)
{
    return *pvalue;
}

template <typename T>
void atomic_store_relaxed(T* ptarget, T value)
{
    *ptarget = value;
}


template<typename T>
T atomic_load_acquire(T const* value)
{
#if defined(__GNUC__)
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
#elif defined(WIN32)
    TODO;
#endif
}

template<typename T>
void atomic_store_release(T* ptarget, T const& value)
{
#if defined(__GNUC__)
    __atomic_store_n(ptarget, value, __ATOMIC_RELEASE);
#elif defined(WIN32)
    TODO;
#endif
}

inline bool atomic_compare_exchange_weak_relaxed(std::uint64_t* ptarget, std::uint64_t expected, std::uint64_t desired)
{
#if defined(__GNUC__)
    return __atomic_compare_exchange_n(ptarget, &expected, desired, true,
            __ATOMIC_RELAXED, __ATOMIC_RELAXED);
#elif defined(_WIN32) // TODO change to _MSC_VER and skip windows.h
    return static_cast<LONGLONG const&>(expected) ==
        InterlockedCompareExchangeNoFence64(
            reinterpret_cast<LONGLONG*>(ptarget),
            reinterpret_cast<LONGLONG&>(desired),
            reinterpret_cast<LONGLONG&>(expected));
#endif
}

extern unsigned const cache_line_size;
std::size_t get_page_size() __attribute__ ((const));
void prefetch(void const* ptr, std::size_t size);

inline constexpr bool is_power_of_two(std::size_t v)
{
    return (v & (v - 1)) == 0;
}

template <std::size_t... Seq>
struct index_sequence
{
};

template <std::size_t Pos, std::size_t N, std::size_t... Seq>
struct make_index_sequence_helper
{
    typedef typename make_index_sequence_helper<Pos+1, N, Seq..., Pos>::type type;
};


template <std::size_t N, std::size_t... Seq>
struct make_index_sequence_helper<N, N, Seq...>
{
    typedef index_sequence<Seq...> type;
};

template <std::size_t N>
struct make_index_sequence
{
    typedef typename make_index_sequence_helper<0, N>::type type;
};

void unreachable() __attribute__((noreturn));
inline void unreachable()
{
    __builtin_trap();
}

}
}

#endif  // RECKLESS_DETAIL_UTILITY_HPP
