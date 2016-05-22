/* This file is part of reckless logging
 * Copyright 2015, 2016 Mattias Flodin <git@codepentry.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <reckless/template_formatter.hpp>
#include <reckless/ntoa.hpp>

#include <cstdio>
#include <cstring>
#include <cstdint>

namespace reckless {
namespace {

    unsigned atou(char const*& s)
    {
        unsigned v = *s - '0';
        ++s;
        if(!isdigit(*s))
            return v;
        v = 10*v + *s - '0';
        ++s;
        while(isdigit(*s)) {
            v = 10*v + *s - '0';
            ++s;
        }
        return v;
    }

    bool isdigit(char c)
    {
        return c >= '0' && c <= '9';
    }
    
    // TODO for people writing custom format functions, it would be nice to
    // have access to this. Also being able to just skip past the conversion
    // specification so they can check the format character.
    char const* parse_conversion_specification(conversion_specification* pspec, char const* pformat)
    {
        bool left_justify = false;
        bool alternative_form = false;
        bool show_plus_sign = false;
        bool blank_sign = false;
        bool pad_with_zeroes = false;
        while(true) {
            char flag = *pformat;
            if(flag == '-')
                left_justify = true;
            else if(flag == '+')
                show_plus_sign = true;
            else if (flag == ' ')
                blank_sign = true;
            else if(flag == '#')
                alternative_form = true;
            else if(flag == '0')
                pad_with_zeroes = true;
            else
                break;
            ++pformat;
        }
        
        pspec->minimum_field_width = isdigit(*pformat)? atou(pformat) : 0;
        if(*pformat == '.') {
            ++pformat;
            pspec->precision = isdigit(*pformat)? atou(pformat) : UNSPECIFIED_PRECISION;
        } else {
            pspec->precision = UNSPECIFIED_PRECISION;
        }
        if(show_plus_sign)
            pspec->plus_sign = '+';
        else if(blank_sign)
            pspec->plus_sign = ' ';
        else
            pspec->plus_sign = 0;
        
        pspec->left_justify = left_justify;
        pspec->alternative_form = alternative_form;
        pspec->pad_with_zeroes = pad_with_zeroes;
        pspec->uppercase = false;
        
        return pformat;
    }
        
    template <typename T>
    char const* generic_format_int(output_buffer* pbuffer, char const* pformat, T v)
    {
        conversion_specification spec;
        pformat = parse_conversion_specification(&spec, pformat);
        char f = *pformat;
        if(f == 'd') {
            itoa_base10(pbuffer, v, spec);
            return pformat + 1;
        } else if(f == 'x') {
            spec.uppercase = false;
            itoa_base16(pbuffer, v, spec);
            return pformat + 1;
        } else if(f == 'X') {
            spec.uppercase = true;
            itoa_base16(pbuffer, v, spec);
            return pformat + 1;
        } else if(f == 'b') {
            // TODO
            return nullptr;
        } else {
            return nullptr;
        }
    }

    template <typename T>
    char const* generic_format_float(output_buffer* pbuffer, char const* pformat, T v)
    {
        conversion_specification cs;
        pformat = parse_conversion_specification(&cs, pformat);
        char f = *pformat;
        if(f != 'f')
            return nullptr;
        
        ftoa_base10_f(pbuffer, v, cs);
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

char const* format(output_buffer* pbuffer, char const* pformat, char v)
{
    return generic_format_char(pbuffer, pformat, v);
}

char const* format(output_buffer* pbuffer, char const* pformat, signed char v)
{
    return generic_format_char(pbuffer, pformat, v);
}

char const* format(output_buffer* pbuffer, char const* pformat, unsigned char v)
{
    return generic_format_char(pbuffer, pformat, v);
}

//char const* format(output_buffer* pbuffer, char const* pformat, wchar_t v);
//char const* format(output_buffer* pbuffer, char const* pformat, char16_t v);
//char const* format(output_buffer* pbuffer, char const* pformat, char32_t v);

char const* format(output_buffer* pbuffer, char const* pformat, short v)
{
    return generic_format_int(pbuffer, pformat, v);
}

char const* format(output_buffer* pbuffer, char const* pformat, unsigned short v)
{
    return generic_format_int(pbuffer, pformat, v);
}

char const* format(output_buffer* pbuffer, char const* pformat, int v)
{
    return generic_format_int(pbuffer, pformat, v);
}

char const* format(output_buffer* pbuffer, char const* pformat, unsigned int v)
{
    return generic_format_int(pbuffer, pformat, v);
}

char const* format(output_buffer* pbuffer, char const* pformat, long v)
{
    return generic_format_int(pbuffer, pformat, v);
}

char const* format(output_buffer* pbuffer, char const* pformat, unsigned long v)
{
    return generic_format_int(pbuffer, pformat, v);
}

char const* format(output_buffer* pbuffer, char const* pformat, long long v)
{
    return generic_format_int(pbuffer, pformat, v);
}

char const* format(output_buffer* pbuffer, char const* pformat, unsigned long long v)
{
    return generic_format_int(pbuffer, pformat, v);
}

char const* format(output_buffer* pbuffer, char const* pformat, float v)
{
    return generic_format_float(pbuffer, pformat, v);
}

char const* format(output_buffer* pbuffer, char const* pformat, double v)
{
    return generic_format_float(pbuffer, pformat, v);
}

char const* format(output_buffer* pbuffer, char const* pformat, long double v)
{
    return generic_format_float(pbuffer, pformat, v);
}

char const* format(output_buffer* pbuffer, char const* pformat, char const* v)
{
    char c = *pformat;
    if(c =='s') {
        auto len = std::strlen(v);
        char* p = pbuffer->reserve(len);
        std::memcpy(p, v, len);
        pbuffer->commit(len);
    } else if(c == 'p') {
        conversion_specification cs;
        cs.minimum_field_width = 0;
        cs.precision = 1;
        cs.plus_sign = 0;
        cs.left_justify = false;
        cs.alternative_form = true;
        cs.pad_with_zeroes = false;
        cs.uppercase = false;
        itoa_base16(pbuffer, reinterpret_cast<std::uintptr_t>(v), cs);
    } else {
        return nullptr;
    }

    return pformat + 1;
}

char const* format(output_buffer* pbuffer, char const* pformat, std::string const& v)
{
    if(*pformat != 's')
        return nullptr;
    auto len = v.size();
    char* p = pbuffer->reserve(len);
    std::memcpy(p, v.data(), len);
    pbuffer->commit(len);
    return pformat + 1;
}

char const* format(output_buffer* pbuffer, char const* pformat, void const* p)
{
    char c = *pformat;
    if(c != 'p' && c !='s')
        return nullptr;
    
    conversion_specification cs;
    cs.minimum_field_width = 0;
    cs.precision = 1;
    cs.plus_sign = 0;
    cs.left_justify = false;
    cs.alternative_form = true;
    cs.pad_with_zeroes = false;
    cs.uppercase = false;
    
    itoa_base16(pbuffer, reinterpret_cast<std::uintptr_t>(p), cs);
    return pformat+1;
}

void template_formatter::append_percent(output_buffer* pbuffer)
{
    auto p = pbuffer->reserve(1u);
    *p = '%';
    pbuffer->commit(1u);
}

char const* template_formatter::next_specifier(output_buffer* pbuffer,
        char const* pformat)
{
#ifdef _GNU_SOURCE
    while(true) {
        char const* pspecifier = strchrnul(pformat, '%');
        
        auto len = pspecifier - pformat;
        auto p = pbuffer->reserve(len);
        std::memcpy(p, pformat, len);
        pbuffer->commit(len);
        if(*pspecifier == '\0')
            return nullptr;

        pformat = pspecifier + 1;

        if(*pformat != '%')
            return pformat;

        // Found "%%". Add a single '%' and continue.
        ++pformat;
        append_percent(pbuffer);
    }
#else
    static_assert(false, "need replacement for strchrnul");
#endif
}

void template_formatter::format(output_buffer* pbuffer, char const* pformat)
{
    // There are no remaining arguments to format, so we will ignore additional
    // format specifications that might occur in the format string. However, we
    // still need to treat "%%" as "%". We'll iterate over on next_specifier
    // and when it finds a format specifier, we append a '%' and move on.
    while((pformat = next_specifier(pbuffer, pformat)) != nullptr)
        append_percent(pbuffer);
}

}   // namespace reckless
