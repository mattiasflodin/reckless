#include <pantheios/pantheios.hpp>
#include <pantheios/frontends/fe.simple.h>
#include <pantheios/backends/bec.file.h>
#include <pantheios/inserters/character.hpp>
#include <pantheios/inserters/integer.hpp>
#include <pantheios/inserters/real.hpp>

#ifdef LOG_ONLY_DECLARE
#else
extern "C" char const PANTHEIOS_FE_PROCESS_IDENTITY[] = "periodic_calls";
#endif

#define LOG_INIT() \
    pantheios_be_file_setFilePath("log.txt"); \
    pantheios_fe_simple_setSeverityCeiling(PANTHEIOS_SEV_DEBUG)

#define LOG_CLEANUP()

#define LOG( c, i, f ) pantheios::log_INFORMATIONAL("Hello World! ", pantheios::character(c), " ", pantheios::integer(i), " ", pantheios::real(f))

#define LOG_FILE_WRITE(FileNumber, Percent) \
    pantheios::log_INFORMATIONAL("file", pantheios::integer(FileNumber), " (", pantheios::real(Percent), "%)")

#define LOG_MANDELBROT(Thread, X, Y, FloatX, FloatY, Iterations) \
    pantheios::log_INFORMATIONAL("[T", pantheios::integer(Thread), "] " , pantheios::integer(X) , "," , pantheios::integer(Y) , "/" , pantheios::real(FloatX) , "," , pantheios::real(FloatY) , ": " , pantheios::integer(Iterations) , " iterations")
