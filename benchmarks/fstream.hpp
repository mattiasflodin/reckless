#include <fstream>

#define LOG_INIT() \
    std::ofstream ofs("log.txt")
    
#define LOG_CLEANUP()

#define LOG( c, i, f ) ofs << "Hello World! " << c << ' ' << i << ' ' << f << std::endl
