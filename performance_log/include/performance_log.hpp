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
#ifndef PERFORMANCE_LOG_HPP
#define PERFORMANCE_LOG_HPP
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace performance_log {
namespace detail {
    void lock_memory(void const* addr, std::size_t len);
    void unlock_memory(void const* addr, std::size_t len);
}

// Evaluates timestamp counter using rdtscp/rdtsc/cpuid combo according to
// "How to Benchmark Code Execution Times on Intel IA-32 and IA-64
// Instruction Set Architectures" by Gabriele Paoloni
// (http://www.intel.com/content/www/us/en/intelligent-systems/embedded-systems-training/ia-32-ia-64-benchmark-code-execution-paper.html)
class rdtscp_cpuid_clock {
public:
    typedef std::uint64_t timestamp;
    typedef std::uint64_t duration;

    timestamp start() const;
    timestamp stop() const;

    static void bind_cpu(int cpu);
    static void unbind_cpu();
};

template <std::size_t LogSize, class ClockSource, class Sample = typename ClockSource::duration>
class logger : private ClockSource {
public:
    typedef typename ClockSource::timestamp timestamp;
    typedef typename ClockSource::duration duration;
    typedef Sample sample;

    logger();
    ~logger();

    timestamp start() const
    {
        return ClockSource::start();
    }
    void stop(timestamp start_timestamp)
    {
        sample d = static_cast<sample>(ClockSource::stop() - start_timestamp);
        auto i = _next_sample_position;
        _samples[i] = d;
        _next_sample_position = (i + 1) % LogSize;
    }

    sample const* begin() const
    {
        return _samples;
    }

    sample const* end() const
    {
        return _samples + _next_sample_position;
    }

    std::size_t size() const
    {
        return _next_sample_position;
    }

private:
    sample _samples[LogSize];
    std::size_t _next_sample_position;
};
}

template <std::size_t LogSize, class ClockSource, class Sample>
performance_log::logger<LogSize, ClockSource, Sample>::logger() :
    _next_sample_position(0)
{
    //detail::lock_memory(_samples, sizeof(_samples));
    std::memset(_samples, 0, sizeof(_samples));
}

template <std::size_t LogSize, class ClockSource, class Sample>
performance_log::logger<LogSize, ClockSource, Sample>::~logger()
{
    //detail::unlock_memory(_samples, sizeof(_samples));
}

inline auto performance_log::rdtscp_cpuid_clock::start() const -> timestamp
{
    std::uint64_t t_high;
    std::uint64_t t_low;
    std::uint64_t b, c;
    asm volatile(
            "cpuid\n\t"
            "rdtsc\n\t"
            : "=a"(t_low), "=b"(b), "=c"(c), "=d"(t_high));

    return (t_high << 32) | static_cast<std::uint32_t>(t_low);
}

inline auto performance_log::rdtscp_cpuid_clock::stop() const -> timestamp
{
    std::uint64_t t_high;
    std::uint64_t t_low;
    std::uint64_t aux;
    asm volatile("rdtscp\n\t" : "=a"(t_low), "=c"(aux), "=d"(t_high));
    std::uint64_t a, b, c, d;
    asm volatile("cpuid\n\t" : "=a"(a), "=b"(b), "=c"(c), "=d"(d));

    return (t_high << 32) | static_cast<std::uint32_t>(t_low);
}

#endif // PERFORMANCE_LOG_HPP
