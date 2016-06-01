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
#ifndef RECKLESS_DETAIL_PLATFORM_HPP
#define RECKLESS_DETAIL_PLATFORM_HPP

#ifndef RECKLESS_CACHE_LINE_SIZE
#define RECKLESS_CACHE_LINE_SIZE 64
#endif

#include <cstdint>  // uint64_t, int64_t

#if defined(__GNUC__)
#define RECKLESS_TLS __thread
#elif defined(_MSC_VER)
#define RECKLESS_TLS __declspec(thread)
#else
// Note that __thread/declspec(thread) is not exactly the same as C++11
// thread_local since the latter has a runtime penalty. See
// https://stackoverflow.com/questions/13106049/c11-gcc-4-8-thread-local-performance-penalty
// https://gcc.gnu.org/gcc-4.8/changes.html#cxx
static_assert(false, "RECKLESS_TLS is not implemented for this compiler")
#endif

#if defined(_MSC_VER)
#    if defined(_M_IX86) || defined(_M_X64)
         extern "C" {
             __int64 _InterlockedCompareExchange64(__int64 volatile* Destination, __int64 Exchange, __int64 Comparand);
             void _mm_prefetch(char const* p, int i);
             void _mm_pause(void);
         }
#        pragma intrinsic(_InterlockedCompareExchange64)
#        pragma intrinsic(_mm_prefetch)
#        pragma intrinsic(_mm_pause)
#    else
         static_assert(false, "Only x86/x64 support is implemented for this compiler");
#    endif
#endif

namespace reckless {
namespace detail {

template <typename T>
T atomic_load_relaxed(T const* pvalue)
{
#if defined(__GNUC__)
    static_assert(false, "check what this does on gcc")
    return __atomic_load_n(value, __ATOMIC_RELAXED);
#elif defined(_MSC_VER)
    // Chapter 7,  Part 3A - System Programming Guide of Intel's processor manuals:
    // 8.1.1 Guaranteed Atomic Operations
    // The Pentium processor (and newer processors since) guarantees that the following
    // additional memory operations will always be carried out atomically :
    // * Reading or writing a quadword aligned on a 64-bit boundary
    //
    // On IA-32 the VC++ stdlib goes through many hoops with interlocked Ors, loops etc
    // to achieve atomicity when it really isn't necessary. Same applies for other atomic
    // load/store functions.
    return *pvalue;
#else
    static_assert(false, "atomic_load_relaxed is not implemented for this compiler");
#endif
}

template <typename T>
void atomic_store_relaxed(T* ptarget, T value)
{
#if defined(__GNUC__)
    static_assert(false, "check what this does on gcc");
    __atomic_store_n(ptarget, value, __ATOMIC_RELAXED);
#elif defined(_MSC_VER)
    *ptarget = value;
#else
    static_assert(false, "atomic_store_relaxed is not implemented for this compiler");
#endif
}


template<typename T>
T atomic_load_acquire(T const* pvalue)
{
#if defined(__GNUC__)
    return __atomic_load_n(pvalue, __ATOMIC_ACQUIRE);
#elif defined(_MSC_VER)
    return *pvalue;
#else
    static_assert(false, "atomic_store_relaxed is not implemented for this compiler");
#endif
}

template<typename T>
void atomic_store_release(T* ptarget, T const& value)
{
#if defined(__GNUC__)
    __atomic_store_n(ptarget, value, __ATOMIC_RELEASE);
#elif defined(_MSC_VER)
    *ptarget = value;
#else
    static_assert(false, "atomic_store_release is not implemented for this compiler");
#endif
}

#if defined(__GNUC__)
template<typename T>
bool atomic_compare_exchange_weak_relaxed(T* ptarget, T expected, T desired)
{
    return __atomic_compare_exchange_n(ptarget, &expected, desired, true,
        __ATOMIC_RELAXED, __ATOMIC_RELAXED);
    return expected == _InterlockedCompareExchange64(ptarget, desired, expected);
}
#elif defined(_MSC_VER)
inline bool atomic_compare_exchange_weak_relaxed(std::uint64_t* ptarget,
    std::uint64_t expected, std::uint64_t desired)
{
        return reinterpret_cast<std::int64_t const&>(expected) ==
            _InterlockedCompareExchange64(
            reinterpret_cast<std::int64_t*>(ptarget),
            reinterpret_cast<std::int64_t const&>(desired),
            reinterpret_cast<std::int64_t const&>(expected));
}
#else
    static_assert(false, "atomic_compare_exchange_weak_relaxed is not implemented for this platform")
#endif

inline bool likely(bool expr) {
#ifdef __GNUC__
    return __builtin_expect(expr, true);
#else
    return expr;
#endif
}

inline void assume(bool condition)
{
#if defined(__GNUC__)
    XXX check that this is still working
    if(condition)
        __builtin_unreachable();
#elif defined(_MSC_VER)
    __assume(condition);
#else
    static_assert(false, "assume() is not implemented for this compiler");
#endif
}

template<int Locality = 3>
inline void prefetch(void const* ptr)
{
#if defined(__GNUC__)
    __builtin_prefetch(p, 0, Locality);
#elif defined(_MSC_VER)
    _mm_prefetch(static_cast<char const*>(ptr), Locality);
#else
    static_assert(false, "prefetch() is not implemented for this compiler");
#endif
}

inline void pause()
{
    _mm_pause();
}

unsigned get_page_size();
extern unsigned const page_size;

void set_thread_name(char const* name);


}   // detail
}   // reckless

#endif  // RECKLESS_DETAIL_PLATFORM_HPP
