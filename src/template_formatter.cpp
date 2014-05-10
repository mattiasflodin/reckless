#include "asynclog.hpp"

#include <cstdio>
#include <cstring>
#include <sstream>

namespace asynclog {
namespace {
    template <typename T>
    char const* generic_format_int(output_buffer* pbuffer, char const*& pformat, T v)
    {
        char f = *pformat;
        if(f == 'd') {
            std::ostringstream ostr;
            ostr << v;
            std::string const& s = ostr.str();
            char* p = pbuffer->reserve(s.size());
            std::memcpy(p, s.data(), s.size());
            pbuffer->commit(s.size());
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

    int typesafe_sprintf(char* str, double v)
    {
        return std::sprintf(str, "%f", v);
    }

    int typesafe_sprintf(char* str, long double v)
    {
        return std::sprintf(str, "%Lf", v);
    }

    template <typename T>
    char const* generic_format_float(output_buffer* pbuffer, char const* pformat, T v)
    {
        char f = *pformat;
        if(f != 'd')
            return nullptr;

        T v_for_counting_digits = std::fabs(v);
        if(v_for_counting_digits < 1.0)
            v_for_counting_digits = static_cast<T>(1.0);
        std::size_t digits = static_cast<std::size_t>(std::log10(v_for_counting_digits)) + 1;
        // %f without precision modifier gives us 6 digits after the decimal point.
        // The format is [-]ddd.dddddd (minimum 9 chars). We can also get e.g.
        // "-infinity" but that won't be more chars than the digits.
        char* p = pbuffer->reserve(1+digits+1+6+1);
        int written = typesafe_sprintf(p, v);
        pbuffer->commit(written);
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

