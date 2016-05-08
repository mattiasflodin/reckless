#ifndef RECKLESS_TEMPLATE_FORMATTER_HPP
#define RECKLESS_TEMPLATE_FORMATTER_HPP

#include <utility>    // forward
#include <string>
#include <type_traits>  // is_convertible

namespace reckless {

class output_buffer;
namespace detail {
    template <typename T>
    char const* invoke_custom_format(output_buffer* pbuffer,
        char const* pformat, T&& v);
}

class template_formatter {
public:
    static void format(output_buffer* pbuffer, char const* pformat);

    template <typename T, typename... Args>
    static void format(output_buffer* pbuffer, char const* pformat,
            T&& value, Args&&... args)
    {
        pformat = next_specifier(pbuffer, pformat);
        if(not pformat)
            return;

        char const* pnext_format = detail::invoke_custom_format(pbuffer,
                pformat, std::forward<T>(value));
        if(pnext_format)
            pformat = pnext_format;
        else
            append_percent(pbuffer);
        return template_formatter::format(pbuffer, pformat,
                std::forward<Args>(args)...);
    }

private:
    static void append_percent(output_buffer* pbuffer);
    static char const* next_specifier(output_buffer* pbuffer,
            char const* pformat);
};

char const* format(output_buffer* pbuffer, char const* pformat, char v);
char const* format(output_buffer* pbuffer, char const* pformat, signed char v);
char const* format(output_buffer* pbuffer, char const* pformat, unsigned char v);
char const* format(output_buffer* pbuffer, char const* pformat, wchar_t v);
char const* format(output_buffer* pbuffer, char const* pformat, char16_t v);
char const* format(output_buffer* pbuffer, char const* pformat, char32_t v);

char const* format(output_buffer* pbuffer, char const* pformat, short v);
char const* format(output_buffer* pbuffer, char const* pformat, unsigned short v);
char const* format(output_buffer* pbuffer, char const* pformat, int v);
char const* format(output_buffer* pbuffer, char const* pformat, unsigned int v);
char const* format(output_buffer* pbuffer, char const* pformat, long v);
char const* format(output_buffer* pbuffer, char const* pformat, unsigned long v);
char const* format(output_buffer* pbuffer, char const* pformat, long long v);
char const* format(output_buffer* pbuffer, char const* pformat, unsigned long long v);

char const* format(output_buffer* pbuffer, char const* pformat, float v);
char const* format(output_buffer* pbuffer, char const* pformat, double v);
char const* format(output_buffer* pbuffer, char const* pformat, long double v);

char const* format(output_buffer* pbuffer, char const* pformat, char const* v);
char const* format(output_buffer* pbuffer, char const* pformat, std::string const& v);

char const* format(output_buffer* pbuffer, char const* pformat, void const* p);

namespace detail {
// TODO can we invoke free format() using argument-dependent lookup without
// causing infinite recursion on this member function, without this
// intermediary kludge?
template <typename T>
char const* invoke_custom_format(output_buffer* pbuffer, char const* pformat, T&& v)
{
    typedef decltype(format(pbuffer, pformat, std::forward<T>(v))) return_type;
    static_assert(std::is_convertible<return_type, char const*>::value,
        "return type of format<T> must be char const*");
    return format(pbuffer, pformat, std::forward<T>(v));
}
}   // namesapce detail

}   // namespace reckless

#endif  // RECKLESS_TEMPLATE_FORMATTER_HPP
