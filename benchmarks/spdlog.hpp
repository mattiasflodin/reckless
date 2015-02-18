#include <spdlog/spdlog.h>

// A design constraint / prerequisite for our library is to minimize the
// risk of messages getting lost. That means we want to flush on every
// write, not keep stuff in the stdio buffer. Hence we set
// force_flush=true here.
#define BENCHMARK_INIT() \
    spdlog::set_async_mode(128); \
    auto logger = spdlog::create<spdlog::sinks::simple_file_sink_st>("log", "log.txt", true)
    
#define BENCHMARK_CLEANUP()

#define LOG( c, i, f ) logger->info( "Hello World! {} {} {}", c, i, f )
