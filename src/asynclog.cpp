#include "asynclog.hpp"

#include <cstring>

#include <unistd.h>

asynclog::writer::~writer()
{
}

std::size_t asynclog::detail::get_page_size()
{
    long sz = sysconf(_SC_PAGESIZE);
    return sz;
}
