#include "performance.hpp"
#include "dlog.hpp"

typedef dlog::logger<dlog::formatter> logger;

int main()
{
    dlog::file_writer writer("dlog.txt");
    dlog::initialize(&writer);
    performance::logger<16384, performance::rdtscp_cpuid_clock> performance_log;
    performance::rdtscp_cpuid_clock::bind_cpu(0);
    for(std::size_t i=0; i!=10000; ++i) {
        auto start = performance_log.start();
        logger::write("three numbers: %s %d %d\n", 'A', 66, i);
        dlog::flush();
        performance_log.end(start);
    }
    dlog::cleanup();
    performance::rdtscp_cpuid_clock::unbind_cpu();
    return 0;
}
