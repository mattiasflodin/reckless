#include <performance_log/performance_log.hpp>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cassert>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include LOG_INCLUDE

int main()
{
    unsigned const LOG_ENTRIES = 2048;
    unsigned long const DATA_SIZE = 64*1024*1024*1024L;
    unlink("log.txt");

    auto const full_access =
        S_IRUSR | S_IWUSR | S_IXUSR |
        S_IRGRP | S_IWGRP | S_IXGRP |
        S_IROTH | S_IWOTH | S_IXOTH;
    mkdir("data", full_access);
    char data[1024*1024];
    std::memset(data, 0xcd, sizeof(data));

    performance_log::logger<2*2048, performance_log::rdtscp_cpuid_clock> performance_log;

    {
        LOG_INIT(128);
        performance_log::rdtscp_cpuid_clock::bind_cpu(0);

        for(unsigned number=0; number!=LOG_ENTRIES; ++number) {
            std::ostringstream ostr;
            ostr << "data/" << number;
            int fd = open(ostr.str().c_str(), O_WRONLY | O_CREAT, full_access);
            assert(fd != -1);

            auto start = performance_log.start();
            LOG_FILE_WRITE(number, 100.0*number/256.0);
            performance_log.stop(start);

            for(std::size_t i=0; i!=DATA_SIZE/LOG_ENTRIES/sizeof(data); ++i) {
                auto res = write(fd, data, sizeof(data));
                assert(res == sizeof(data));
            }
            close(fd);
        }

        performance_log::rdtscp_cpuid_clock::unbind_cpu();
        LOG_CLEANUP();
    }

    for(auto sample : performance_log) {
        std::cout << sample.start << ' ' << sample.stop << std::endl;
    }

    return 0;

}
