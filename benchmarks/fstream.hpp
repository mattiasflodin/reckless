#include <fstream>

#define BENCHMARK_INIT() \
    std::ofstream ofs("log.txt")
    
#define BENCHMARK_CLEANUP()

#define LOG( c, i, f ) ofs << "Hello World! " << c << ' ' << i << ' ' << f << std::endl
