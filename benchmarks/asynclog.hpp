#include <asynclog.hpp>

#define BENCHMARK_INIT() \
    asynclog::file_writer writer("log.txt"); \
    asynclog::severity_log<asynclog::no_indent, ' ', asynclog::severity_field, asynclog::timestamp_field> log(&writer);
    
#define BENCHMARK_CLEANUP()

#define LOG( c, i, f ) log.info( "Hello World! %s %d %f\n", c, i, f)


