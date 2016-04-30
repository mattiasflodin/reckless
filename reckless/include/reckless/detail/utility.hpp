#ifndef RECKLESS_DETAIL_UTILITY_HPP
#define RECKLESS_DETAIL_UTILITY_HPP

#include <cstddef>  // size_t
#include <cstdint>  // uintptr_t

namespace reckless {
namespace detail {

extern unsigned const cache_line_size;
std::size_t get_page_size() __attribute__ ((const));
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

void unreachable() __attribute__((noreturn));
inline void unreachable()
{
    __builtin_trap();
}

}
}

#endif  // RECKLESS_DETAIL_UTILITY_HPP
