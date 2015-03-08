#include <performance_log.hpp>
#include <sstream>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>

#include LOG_INCLUDE

int main()
{
    unlink("log.txt");

    auto const full_access =
        S_IRUSR | S_IWUSR | S_IXUSR |
        S_IRGRP | S_IWGRP | S_IXGRP |
        S_IROTH | S_IWOTH | S_IXOTH;
    mkdir("data", full_access);

    performance_log::rdtscp_cpuid_clock::bind_cpu(0);
    performance_log::logger<16384, performance_log::rdtscp_cpuid_clock, std::uint32_t> performance_log;

    {
        LOG_INIT();

        for(unsigned number=0; number!=1000u; ++number) {
            std::ostringstream ostr;
            ostr << "data/" << number;
            std::ofstream ofs(ostr.str().c_str());

            auto start = performance_log.start();
            LOG_FILE_WRITE(number, 100.0*number/1000.0);
            performance_log.stop(start);

            for(std::size_t i=0; i!=1024u*1024u; ++i) {
                char const c = 'a';
                ofs.write(&c, 1);
            }
        }

        LOG_CLEANUP();
    }
    performance_log::rdtscp_cpuid_clock::unbind_cpu();

    for(auto sample : performance_log) {
        std::cout << sample << std::endl;
    }
    
    return 0;
    
}
