#ifndef ASYNCLOG_DETAIL_UTILITY_HPP
#define ASYNCLOG_DETAIL_UTILITY_HPP

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
template <typename T>
bool is_aligned(T p, std::size_t alignment)
{
    std::uintptr_t v = reinterpret_cast<std::uintptr_t>(p);
    return v % alignment == 0;
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

std::size_t get_page_size();

std::size_t get_cache_line_size();

inline bool is_power_of_two(std::size_t v)
{
    return (v & (v - 1)) == 0;
}

}
}

#endif  // ASYNCLOG_DETAIL_UTILITY_HPP
