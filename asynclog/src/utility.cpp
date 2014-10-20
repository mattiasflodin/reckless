#include "asynclog/detail/utility.hpp"

#include <unistd.h>

namespace asynclog {
namespace detail {
    
unsigned const cache_line_size = get_cache_line_size();

std::size_t get_page_size()
{
    long sz = sysconf(_SC_PAGESIZE);
    return sz;
}

std::size_t get_cache_line_size()
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

void prefetch(void const* ptr, std::size_t size)
{
    char const* p = static_cast<char const*>(ptr);
    unsigned i = 0;
    unsigned const stride = cache_line_size;
    while(i < size) {
        __builtin_prefetch(p + i);
        i += stride;
    }
}

}   // namespace detail
}   // namespace asynclog
