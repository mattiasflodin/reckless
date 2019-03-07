#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#ifdef LOG_ONLY_DECLARE
extern std::shared_ptr<spdlog::logger> g_logger;
#else
       std::shared_ptr<spdlog::logger> g_logger;
#endif

// A design constraint / prerequisite for our library is to minimize the
// risk of messages getting lost. That means we want to flush on every
// write, not keep stuff in the stdio buffer. Hence we set
// force_flush=true here.

// In the reckless input buffer, each log line in the mandelbrot
// benchmark takes up 88 bytes, which rounds to 128 bytes when aligned
// to a cache line. This means our 64 kb input buffer fits 512 log
// messages. So in an attempt to make the benchmark fair we set the
// same number of log entries for spdlog's buffer.
#define LOG_INIT(queue_size) \
    spdlog::init_thread_pool(queue_size, 1); \
    spdlog::set_pattern("%L %Y-%m-%d %H:%M:%S.%e %v"); \
    g_logger = spdlog::basic_logger_mt<spdlog::async_factory>("log", "log.txt", true)

#define LOG_CLEANUP() \
    g_logger.reset()

#define LOG( c, i, f ) g_logger->info("Hello World! {} {} {}", c, i, f)

#define LOG_FILE_WRITE(FileNumber, Percent) \
    g_logger->info("file {} ({}%)", FileNumber, Percent)

#define LOG_MANDELBROT(Thread, X, Y, FloatX, FloatY, Iterations) \
    g_logger->info("[T{}] {},{}/{},{}: {} iterations", Thread, X, Y, FloatX, FloatY, Iterations)
