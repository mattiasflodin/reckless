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
    performance_log::rdtscp_cpuid_clock::bind_cpu(0);
    performance_log::logger<131072, performance_log::rdtscp_cpuid_clock, std::uint32_t> performance_log;

    {
        LOG_INIT();

        for(int i=0; i!=100000; ++i) {
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

    return 0;

}
