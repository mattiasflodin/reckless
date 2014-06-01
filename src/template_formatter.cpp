#include "asynclog.hpp"
#include "itoa.hpp"

#include <cstdio>
#include <cstring>
#include <sstream>

namespace asynclog {
namespace {
    template <typename T>
    char const* generic_format_uint(output_buffer* pbuffer, char const*& pformat, T v)
    {
        char f = *pformat;
        if(f == 'd') {
            detail::utoa_base10(pbuffer, v);
            return pformat + 1;
        } else if(f == 'x') {
            // FIXME
            return nullptr;
        } else if(f == 'b') {
            // FIXME
            return nullptr;
        } else {
            return nullptr;
        }
    }
    template <typename T>
    char const* generic_format_int(output_buffer* pbuffer, char const*& pformat, T v)
    {
        char f = *pformat;
        if(f == 'd') {
            detail::itoa_base10(pbuffer, v);
            return pformat + 1;
        } else if(f == 'x') {
            // FIXME
            return nullptr;
        } else if(f == 'b') {
            // FIXME
            return nullptr;
        } else {
            return nullptr;
        }
    }

    template <typename T>
    char const* generic_format_float(output_buffer* pbuffer, char const* pformat, T v)
    {
        char f = *pformat;
        if(f != 'd')
            return nullptr;

        detail::ftoa_base10_natural(pbuffer, v, 6u);
        return pformat + 1;
    }

    template <typename T>
    char const* generic_format_char(output_buffer* pbuffer, char const* pformat, T v)
    {
        char f = *pformat;
        if(f == 's') {
            char* p = pbuffer->reserve(1);
            *p = static_cast<char>(v);
            pbuffer->commit(1);
            return pformat + 1;
        } else {
            return generic_format_int(pbuffer, pformat, static_cast<int>(v));
        }
    }
}   // anonymous namespace
}   // namespace dlog

char const* asynclog::format(output_buffer* pbuffer, char const* pformat, char v)
{
    return generic_format_char(pbuffer, pformat, v);
}

char const* asynclog::format(output_buffer* pbuffer, char const* pformat, signed char v)
{
    return generic_format_char(pbuffer, pformat, v);
}

char const* asynclog::format(output_buffer* pbuffer, char const* pformat, unsigned char v)
{
    return generic_format_char(pbuffer, pformat, v);
}

//char const* asynclog::format(output_buffer* pbuffer, char const* pformat, wchar_t v);
//char const* asynclog::format(output_buffer* pbuffer, char const* pformat, char16_t v);
//char const* asynclog::format(output_buffer* pbuffer, char const* pformat, char32_t v);

char const* asynclog::format(output_buffer* pbuffer, char const* pformat, short v)
{
    return generic_format_int(pbuffer, pformat, v);
}

char const* asynclog::format(output_buffer* pbuffer, char const* pformat, unsigned short v)
{
    return generic_format_int(pbuffer, pformat, v);
}

char const* asynclog::format(output_buffer* pbuffer, char const* pformat, int v)
{
    return generic_format_int(pbuffer, pformat, v);
}

char const* asynclog::format(output_buffer* pbuffer, char const* pformat, unsigned int v)
{
    return generic_format_int(pbuffer, pformat, v);
}

char const* asynclog::format(output_buffer* pbuffer, char const* pformat, long v)
{
    return generic_format_int(pbuffer, pformat, v);
}

char const* asynclog::format(output_buffer* pbuffer, char const* pformat, unsigned long v)
{
    return generic_format_int(pbuffer, pformat, v);
}

char const* asynclog::format(output_buffer* pbuffer, char const* pformat, long long v)
{
    return generic_format_int(pbuffer, pformat, v);
}

char const* asynclog::format(output_buffer* pbuffer, char const* pformat, unsigned long long v)
{
    return generic_format_int(pbuffer, pformat, v);
}

char const* asynclog::format(output_buffer* pbuffer, char const* pformat, float v)
{
    return generic_format_float(pbuffer, pformat, v);
}

char const* asynclog::format(output_buffer* pbuffer, char const* pformat, double v)
{
    return generic_format_float(pbuffer, pformat, v);
}

char const* asynclog::format(output_buffer* pbuffer, char const* pformat, long double v)
{
    return generic_format_float(pbuffer, pformat, v);
}

char const* asynclog::format(output_buffer* pbuffer, char const* pformat, char const* v)
{
    if(*pformat != 's')
        return nullptr;
    auto len = std::strlen(v);
    char* p = pbuffer->reserve(len);
    std::memcpy(p, v, len);
    pbuffer->commit(len);
    return pformat + 1;
}

char const* asynclog::format(output_buffer* pbuffer, char const* pformat, std::string const& v)
{
    if(*pformat != 's')
        return nullptr;
    auto len = v.size();
    char* p = pbuffer->reserve(len);
    std::memcpy(p, v.data(), len);
    pbuffer->commit(len);
    return pformat + 1;
}

void asynclog::template_formatter::append_percent(output_buffer* pbuffer)
{
    auto p = pbuffer->reserve(1u);
    *p = '%';
    pbuffer->commit(1u);
}

char const* asynclog::template_formatter::next_specifier(output_buffer* pbuffer,
        char const* pformat)
{
    while(true) {
        char const* pspecifier = std::strchr(pformat, '%');
        if(pspecifier == nullptr) {
            format(pbuffer, pformat);
            return nullptr;
        }

        auto len = pspecifier - pformat;
        auto p = pbuffer->reserve(len);
        std::memcpy(p, pformat, len);
        pbuffer->commit(len);

        pformat = pspecifier + 1;

        if(*pformat != '%')
            return pformat;

        ++pformat;
        append_percent(pbuffer);
    }
}

void asynclog::template_formatter::format(output_buffer* pbuffer, char const* pformat)
{
    auto len = std::strlen(pformat);
    char* p = pbuffer->reserve(len);
    std::memcpy(p, pformat, len);
    pbuffer->commit(len);
}

