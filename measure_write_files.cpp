#include "performance.hpp"
#include "dlog.hpp"

#include <fstream>
#include <sstream>
#include <cstdlib>

#include <sys/stat.h>

typedef dlog::logger<dlog::formatter> logger;

template <class Fun>
void measure(Fun fun, char const* timings_file_name)
{
    auto const full_access =
        S_IRUSR | S_IWUSR | S_IXUSR |
        S_IRGRP | S_IWGRP | S_IXGRP |
        S_IROTH | S_IWOTH | S_IXOTH;
    mkdir("data", full_access);

    performance::logger<16384, performance::rdtscp_cpuid_clock, std::uint32_t> performance_log;

    for(unsigned number=0; number!=1000u; ++number) {
        std::ostringstream ostr;
        ostr << "data/" << number;
        std::ofstream ofs(ostr.str().c_str());
        auto start = performance_log.start();
        fun(number, 100.0*number/1000.0);
        performance_log.end(start);
        for(std::size_t i=0; i!=1024u*1024u; ++i) {
            char const c = 'a';
            ofs.write(&c, 1);
        }
    }

    std::ofstream timings(timings_file_name);
    for(auto sample : performance_log) {
        timings << sample << std::endl;
    }
}

int main()
{
    unlink("fstream.txt");
    unlink("stdio.txt");
    unlink("alog.txt");

    performance::rdtscp_cpuid_clock::bind_cpu(0);

    // Do one run in the hope of filling up the cache somewhat so that the
    // first case doesn't get so much of an advantage.
    measure([&](unsigned number, double percent)
        {
       }, "timings_write_file_noop.txt");

    std::system("/bin/sync");
    std::ofstream ofs("fstream.txt");
    measure([&](unsigned number, double percent)
        {
            ofs << "file " << number << " (" << percent << "%)" << std::endl;
       }, "timings_write_file_fstream.txt");
    ofs.close();

    std::system("/bin/sync");
    FILE* stdio_file = std::fopen("stdio.txt", "w");
    measure([&](unsigned number, double percent)
        {
            fprintf(stdio_file, "file %u (%f%%)\n", number, percent);
            fflush(stdio_file);
       }, "timings_write_file_stdio.txt");
    fclose(stdio_file);

    std::system("/bin/sync");
    dlog::file_writer writer("alog.txt");
    dlog::initialize(&writer);
    measure([&](unsigned number, double percent)
        {
        logger::write("file %d (%d%%)\n", number, percent);
        dlog::flush();
       }, "timings_write_file_alog.txt");
    dlog::cleanup();
    return 0;
}
