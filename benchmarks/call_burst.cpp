#ifdef RECKLESS_ENABLE_TRACE_LOG
#include <performance_log/trace_log.hpp>
#include <fstream>
#endif

#include <performance_log/performance_log.hpp>
#include <iostream>

#include LOG_INCLUDE

#if defined(__unix__)
#include <unistd.h>
void remove_file(char const* path)
{
    unlink(path);
}
#elif defined(_WIN32)
#include <Windows.h>
void remove_file(char const* path)
{
    DeleteFile(path);
}
#endif


char c = 'A';
float pi = 3.1415f;
int main()
{
    remove_file("log.txt");
    performance_log::logger<100000, performance_log::rdtscp_cpuid_clock> performance_log;

    {
        LOG_INIT(128);
        // It's important to set our CPU affinity *after* the log is
        // initialized, otherwise all threads will run on the same CPU.
        performance_log::rdtscp_cpuid_clock::bind_cpu(0);

        for(int i=0; i!=100000; ++i) {
            LOG(c, i, pi);
        }

        for(int i=0; i!=100000; ++i) {
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
