#include <asynclog.hpp>

#include <cstring>

__thread unsigned asynclog::scoped_indent::level_ = 0;

asynclog::writer::~writer()
{
}

