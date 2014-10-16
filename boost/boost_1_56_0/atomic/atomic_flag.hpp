/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2011 Helge Bahmann
 * Copyright (c) 2013 Tim Blechmann
 * Copyright (c) 2014 Andrey Semashev
 */
/*!
 * \file   atomic/atomic_flag.hpp
 *
 * This header contains definition of \c atomic_flag.
 */

#ifndef BOOST_ATOMIC_ATOMIC_FLAG_HPP_INCLUDED_
#define BOOST_ATOMIC_ATOMIC_FLAG_HPP_INCLUDED_

#include <boost_1_56_0/atomic/capabilities.hpp>
#include <boost_1_56_0/atomic/detail/operations.hpp>
#include <boost_1_56_0/atomic/detail/atomic_flag.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost_1_56_0 {

using atomics::atomic_flag;

} // namespace boost_1_56_0

#endif // BOOST_ATOMIC_ATOMIC_FLAG_HPP_INCLUDED_
