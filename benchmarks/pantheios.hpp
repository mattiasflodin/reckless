#include <pantheios/pantheios.hpp>
#include <pantheios/frontends/fe.simple.h>
#include <pantheios/backends/bec.file.h>
#include <pantheios/inserters/character.hpp>
#include <pantheios/inserters/integer.hpp>
#include <pantheios/inserters/real.hpp>

extern "C" char const PANTHEIOS_FE_PROCESS_IDENTITY[] = "periodic_calls";

#define LOG_INIT() \
    pantheios_be_file_setFilePath("log.txt"); \
    pantheios_fe_simple_setSeverityCeiling(PANTHEIOS_SEV_DEBUG)
    
#define LOG_CLEANUP()

#define LOG( c, i, f ) pantheios::log_INFORMATIONAL( "Hello World! ", pantheios::character(c), " ", pantheios::integer(i), " ", pantheios::real(f))
