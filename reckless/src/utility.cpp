#include "reckless/detail/utility.hpp"

#include <unistd.h>

namespace {
std::size_t get_cache_line_size()
{
    long sz = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
    return sz;
}
}   // anonymous namespace

namespace reckless {
namespace detail {
    
unsigned const cache_line_size = get_cache_line_size();

std::size_t get_page_size()
{
    long sz = sysconf(_SC_PAGESIZE);
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
}   // namespace reckless
