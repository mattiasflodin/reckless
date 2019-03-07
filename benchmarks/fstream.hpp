#include <fstream>
#include <mutex>

#ifdef LOG_ONLY_DECLARE
extern std::mutex g_log_mutex;
extern std::ofstream g_log;
#else
       std::mutex g_log_mutex;
       std::ofstream g_log;
#endif

#define LOG_INIT(queue_size) \
    g_log.open("log.txt")

#define LOG_CLEANUP() \
    g_log.close()

#define LOG( c, i, f ) \
    g_log_mutex.lock(); \
    g_log << "Hello World! " << c << ' ' << i << ' ' << f << std::endl; \
    g_log_mutex.unlock()

#define LOG_FILE_WRITE(FileNumber, Percent) \
    g_log << "file " << FileNumber << " (" << Percent << "%)" << '\n' << std::flush

#define LOG_MANDELBROT(Thread, X, Y, FloatX, FloatY, Iterations) \
    g_log_mutex.lock(); \
    g_log << "[T" << Thread << "] " << X << ',' << Y << '/' << FloatX << ',' << FloatY << ": " << Iterations << " iterations" << std::endl; \
    g_log_mutex.unlock()
