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
    performance_log::logger<4096, performance_log::rdtscp_cpuid_clock> performance_log;

    {
        LOG_INIT(6000);
        performance_log::rdtscp_cpuid_clock::bind_cpu(0);

        for(int i=0; i!=2500; ++i) {
            usleep(1000);
            struct timespec busywait_start;
            clock_gettime(CLOCK_REALTIME, &busywait_start);
            struct timespec busywait_end = busywait_start;
            while( (busywait_end.tv_sec - busywait_start.tv_sec)*1000000 + busywait_end.tv_nsec - busywait_start.tv_nsec < 1000 )
                clock_gettime(CLOCK_REALTIME, &busywait_end);

            auto start = performance_log.start();
            LOG(c, i, pi);
            performance_log.stop(start);
        }

        performance_log::rdtscp_cpuid_clock::unbind_cpu();
        LOG_CLEANUP();
    }

    for(auto sample : performance_log) {
        std::cout << sample.start << ' ' << sample.stop << std::endl;
    }

#ifdef RECKLESS_ENABLE_TRACE_LOG
    std::ofstream trace_log("trace_log.txt", std::ios::trunc);
    reckless::detail::g_trace_log.save(trace_log);
#endif

    return 0;

}
