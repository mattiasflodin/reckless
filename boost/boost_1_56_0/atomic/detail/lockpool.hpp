/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2011 Helge Bahmann
 * Copyright (c) 2013-2014 Andrey Semashev
 */
/*!
 * \file   atomic/detail/lockpool.hpp
 *
 * This header contains declaration of the lockpool used to emulate atomic ops.
 */

#ifndef BOOST_ATOMIC_DETAIL_LOCKPOOL_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_LOCKPOOL_HPP_INCLUDED_

#include <boost_1_56_0/atomic/detail/config.hpp>
#include <boost_1_56_0/atomic/detail/link.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost_1_56_0 {
namespace atomics {
namespace detail {

struct lockpool
{
    class scoped_lock
    {
        void* m_lock;

    public:
        explicit BOOST_ATOMIC_DECL scoped_lock(const volatile void* addr) BOOST_NOEXCEPT;
        BOOST_ATOMIC_DECL ~scoped_lock() BOOST_NOEXCEPT;

        BOOST_DELETED_FUNCTION(scoped_lock(scoped_lock const&))
        BOOST_DELETED_FUNCTION(scoped_lock& operator=(scoped_lock const&))
    };

    static BOOST_ATOMIC_DECL void thread_fence() BOOST_NOEXCEPT;
    static BOOST_ATOMIC_DECL void signal_fence() BOOST_NOEXCEPT;
};

} // namespace detail
} // namespace atomics
} // namespace boost_1_56_0

#endif // BOOST_ATOMIC_DETAIL_LOCKPOOL_HPP_INCLUDED_
