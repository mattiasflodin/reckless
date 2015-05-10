// Copyright Daniel Wallin 2006. Use, modification and distribution is
// subject to the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_PARAMETER_SET_060912_HPP
# define BOOST_PARAMETER_SET_060912_HPP

# include <boost_1_56_0/detail/workaround.hpp>

# if !BOOST_WORKAROUND(__BORLANDC__, BOOST_TESTED_AT(0x564)) \
  && !BOOST_WORKAROUND(__GNUC__, < 3)
#  include <boost_1_56_0/mpl/insert.hpp>
#  include <boost_1_56_0/mpl/set/set0.hpp>
#  include <boost_1_56_0/mpl/has_key.hpp>

namespace boost_1_56_0 { namespace parameter { namespace aux {

typedef mpl::set0<> set0;

template <class Set, class K>
struct insert_
{
    typedef typename mpl::insert<Set, K>::type type;
};

template <class Set, class K>
struct has_key_
{
    typedef typename mpl::has_key<Set, K>::type type;
};

}}} // namespace boost_1_56_0::parameter::aux

# else

#  include <boost_1_56_0/mpl/list.hpp>
#  include <boost_1_56_0/mpl/end.hpp>
#  include <boost_1_56_0/mpl/find.hpp>
#  include <boost_1_56_0/mpl/not.hpp>
#  include <boost_1_56_0/mpl/push_front.hpp>

namespace boost_1_56_0 { namespace parameter { namespace aux {

typedef mpl::list0<> set0;

template <class Set, class K>
struct insert_
{
    typedef typename mpl::push_front<Set, K>::type type;
};

template <class Set, class K>
struct has_key_
{
    typedef typename mpl::find<Set, K>::type iter;
    typedef mpl::not_<
        is_same<iter, typename mpl::end<Set>::type> 
    > type;
};

}}} // namespace boost_1_56_0::parameter::aux

# endif


#endif // BOOST_PARAMETER_SET_060912_HPP

