#include <cstddef>
#include <cstdint>
#include <cpuid.h>

namespace performance {
    namespace detail {
        void lock_memory(void const* addr, std::size_t len);
        void unlock_memory(void const* addr, std::size_t len);
    };

    // Evalutates timestamp counter using rdtscp/rdtsc/cpuid combo according to
    // "How to Benchmark Code Execution Times on Intel IA-32 and IA-64
    // Instruction Set Architectures" by Gabriele Paoloni
    // (http://www.intel.com/content/www/us/en/intelligent-systems/embedded-systems-training/ia-32-ia-64-benchmark-code-execution-paper.html)
    class rdtscp_cpuid_clock {
    public:
        typedef std::uint64_t timestamp;
        typedef std::uint64_t duration;

        timestamp start();
        timestamp end();
    };

    template <std::size_t LogSize, class ClockSource, class Sample = typename ClockSource::duration>
    class logger {
    public:
        typedef Sample sample;

        logger();
        ~logger();

    private:
        sample _samples[LogSize]; 
        std::size_t _next_sample_position;
    };
}

template <std::size_t LogSize, class ClockSource, class Sample>
performance::logger<LogSize, ClockSource, Sample>::logger()
{
}

template <std::size_t LogSize, class ClockSource, class Sample>
performance::logger<LogSize, ClockSource, Sample>::~logger()
{
}

inline auto performance::rdtscp_cpuid_clock::start() -> timestamp
{
    __builtin_ia32_cpuid();
    return __builtin_ia32_rdtsc();
}

inline auto performance::rdtscp_cpuid_clock::end() -> timestamp
{
    int dummy;
    auto res = __builtin_ia32_rdtscp(&dummy);
    __builtin_ia32_cpuid();
    return res;
}


