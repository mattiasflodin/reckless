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
    performance_log::logger<4096, performance_log::rdtscp_cpuid_clock, std::uint32_t> performance_log;

    {
        LOG_INIT();

        for(int i=0; i!=2500; ++i) {
            usleep(200);
            auto start = performance_log.start();
            LOG(c, i, pi);
            performance_log.stop(start);
        }
        
        LOG_CLEANUP();
    }
    performance_log::rdtscp_cpuid_clock::unbind_cpu();

    std::uint64_t sum = 0;
    for(auto sample : performance_log) {
        std::cout << sample << std::endl;
        sum += sample;
    }

    //std::cout << "Avg: " << static_cast<double>(sum)/performance_log.size() << std::endl;
    
    return 0;
    
}
