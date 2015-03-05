#include <asynclog.hpp>

#ifdef LOG_ONLY_DECLARE
extern asynclog::severity_log<asynclog::no_indent, ' ', asynclog::severity_field, asynclog::timestamp_field> g_log;
#else
       asynclog::severity_log<asynclog::no_indent, ' ', asynclog::severity_field, asynclog::timestamp_field> g_log;
#endif

#define LOG_INIT() \
    asynclog::file_writer writer("log.txt"); \
    g_log.open(&writer);
    
#define LOG_CLEANUP() g_log.close()

#define LOG( c, i, f ) g_log.info("Hello World! %s %d %f\n", c, i, f)

#define LOG_MANDELBROT(Thread, X, Y, FloatX, FloatY, Iterations) \
    g_log.info("[T%d] %d,%d/%f,%f: %d iterations\n", Thread, X, Y, FloatX, FloatY, Iterations)
