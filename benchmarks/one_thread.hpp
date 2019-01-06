#include <performance_log/performance_log.hpp>
#include <iostream>
#include <unistd.h>

char c = 'A';
float pi = 3.1415;
int main()
{
    unlink("log.txt");
    performance_log::logger<16384, performance_log::rdtscp_cpuid_clock, std::uint32_t> performance_log;

    {
        BENCHMARK_INIT();

        for(int i=0; i!=10000; ++i) {
            auto start = performance_log.start();
            LOG(c, i, pi);
            performance_log.stop(start);
        }

        BENCHMARK_CLEANUP();
    }

    for(auto sample : performance_log) {
        std::cout << sample << std::endl;
    }

    return 0;

}
