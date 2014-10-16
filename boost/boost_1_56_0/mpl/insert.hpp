
#ifndef BOOST_MPL_INSERT_HPP_INCLUDED
#define BOOST_MPL_INSERT_HPP_INCLUDED

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

#include <boost_1_56_0/mpl/insert_fwd.hpp>
#include <boost_1_56_0/mpl/sequence_tag.hpp>
#include <boost_1_56_0/mpl/aux_/insert_impl.hpp>
#include <boost_1_56_0/mpl/aux_/na_spec.hpp>
#include <boost_1_56_0/mpl/aux_/lambda_support.hpp>

namespace boost_1_56_0 { namespace mpl {

template<
      typename BOOST_MPL_AUX_NA_PARAM(Sequence)
    , typename BOOST_MPL_AUX_NA_PARAM(Pos_or_T)
    , typename BOOST_MPL_AUX_NA_PARAM(T)
    >
struct insert
    : insert_impl< typename sequence_tag<Sequence>::type >
        ::template apply< Sequence,Pos_or_T,T >
{
    BOOST_MPL_AUX_LAMBDA_SUPPORT(3,insert,(Sequence,Pos_or_T,T))
};

BOOST_MPL_AUX_NA_SPEC(3, insert)

}}

#endif // BOOST_MPL_INSERT_HPP_INCLUDED
