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

// TODO move this to separate program for measuring rdtsc behavior with
// frequency scaling
// #ifdef RECKLESS_ENABLE_TRACE_LOG
//     struct idle_tick_event :
//         public reckless::detail::timestamped_trace_event
//     {
//         std::string format() const
//         {
//             return timestamped_trace_event::format() + " idle_tick";
//         }
//     };
//     struct busy_tick_event :
//         public reckless::detail::timestamped_trace_event
//     {
//         std::string format() const
//         {
//             return timestamped_trace_event::format() + " busy_tick";
//         }
//     };


//     usleep(1000000);
//     for(int i=0; i!=10; i++) {
//         usleep(1000000);
//         RECKLESS_TRACE(idle_tick_event);
//     }
//     struct timeval start, now;
//     gettimeofday(&start, nullptr);
//     do {
//         gettimeofday(&now, nullptr);

//     } while(now.tv_sec == start.tv_sec);
//     for(int i=0; i!=10; i++) {
//         start = now;
//         do {
//             gettimeofday(&now, nullptr);

//         } while(now.tv_sec == start.tv_sec);
//         RECKLESS_TRACE(busy_tick_event);
//     }
// #endif

    {
        LOG_INIT();
        performance_log::rdtscp_cpuid_clock::bind_cpu(0);

        for(int i=0; i!=2500; ++i) {
            usleep(200);
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
