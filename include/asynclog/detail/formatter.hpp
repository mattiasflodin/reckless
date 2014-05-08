#ifndef ASYNCLOG_DETAIL_FORMATTER_HPP
#define ASYNCLOG_DETAIL_FORMATTER_HPP

#include "asynclog/output_buffer.hpp"

#include "log_base.hpp"

#include "asynclog/detail/utility.hpp"

#include <tuple>    // tuple, get

namespace asynclog {
namespace detail {

template <class Formatter, typename... Args, std::size_t... Indexes>
void call_formatter(output_buffer* poutput, std::tuple<Args...>& args, index_sequence<Indexes...>)
{
    Formatter::format(poutput, std::move(std::get<Indexes>(args))...);
}

template <class Formatter, typename... Args>
std::size_t formatter_dispatch(output_buffer* poutput, char* pinput)
{
    using namespace detail;
    typedef std::tuple<Args...> args_t;
    std::size_t const args_align = alignof(args_t);
    std::size_t const args_offset = (sizeof(formatter_dispatch_function_t*) + args_align-1)/args_align*args_align;
    std::size_t const frame_size = args_offset + sizeof(args_t);
    args_t& args = *reinterpret_cast<args_t*>(pinput + args_offset);

    typename make_index_sequence<sizeof...(Args)>::type indexes;
    call_formatter<Formatter>(poutput, args, indexes);

    args.~args_t();
    return frame_size;
}

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

}
}

#endif  // ASYNCLOG_DETAIL_FORMATTER_HPP
