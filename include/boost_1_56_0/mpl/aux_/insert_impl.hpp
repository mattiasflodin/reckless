
#ifndef BOOST_MPL_INSERT_IMPL_HPP_INCLUDED
#define BOOST_MPL_INSERT_IMPL_HPP_INCLUDED

// Copyright Aleksey Gurtovoy 2000-2004
//
// Distributed under the Boost Software License, Version 1.0. 
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/mpl for documentation.

// $Id$
// $Date$
// $Revision$

#include <boost_1_56_0/mpl/reverse_fold.hpp>
#include <boost_1_56_0/mpl/iterator_range.hpp>
#include <boost_1_56_0/mpl/clear.hpp>
#include <boost_1_56_0/mpl/push_front.hpp>
#include <boost_1_56_0/mpl/aux_/na_spec.hpp>
#include <boost_1_56_0/mpl/aux_/traits_lambda_spec.hpp>
#include <boost_1_56_0/type_traits/is_same.hpp>

namespace boost_1_56_0 { namespace mpl {

// default implementation; conrete sequences might override it by 
// specializing either the 'insert_impl' or the primary 'insert' template

template< typename Tag >
struct insert_impl
{
    template<
          typename Sequence
        , typename Pos
        , typename T
        >
    struct apply
    {
        typedef iterator_range<
              typename begin<Sequence>::type
            , Pos
            > first_half_;

        typedef iterator_range<
              Pos
            , typename end<Sequence>::type
            > second_half_;

        typedef typename reverse_fold<
              second_half_
            , typename clear<Sequence>::type
            , push_front<_,_>
            >::type half_sequence_;

        typedef typename reverse_fold<
              first_half_
            , typename push_front<half_sequence_,T>::type
            , push_front<_,_>
            >::type type;
    };
};

BOOST_MPL_ALGORITM_TRAITS_LAMBDA_SPEC(3,insert_impl)

}}

#endif // BOOST_MPL_INSERT_IMPL_HPP_INCLUDED
