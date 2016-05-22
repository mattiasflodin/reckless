/* This file is part of reckless logging
 * Copyright 2015, 2016 Mattias Flodin <git@codepentry.com>
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
#ifndef RECKLESS_DETAIL_THREAD_OBJECT_HPP
#define RECKLESS_DETAIL_THREAD_OBJECT_HPP

#include "branch_hints.hpp"
#include "utility.hpp"

#include <pthread.h>
#include <errno.h>
#include <signal.h>     // raise

#include <memory>
#include <cassert>      // assert
#include <new>          // bad_alloc, ?
#include <tuple>        // tuple
#include <system_error> // system_error

namespace reckless {
namespace detail {
struct uninitialized_t {};
static uninitialized_t const uninitialized;

// TODO take an allocator argument here, somehow. The tricky part is that
// pthread_key_create takes a destroy function that doesn't have any additional
// context argument. All we get is the key value, so the allocator instance
// isn't available when we wish to destroy the contained object.
template <typename T, class... Args>
class thread_object {
private:
    // We need to store the key together with the object to be able to call
    // pthread_setspecific from inside the destructor function.
    struct holder : public T
    {
        template <class... Args2>
        holder(Args2&&... args) :
            T(std::forward<Args2>(args)...)
        {
        }
        pthread_key_t key;
    };

public:
    thread_object(uninitialized_t) :
        initialized_(false)
    {
    }
    thread_object(typename std::enable_if<sizeof...(Args)!=0>::type* = 0) :
        initialized_(false)
    {
    }
    thread_object(Args const&... args) :
        args_(args...),
        initialized_(true)
    {
        if(0 != pthread_key_create(&key_, &destroy))
            throw std::bad_alloc();
    }

    thread_object(thread_object&& other) :
        args_(std::move(other.args_)),
        key_(other.key_),
        initialized_(other.initialized_)
    {
        other.initialized_ = false;
    }

    ~thread_object()
    {
        // TODO check that there's only one instance of the contained object.
        if(not initialized_)
            return;
        holder* p = static_cast<holder*>(pthread_getspecific(key_));
        delete p;
        int result = pthread_key_delete(key_);
        assert(result == 0);
        (void) result;  /// avoid warning about unused variable when NDEBUG is defined
    }

    thread_object& operator=(thread_object&& other)
    {
        args_ = std::move(other.args_);
        key_ = other.key_;
        initialized_ = other.initialized_;
        other.initialized_ = false;
        return *this;
    }

    T* get() const
    {
        assert(initialized_);
        holder* p = static_cast<holder*>(pthread_getspecific(key_));
        if(likely(p != nullptr)) {
            return p;
        } else {
            return create_and_get();
        }
    }

    T* operator->() const
    {
        return get();
    }

private:
    thread_object(thread_object const&);               // not defined
    thread_object& operator=(thread_object const&);    // not defined

    T* create_and_get() const
    {
        typename make_index_sequence<sizeof...(Args)>::type indexes;
        holder* p = create(indexes);
        p->key = key_;
        // TODO put pthread calls in a cpp file to avoid #includes in global namespace, also to reduce code bloat
        int result = pthread_setspecific(key_, p);
        if(likely(result == 0))
            return p;
        else if(result == ENOMEM)
            throw std::bad_alloc();
        else
            throw std::system_error(result, std::system_category());
    }

    template <std::size_t... Indexes>
    holder* create(index_sequence<Indexes...>) const
    {
        return new holder(std::get<Indexes>(args_)...);
    }

    static void destroy(void* p)
    {
        if(not p)
            return;
        holder* qp = static_cast<holder*>(p);
        auto key = qp->key;
        // pthreads will already have set the value to null, meaning that a
        // call to thread_object::get() inside the object's dtor will create a
        // new instance of the contained object, and that's bad. The easiest
        // way around this is to temporarily set the value again for the dtor
        // call, then clear it. This will count towards
        // PTHREAD_DESTRUCTOR_ITERATIONS but I don't think that will be a
        // problem.
        if(0 != pthread_setspecific(key, p))
        {
            // I can think of three options for dealing with this failure:
            // 1) Throw an exception, but who is going to catch an exception
            //    during thread teardown? Also, the thread teardown can
            //    possibly be due to an exception, and in this case C++ will be
            //    really disappointed in us if we try to throw another one.
            // 2) Crash, e.g. via SIGSEGV.
            // 3) Leak the object.
            // None of these are good options, but if we're running out of
            // memory in the 21st century we are close to a crash in 999 times
            // of 1000 anyway. Linux, for example, doesn't even tell
            // applications that memory has run out, it just kills them. I'm
            // going to go with option 2 for now, because I prefer crashing to
            // hiding leaks that could trigger later failures.
            raise(SIGSEGV);
        }
        delete qp;
        pthread_setspecific(key, nullptr);
    }

    std::tuple<Args...> args_;
    pthread_key_t key_;
    bool initialized_;
};


}   // detail
}   // reckless

#endif  // RECKLESS_DETAIL_THREAD_OBJECT_HPP
