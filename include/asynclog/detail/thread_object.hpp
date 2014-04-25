#include "branch_hints.hpp"

#include <pthread.h>

#include <memory>
#include <functional>
#include <cassert>
#include <new>
#include <tuple>

namespace asynclog {
namespace detail {

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

// TODO take an allocator argument here, somehow. The tricky part is that
// pthread_key_create takes a destroy function that doesn't have any additional
// context argument. All we get is the key value, so the allocator instance
// isn't available when we wish to destroy the contained object.
template <typename T, class... Args>
class thread_object {
private:
public:
    thread_object(Args const&... args) :
        args_(args...)
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
    T* create_and_get() const
    {
        typename make_index_sequence<sizeof...(Args)>::type indexes;
        T* p = create(indexes);
        int result = pthread_setspecific(key_, p);
        assert(result == 0);
        return p;
    }

    template <std::size_t... Indexes>
    T* create(index_sequence<Indexes...>) const
    {
        new T(std::get<Indexes>(args_)...);
    }

    static void destroy(void* p)
    {
        delete static_cast<T*>(p);
    }

    std::tuple<Args...> args_;
    pthread_key_t key_;
};

}   // detail
}   // asynclog
