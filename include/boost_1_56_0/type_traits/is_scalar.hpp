
//  (C) Copyright Steve Cleary, Beman Dawes, Howard Hinnant & John Maddock 2000.
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).
//
//  See http://www.boost.org/libs/type_traits for most recent version including documentation.

#ifndef BOOST_TT_IS_SCALAR_HPP_INCLUDED
#define BOOST_TT_IS_SCALAR_HPP_INCLUDED

#include <boost_1_56_0/type_traits/is_arithmetic.hpp>
#include <boost_1_56_0/type_traits/is_enum.hpp>
#include <boost_1_56_0/type_traits/is_pointer.hpp>
#include <boost_1_56_0/type_traits/is_member_pointer.hpp>
#include <boost_1_56_0/type_traits/detail/ice_or.hpp>
#include <boost_1_56_0/config.hpp>

// should be the last #include
#include <boost_1_56_0/type_traits/detail/bool_trait_def.hpp>

namespace boost_1_56_0 {

namespace detail {

template <typename T>
struct is_scalar_impl
{ 
   BOOST_STATIC_CONSTANT(bool, value =
      (::boost_1_56_0::type_traits::ice_or<
         ::boost_1_56_0::is_arithmetic<T>::value,
         ::boost_1_56_0::is_enum<T>::value,
         ::boost_1_56_0::is_pointer<T>::value,
         ::boost_1_56_0::is_member_pointer<T>::value
      >::value));
};

// these specializations are only really needed for compilers 
// without partial specialization support:
template <> struct is_scalar_impl<void>{ BOOST_STATIC_CONSTANT(bool, value = false ); };
#ifndef BOOST_NO_CV_VOID_SPECIALIZATIONS
template <> struct is_scalar_impl<void const>{ BOOST_STATIC_CONSTANT(bool, value = false ); };
template <> struct is_scalar_impl<void volatile>{ BOOST_STATIC_CONSTANT(bool, value = false ); };
template <> struct is_scalar_impl<void const volatile>{ BOOST_STATIC_CONSTANT(bool, value = false ); };
#endif

} // namespace detail

BOOST_TT_AUX_BOOL_TRAIT_DEF1(is_scalar,T,::boost_1_56_0::detail::is_scalar_impl<T>::value)

} // namespace boost_1_56_0

#include <boost_1_56_0/type_traits/detail/bool_trait_undef.hpp>

#endif // BOOST_TT_IS_SCALAR_HPP_INCLUDED
