#ifndef ASYNCLOG_DETAIL_UTILITY_HPP
#define ASYNCLOG_DETAIL_UTILITY_HPP

#include <cstddef>  // size_t
#include <cstdint>  // uintptr_t

namespace asynclog {
namespace detail {

template <typename... Args>
struct typelist {
};

template <typename T>
struct make_pointer_from_array
{
    using type = T;
};

template <typename T, std::size_t Size>
struct make_pointer_from_array<T (&) [Size]>
{
    using type = T*;
};

template <typename T>
T align(T p, std::size_t alignment)
{
    std::uintptr_t v = reinterpret_cast<std::uintptr_t>(p);
    v = ((v + alignment-1)/alignment)*alignment;
    return reinterpret_cast<T>(v);
}

struct evaluate {
    template <class... Args>
    evaluate(Args&&...) {}
};

template <typename T>
int destroy(T* p)
{
    p->~T();
    return 0;
}

extern unsigned const cache_line_size;
std::size_t get_page_size() __attribute__ ((const));
// TODO try commentinig this out and see if code that depends on it can use
// cache_line_size instead.
std::size_t get_cache_line_size() __attribute__((const));
void prefetch(void const* ptr, std::size_t size);

inline constexpr bool is_power_of_two(std::size_t v)
{
    return (v & (v - 1)) == 0;
}

template <std::size_t... Seq>
struct index_sequence
{
};

template <std::size_t Pos, std::size_t N, std::size_t... Seq>
struct make_index_sequence_helper
{
    typedef typename make_index_sequence_helper<Pos+1, N, Seq..., Pos>::type type;
};


template <std::size_t N, std::size_t... Seq>
struct make_index_sequence_helper<N, N, Seq...>
{
    typedef index_sequence<Seq...> type;
};

template <std::size_t N>
struct make_index_sequence
{
    typedef typename make_index_sequence_helper<0, N>::type type;
};

}
}

#endif  // ASYNCLOG_DETAIL_UTILITY_HPP
