#ifndef ASYNCLOG_DETAIL_FORMATTER_HPP
#define ASYNCLOG_DETAIL_FORMATTER_HPP

#include "../output_buffer.hpp"

#include "basic_log.hpp"
#include "utility.hpp"

#include <tuple>    // tuple, get

namespace asynclog {
namespace detail {

typedef std::size_t formatter_dispatch_function_t(output_buffer*, char*);
// TODO these checks need to be done at runtime now
//static_assert(alignof(dispatch_function_t*) <= ASYNCLOG_FRAME_ALIGNMENT,
//        "ASYNCLOG_FRAME_ALIGNMENT must at least match that of a function pointer");
//// We need the requirement below to ensure that, after alignment, there
//// will either be 0 free bytes available in the circular buffer, or
//// enough to fit a dispatch pointer. This simplifies the code a bit.
//static_assert(sizeof(dispatch_function_t*) <= FRAME_ALIGNMENT,
//        "ASYNCLOG_FRAME_ALIGNMENT must at least match the size of a function pointer");
formatter_dispatch_function_t* const WRAPAROUND_MARKER = reinterpret_cast<
    formatter_dispatch_function_t*>(0);

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
