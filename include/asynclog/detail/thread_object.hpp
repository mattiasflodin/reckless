#include "branch_hints.hpp"

#include <pthread.h>

#include <memory>
#include <functional>
#include <cassert>
#include <new>

namespace asynclog {
namespace detail {

// TODO take an allocator argument here, somehow. The tricky part is that
// pthread_key_create takes a destroy function that doesn't have any additional
// context argument. All we get is the key value, so the allocator instance
// isn't available when we wish to destroy the contained object.
template <typename T>
class thread_object {
public:
    template <typename... Args>
    thread_object(Args&&... args) :
        create_(std::bind(&thread_object::create<Args...>, args...))
    {
        if(0 != pthread_key_create(&key_, &destroy))
            throw std::bad_alloc();
    }

    ~thread_object()
    {
        // TODO check that there's only one instance of the contained object.
        T* p = static_cast<T*>(pthread_getspecific(key_));
        delete p;
        int result = pthread_key_delete(key_);
        assert(result == 0);
    }

    T* get() const
    {
        T* p = static_cast<T*>(pthread_getspecific(key_));
        if(likely(p != nullptr))
            return p;
        else
            return create_and_get();
    }

    T* operator->() const
    {
        return get();
    }

private:
    T* create_and_get() const
    {
        T* p = create_();
        int result = pthread_setspecific(key_, p);
        assert(result == 0);
        return p;
    }

    template <typename... Args>
    static T* create(Args&&... args)
    {
        return new T(std::forward(args)...);
    }

    static void destroy(void* p)
    {
        delete static_cast<T*>(p);
    }

    std::function<T* ()> create_;
    pthread_key_t key_;
};

}   // detail
}   // asynclog
