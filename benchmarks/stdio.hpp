#include <cstdio>
#include <ctime>

#include <sys/time.h>

#define BENCHMARK_INIT() \
    FILE* file = std::fopen("log.txt", "w")
    
#define BENCHMARK_CLEANUP() \
    std::fclose(file)

inline void log(FILE* file, char c, int i, float f)
{
    timeval tv;
    gettimeofday(&tv, nullptr);
    struct tm tm;
    localtime_r(&tv.tv_sec, &tm);
    // "YYYY-mm-dd HH:MM:SS" -> 19 chars
    // Need 20 chars since we need a NUL char.
    char buf[20];
    std::strftime(buf, 20, "%Y-%m-%d %H:%M:%S.", &tm);
    std::fprintf(file, "%s.%03u Hello World! %c %d %f\n",
            buf, static_cast<unsigned>(tv.tv_usec)/1000u, c, i, f);
    std::fflush(file);
}

#define LOG(c, i, f) log(file, c, i, f)
