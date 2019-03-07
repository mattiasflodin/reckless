#include <cstdio>
#include <ctime>
#include <utility>  // forward

#include <sys/time.h>

#ifdef LOG_ONLY_DECLARE
extern FILE* g_log;
#else
       FILE* g_log;
#endif

#define LOG_INIT(queue_size) \
    g_log = std::fopen("log.txt", "w")

#define LOG_CLEANUP() \
    std::fclose(g_log)

template <class... T>
void log(char const* fmt, T&&... args)
{
    timeval tv;
    gettimeofday(&tv, nullptr);
    struct tm tm;
    localtime_r(&tv.tv_sec, &tm);
    // "YYYY-mm-dd HH:MM:SS" -> 19 chars
    // Need 20 chars since we need a NUL char.
    char buf[20];
    std::strftime(buf, 20, "%Y-%m-%d %H:%M:%S.", &tm);

    std::fprintf(g_log, fmt, buf,
        static_cast<unsigned>(tv.tv_usec)/1000u, std::forward<T>(args)...);
    std::fflush(g_log);
}

#define LOG(c, i, f) log("%s.%03u Hello World! %c %d %f\n", c, i, f)

#define LOG_FILE_WRITE(FileNumber, Percent) \
    std::fprintf(g_log, "file %u (%f%%)\n", FileNumber, Percent); \
    std::fflush(g_log)

#define LOG_MANDELBROT(Thread, X, Y, FloatX, FloatY, Iterations) \
    log("%s.%03u [T%d] %d,%d/%f,%f: %d iterations\n", \
        Thread, X, Y, FloatX, FloatY, Iterations)
