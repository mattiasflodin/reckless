#include <reckless/severity_log.hpp>
#include <reckless/file_writer.hpp>

#ifdef LOG_ONLY_DECLARE
extern reckless::severity_log<reckless::no_indent, ' ', reckless::severity_field, reckless::timestamp_field> g_log;
#else
       reckless::severity_log<reckless::no_indent, ' ', reckless::severity_field, reckless::timestamp_field> g_log;
#endif

#define LOG_INIT(queue_size) \
    reckless::file_writer writer("log.txt"); \
    g_log.open(&writer, 64*queue_size, 64*queue_size);

#define LOG_CLEANUP() g_log.close()

#define LOG( c, i, f ) g_log.info("Hello World! %s %d %f", c, i, f)

#define LOG_FILE_WRITE(FileNumber, Percent) \
    g_log.info("file %d (%f%%)", FileNumber, Percent)

#define LOG_MANDELBROT(Thread, X, Y, FloatX, FloatY, Iterations) \
    g_log.info("[T%d] %d,%d/%f,%f: %d iterations", Thread, X, Y, FloatX, FloatY, Iterations)
