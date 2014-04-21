#ifndef ASYNCLOG_DETAIL_FRAME_HPP
#define ASYNCLOG_DETAIL_FRAME_HPP

#include "utility.hpp"

namespace asynclog {

class output_buffer;

namespace detail {

template <std::size_t Offset, typename Type>
struct bound_argument {
    using type = Type;
    static std::size_t const offset = Offset;
};

typedef std::size_t dispatch_function_t(output_buffer*, char*);
// TODO these checks need to be done at runtime now
//static_assert(alignof(dispatch_function_t*) <= ASYNCLOG_FRAME_ALIGNMENT,
//        "ASYNCLOG_FRAME_ALIGNMENT must at least match that of a function pointer");
//// We need the requirement below to ensure that, after alignment, there
//// will either be 0 free bytes available in the circular buffer, or
//// enough to fit a dispatch pointer. This simplifies the code a bit.
//static_assert(sizeof(dispatch_function_t*) <= FRAME_ALIGNMENT,
//        "ASYNCLOG_FRAME_ALIGNMENT must at least match the size of a function pointer");
dispatch_function_t* const WRAPAROUND_MARKER = reinterpret_cast<
    dispatch_function_t*>(0);

template <class Formatter, std::size_t FrameSize, class BoundArgs>
struct frame;
template <class Formatter, std::size_t FrameSize, class... BoundArgs>
struct frame<Formatter, FrameSize, typelist<BoundArgs...>> {
    template<typename... Args>
    static void store_args(char* pbuffer, Args&&... args)
    {
        *reinterpret_cast<dispatch_function_t**>(pbuffer) = &frame::dispatch;
        // TODO we should do this recursively probably, because g++
        // seems to push arguments backwards and i'm not sure that's
        // good for the cache. See wikipedia page on varadic templates,
        // it has info on how to get deterministic evaluation order.
        evaluate(new (pbuffer + BoundArgs::offset) typename BoundArgs::type(args)...);
    }
    static std::size_t dispatch(output_buffer* poutput, char* pinput)
    {
        Formatter::format(poutput,
            std::forward<typename BoundArgs::type>(
                *reinterpret_cast<typename BoundArgs::type*>(pinput + BoundArgs::offset)
            )...);
        evaluate{destroy(reinterpret_cast<typename BoundArgs::type*>(pinput + BoundArgs::offset))...};
        return FrameSize;
    }
};

template <class Accumulator, std::size_t Offset, class... Args>
struct bind_args_helper;
template <class Accumulator, std::size_t Offset>
struct bind_args_helper<Accumulator, Offset> {
    using type = Accumulator;
    static std::size_t const frame_size = Offset;
};
template <class... Accumulator, std::size_t Offset, class Arg, class... RemainingArgs>
struct bind_args_helper<typelist<Accumulator...>, Offset, Arg, RemainingArgs...> {
    using Value = typename std::remove_reference<typename make_pointer_from_array<Arg>::type>::type;
    static std::size_t const my_offset = (Offset + alignof(Value)-1)/alignof(Value)*alignof(Value);
    using next = bind_args_helper<
        typelist<Accumulator..., bound_argument<my_offset, Value>>,
        my_offset + sizeof(Value),
        RemainingArgs...>;
    using type = typename next::type;
    static std::size_t const frame_size = next::frame_size;
};

template <class... Args>
class bind_args {
private:
    // Note that we start not at offset 0 but at sizeof(dispatch_function_t*).
    // This is to make room for the pointer to frame::dispatch, which
    // takes care of picking up all the values from the buffer before
    // calling the proper output function.
    using helper = bind_args_helper<typelist<>, sizeof(dispatch_function_t*), Args...>;
public:
    using type = typename helper::type;
    static std::size_t const frame_size = helper::frame_size;
};

}
}

#endif  // ASYNCLOG_DETAIL_FRAME_HPP
