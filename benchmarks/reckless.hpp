#include <reckless/severity_log.hpp>
#include <reckless/file_writer.hpp>

#include <performance_log.hpp>

#ifdef LOG_ONLY_DECLARE
//extern reckless::severity_log<reckless::no_indent, ' ', reckless::severity_field, reckless::timestamp_field> g_log;
extern reckless::policy_log<reckless::no_indent, ' '> g_log;
#else
//reckless::severity_log<reckless::no_indent, ' ', reckless::severity_field, reckless::timestamp_field> g_log;
reckless::policy_log<reckless::no_indent, ' '> g_log;
#endif

#define LOG_INIT() \
    reckless::file_writer writer("log.txt"); \
    g_log.open(&writer);
    //performance_log::rdtscp_cpuid_clock::bind_cpu(0)

#define LOG_CLEANUP() g_log.close()

//#define LOG( c, i, f ) g_log.info("Hello World! %s %d %f", c, i, f)
#define LOG( c, i, f ) g_log.write("Hello World! %s %d %f", c, i, f)

#define LOG_FILE_WRITE(FileNumber, Percent) \
    g_log.write("file %d (%f%%)", FileNumber, Percent)
    //g_log.info("file %d (%f%%)", FileNumber, Percent)

#define LOG_MANDELBROT(Thread, X, Y, FloatX, FloatY, Iterations) \
    g_log.write("Hello world!")
//g_log.info("[T%d] %d,%d/%f,%f: %d iterations", Thread, X, Y, FloatX, FloatY, Iterations)
