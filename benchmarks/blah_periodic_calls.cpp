#include <spdlog/spdlog.h>
#include <mutex>

#define BENCHMARK_INIT() \
    spdlog::set_async_mode(4096); \
    auto logger = spdlog::create<spdlog::sinks::simple_file_sink_st>("log")
    
#define BENCHMARK_CLEANUP()

#define LOG( c, i, f ) logger->info( "Hello World! {} {} {}", c, i, f )

#include "benchmark_main.hpp"

