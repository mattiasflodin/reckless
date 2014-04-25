#include "asynclog.hpp"

#include <cstring>

#include <unistd.h>

asynclog::writer::~writer()
{
}

// FIXME move to utility.cpp?
std::size_t asynclog::detail::get_page_size()
{
    long sz = sysconf(_SC_PAGESIZE);
    return sz;
}

std::size_t asynclog::detail::get_cache_line_size()
{
    // FIXME we need to make sure we have a power of two for the input buffer
    // alignment. On the off chance that we get something like 48 here (risk is
    // probably slim to none, but I'm a stickler), we need to use e.g. 32 or 64 instead.
    // If my maths is correct we won't ever be able to find a value which is
    // both a power of two and divisible by 48, so we just have to give up on
    // the goal of using cache line size for alignment.
    long sz = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
    return sz;
}
