#include "dlog.hpp"
#include <cstring>

namespace {
    output_buffer g_output_buffer;
}

bool dlog::format(output_buffer* pbuffer, char const*& pformat, char const* v)
{
    if(*pformat != 's')
        return false;
    auto len = std::strlen(v);
    char* p = pbuffer->reserve(len);
    std::memcpy(p, v, len);
    pbuffer->commit(len);
    ++pformat;
}

bool dlog::format(output_buffer* pbuffer, char const* pformat, std::string const& v)
{
    if(*pformat != 's')
        return false;
    auto len = v.size();
    char* p = pbuffer->reserve(len);
    std::memcpy(p, v.data(), len);
    pbuffer->commit(len);
    ++pformat;
}

bool dlog::format(output_buffer* pbuffer, char const* pformat, std::uint8_t v);
bool dlog::format(output_buffer* pbuffer, char const* pformat, std::uint16_t v);
bool dlog::format(output_buffer* pbuffer, char const* pformat, std::uint32_t v);
bool dlog::format(output_buffer* pbuffer, char const* pformat, std::uint64_t v);
