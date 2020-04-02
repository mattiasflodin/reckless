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
#ifndef RECKLESS_DETAIL_PLATFORM_HPP
#define RECKLESS_DETAIL_PLATFORM_HPP

#ifndef RECKLESS_CACHE_LINE_SIZE
#define RECKLESS_CACHE_LINE_SIZE 64
#endif

#include <cstdint>  // uint64_t, int64_t
#include <type_traits>  // enable_if, underlying_type

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
            long _InterlockedIncrement(long volatile* Addend);
            __int64 _InterlockedCompareExchange64(__int64 volatile* Destination, __int64 Exchange, __int64 Comparand);
            long _InterlockedExchangeAdd(long volatile * Addend, long Value);
            void _mm_prefetch(char const* p, int i);
            void _mm_pause(void);
            void __cpuid(int[4], int);
            unsigned __int64 __rdtsc();
            unsigned __int64 __rdtscp(unsigned int *);
		 }
#        pragma intrinsic(_InterlockedIncrement)
#        pragma intrinsic(_InterlockedCompareExchange64)
#        pragma intrinsic(_InterlockedExchangeAdd)
#        pragma intrinsic(_mm_prefetch)
#        pragma intrinsic(_mm_pause)
#        pragma intrinsic(__rdtsc)
#        pragma intrinsic(__rdtscp)
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
    // TODO check what this does on gcc;
    return __atomic_load_n(pvalue, __ATOMIC_RELAXED);
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
void atomic_store_relaxed(T* ptarget, T value,
    typename std::enable_if<std::is_integral<T>::value>::type* = nullptr)
{
#if defined(__GNUC__)
    // TODO check what this does on gcc;
    __atomic_store_n(ptarget, value, __ATOMIC_RELAXED);
#elif defined(_MSC_VER)
    *ptarget = value;
#else
    static_assert(false, "atomic_store_relaxed is not implemented for this compiler");
#endif
}


template <typename T>
void atomic_store_relaxed(T* ptarget, T value,
    typename std::enable_if<std::is_enum<T>::value>::type* = nullptr)
{
    using UT = typename std::underlying_type<T>::type;
    atomic_store_relaxed(static_cast<UT*>(static_cast<void*>(ptarget)),
            static_cast<UT>(value));
}

template<typename T>
T atomic_load_acquire(T const* pvalue,
    typename std::enable_if<std::is_integral<T>::value>::type* = nullptr)
{
#if defined(__GNUC__)
    return __atomic_load_n(pvalue, __ATOMIC_ACQUIRE);
#elif defined(_MSC_VER)
    return *pvalue;
#else
    static_assert(false, "atomic_store_relaxed is not implemented for this compiler");
#endif
}

template <typename T>
T atomic_load_acquire(T const* pvalue,
    typename std::enable_if<std::is_enum<T>::value>::type* = nullptr)
{
    using UT = typename std::underlying_type<T>::type;
    return static_cast<T>(atomic_load_acquire(static_cast<UT const*>(
        static_cast<void const*>(pvalue))));
}

template <typename T>
void atomic_store_release(T* ptarget, T value,
    typename std::enable_if<std::is_integral<T>::value>::type* = nullptr)
{
#if defined(__GNUC__)
    __atomic_store_n(ptarget, value, __ATOMIC_RELEASE);
#elif defined(_MSC_VER)
    *ptarget = value;
#else
    static_assert(false, "atomic_store_release is not implemented for this compiler");
#endif
}

template <typename T>
void atomic_store_release(T* ptarget, T value,
    typename std::enable_if<std::is_enum<T>::value>::type* = nullptr)
{
    using UT = typename std::underlying_type<T>::type;
    return atomic_store_release(static_cast<UT*>(static_cast<void*>(ptarget)),
        static_cast<UT>(value));
}

#if defined(__GNUC__)

template <typename T, typename U>
T atomic_fetch_add_relaxed(T* ptarget, U value)
{
    return __atomic_fetch_add(ptarget, value, __ATOMIC_RELAXED);
}

template <typename T, typename U>
T atomic_fetch_add_release(T* ptarget, U value)
{
    return __atomic_fetch_add(ptarget, value, __ATOMIC_RELEASE);
}

#elif defined(_MSC_VER)

inline long atomic_fetch_add_relaxed(long* ptarget, long value)
{
    return _InterlockedExchangeAdd(ptarget, value);
}
inline long atomic_fetch_add_release(long* ptarget, long value)
{
    // TODO this can use _InterlockedExchangeAdd_rel on ARM.
    return _InterlockedExchangeAdd(ptarget, value);
}

inline int atomic_fetch_add_relaxed(int* ptarget, int value)
{
    return atomic_fetch_add_relaxed(reinterpret_cast<long*>(ptarget), static_cast<long>(value));
}

inline int atomic_fetch_add_release(int* ptarget, int value)
{
    return atomic_fetch_add_release(reinterpret_cast<long*>(ptarget), static_cast<long>(value));
}

inline unsigned atomic_fetch_add_relaxed(unsigned* ptarget, unsigned value)
{
    return atomic_fetch_add_relaxed(reinterpret_cast<int*>(ptarget), static_cast<int>(value));
}

inline unsigned atomic_fetch_add_release(unsigned* ptarget, unsigned value)
{
    return atomic_fetch_add_release(reinterpret_cast<int*>(ptarget), static_cast<int>(value));
}

#else
static_assert(false, "atomic_add_relaxed is not implemented for this compiler");
#endif

#if defined(__GNUC__)
template <typename T>
T atomic_increment_fetch_relaxed(T* ptarget)
{
    return __atomic_add_fetch(ptarget, 1, __ATOMIC_RELAXED);
}

#elif defined(_MSC_VER)
inline long atomic_increment_fetch_relaxed(int* ptarget)
{
    return _InterlockedIncrement(static_cast<long*>(static_cast<void*>(ptarget)));
}
inline long atomic_increment_fetch_relaxed(long* ptarget)
{
    return _InterlockedIncrement(ptarget);
}

inline unsigned atomic_increment_fetch_relaxed(unsigned* ptarget)
{
    return atomic_increment_fetch_relaxed(static_cast<int*>(static_cast<void*>(ptarget)));
}

#else
static_assert(false, "atomic_increment_fetch_relaxed is not implemented for this compiler");
#endif

#if defined(__GNUC__)
template<typename T>
bool atomic_compare_exchange_weak_relaxed(T* ptarget, T* pexpected, T desired)
{
    return __atomic_compare_exchange_n(ptarget, pexpected, desired, true,
        __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}
template<typename T>
bool atomic_compare_exchange_weak_relaxed(T* ptarget, T expected, T desired)
{
    return __atomic_compare_exchange_n(ptarget, &expected, desired, true,
        __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}
#elif defined(_MSC_VER)
inline bool atomic_compare_exchange_weak_relaxed(std::uint64_t* ptarget,
    std::uint64_t* pexpected, std::uint64_t desired)
{
    auto expected = *pexpected;
    std::uint64_t actual = _InterlockedCompareExchange64(
        reinterpret_cast<std::int64_t*>(ptarget),
        reinterpret_cast<std::int64_t const&>(desired),
        reinterpret_cast<std::int64_t const&>(expected));
    if (actual == expected) {
        return true;
    } else {
        *pexpected = actual;
        return false;
    }
}

inline bool atomic_compare_exchange_weak_relaxed(std::uint64_t* ptarget,
    std::uint64_t expected, std::uint64_t desired)
{
    std::uint64_t actual = _InterlockedCompareExchange64(
        reinterpret_cast<std::int64_t*>(ptarget),
        reinterpret_cast<std::int64_t const&>(desired),
        reinterpret_cast<std::int64_t const&>(expected));
    return actual == expected;
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

inline bool unlikely(bool expr) {
#ifdef __GNUC__
    return __builtin_expect(expr, false);
#else
    return expr;
#endif
}

inline void assume(bool condition)
{
#if defined(__GNUC__)
    // FIXME check that this is still working
    if(!condition)
        __builtin_unreachable();
#elif defined(_MSC_VER)
    __assume(condition);
#else
    static_assert(false, "assume() is not implemented for this compiler");
#endif
}

inline std::uint64_t rdtsc()
{
#if defined(__GNUC__)
    return __builtin_ia32_rdtsc();
#elif defined(_MSC_VER)
    return __rdtsc();
#else
    static_assert(false, "rdtsc() is not implemented for this compiler");
#endif
}

inline std::uint64_t serializing_performance_timestamp_begin()
{
#if defined(__GNUC__)
    std::uint64_t t_high;
    std::uint64_t t_low;
    std::uint64_t b, c;
    asm volatile(
        "cpuid\n\t"
        "rdtsc\n\t"
        : "=a"(t_low), "=b"(b), "=c"(c), "=d"(t_high));
    return (t_high << 32) | static_cast<std::uint32_t>(t_low);
#elif defined(_MSC_VER)
	int cpuinfo[4];
	__cpuid(cpuinfo, 0);
	__rdtsc();
#else
    static_assert(false, "serializing_performance_timestamp_begin() is not implemented for this compiler");
#endif
}

inline std::uint64_t serializing_performance_timestamp_end()
{
#if defined(__GNUC__)
    std::uint64_t t_high;
    std::uint64_t t_low;
    std::uint64_t aux;
    asm volatile("rdtscp\n\t" : "=a"(t_low), "=c"(aux), "=d"(t_high));
    std::uint64_t a, b, c, d;
    asm volatile("cpuid\n\t" : "=a"(a), "=b"(b), "=c"(c), "=d"(d));
    return (t_high << 32) | static_cast<std::uint32_t>(t_low);
#elif defined(_MSC_VER)
    unsigned int aux;
    __rdtscp(&aux);
    int cpuinfo[4];
    __cpuid(cpuinfo, 0);
#else
    static_assert(false, "serializing_performance_timestamp_end() is not implemented for this compiler");
#endif
}

template<int Locality = 3>
inline void prefetch(void const* ptr)
{
#if defined(__GNUC__)
    __builtin_prefetch(ptr, 0, Locality);
#elif defined(_MSC_VER)
    _mm_prefetch(static_cast<char const*>(ptr), Locality);
#else
    static_assert(false, "prefetch() is not implemented for this compiler");
#endif
}

inline void pause()
{
#if defined(__clang__)
    asm volatile("pause");
#elif defined(__GNUC__)
    __builtin_ia32_pause();
#elif defined(_MSC_VER)
    _mm_pause();
#else
    static_assert(false, "pause() is not implemented for this compiler");
#endif
}

unsigned get_page_size();
extern unsigned const page_size;

void set_thread_name(char const* name);

}   // detail
}   // reckless

#endif  // RECKLESS_DETAIL_PLATFORM_HPP
