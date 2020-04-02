/* This file is part of reckless logging
 * Copyright 2015-2020 Mattias Flodin <git@codepentry.com>
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
#ifndef RECKLESS_DETAIL_UTILITY_HPP
#define RECKLESS_DETAIL_UTILITY_HPP

#include <cstddef>  // size_t
#include <cstdint>  // uintptr_t
#include <type_traits>  // enable_if, is_pointer, remove_pointer

namespace reckless {
namespace detail {

template <class Pointer, class Type>
struct replace_pointer_type;

template <class From, class To>
struct replace_pointer_type<From*, To>
{
    using type = To*;
};

template <class From, class To>
struct replace_pointer_type<From const*, To>
{
    using type = To const*;
};

template <class T>
T char_cast(char* p)
{
    return static_cast<T>(static_cast<void*>(p));
}

template <class T>
T char_cast(char const* p)
{
    return static_cast<T>(static_cast<void const*>(p));
}

template <class T>
typename replace_pointer_type<T, char>::type
char_cast(T p)
{
    using void_ptr_t = typename replace_pointer_type<T, void>::type;
    using char_ptr_t = typename replace_pointer_type<T, char>::type;
    return static_cast<char_ptr_t>(static_cast<void_ptr_t>(p));
}

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

#endif  // RECKLESS_DETAIL_UTILITY_HPP
