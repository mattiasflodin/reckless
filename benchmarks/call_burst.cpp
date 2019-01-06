#ifdef RECKLESS_ENABLE_TRACE_LOG
#include <performance_log/trace_log.hpp>
#include <fstream>
#endif

#include <performance_log/performance_log.hpp>
#include <iostream>
#include <unistd.h>

#include LOG_INCLUDE

char c = 'A';
float pi = 3.1415;
int main()
{
    unlink("log.txt");
    performance_log::rdtscp_cpuid_clock::bind_cpu(0);
    performance_log::logger<131072, performance_log::rdtscp_cpuid_clock, std::uint32_t> performance_log;

    {
        LOG_INIT();

        for(int i=0; i!=10000; ++i) {
            auto start = performance_log.start();
            LOG(c, i, pi);
            performance_log.stop(start);
        }

        LOG_CLEANUP();
    }
    performance_log::rdtscp_cpuid_clock::unbind_cpu();

    for(auto sample : performance_log) {
        std::cout << sample << std::endl;
    }
    
#ifdef RECKLESS_ENABLE_TRACE_LOG
    std::ofstream trace_log("trace_log.txt", std::ios::trunc);
    reckless::detail::g_trace_log.save(trace_log);
#endif

    return 0;
    
}
