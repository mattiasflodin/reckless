#include <mutex>
#include <condition_variable>
#include <utility>
#include <iostream>

namespace dlog {
    namespace detail {
        // max_align_t is not available in clang?
        std::size_t const FRAME_ALIGNMENT = 16u;
        struct buffer_descriptor {
            char* pfirst;
            char* plast;
            char* pflush_start;
            char* pflush_end;
            char* pwritten_end;
        };
        buffer_descriptor g_buffer; 
        //std::mutex buffer_mutex;

        template <typename... Args>
        struct typelist {
        };

        template <typename T>
        struct make_pointer_from_array
        {
            using type = T;
        };

        template <typename T, std::size_t Size>
        struct make_pointer_from_array<T (&) [Size]>
        {
            using type = T*;
        };

        void dlog_output()
        {
        }
        template <typename T, typename... Args>
        void dlog_output(T&& first, Args&&... rest)
        {
            std::cout << first << std::endl;
            dlog_output(rest...);
        }

        template <typename T>
        T align(T p, std::size_t alignment)
        {
            std::uintptr_t v = reinterpret_cast<std::uintptr_t>(p);
            v = ((v + alignment-1)/alignment)*alignment;
            return reinterpret_cast<T>(v);
        }

        template <class... Args>
        void dummy(Args&&...)
        {
        }

        template <typename T>
        int destroy(T* p)
        {
            p->~T();
            return 0;
        }

        template <std::size_t Offset, typename Type>
        struct bound_argument {
            using type = Type;
            static std::size_t const offset = Offset;
        };

        template <class BoundArgs>
        struct frame;
        template <class... BoundArgs>
        struct frame<typelist<BoundArgs...>> {
            template<typename... Args>
            static void store_args(char* pbuffer, Args&&... args)
            {
                *reinterpret_cast<void (**)(char*)>(pbuffer) = &frame::dispatch;
                // TODO we should do this recursively probably, because g++
                // seems to push arguments backwards and i'm not sure that's
                // good for the cache.
                dummy(new (pbuffer + BoundArgs::offset) typename BoundArgs::type(args)...);
            }
            static void dispatch(char* pbuffer)
            {
                dlog_output(static_cast<typename BoundArgs::type&&>(
                        *reinterpret_cast<typename BoundArgs::type*>(pbuffer + BoundArgs::offset)
                    )...);
                dummy(destroy(reinterpret_cast<typename BoundArgs::type*>(pbuffer + BoundArgs::offset))...);
            }
        };

        template <class Accumulator, std::size_t Offset, class... Args>
        struct bind_args_helper;
        template <class Accumulator, std::size_t Offset>
        struct bind_args_helper<Accumulator, Offset> {
            using type = Accumulator;
            static std::size_t const frame_size = Offset;
        };
        template <class... Accumulator, std::size_t Offset, class Arg, class... RemainingArgs>
        struct bind_args_helper<typelist<Accumulator...>, Offset, Arg, RemainingArgs...> {
            using Value = typename std::remove_reference<Arg>::type;
            static std::size_t const my_offset = (Offset + alignof(Value)-1)/alignof(Value)*alignof(Value);
            using next = bind_args_helper<
                typelist<Accumulator..., bound_argument<my_offset, Value>>,
                my_offset + sizeof(Value),
                RemainingArgs...>;
            using type = typename next::type;
            static std::size_t const frame_size = next::frame_size;
        };

        template <class Args>
        class bind_args;
        template <class... Args>
        class bind_args<typelist<Args...>> {
        private:
            // Note that we start not at offset 0 but at sizeof(void (*)(char*)).
            // This is to make room for the pointer to frame::dispatch, which
            // takes care of picking up all the values from the buffer before
            // calling the proper output function.
            using helper = bind_args_helper<typelist<>, sizeof(void (*)(char*)), Args...>;
        public:
            using type = typename helper::type;
            static std::size_t const frame_size = helper::frame_size;
        };
    }

    void initialize()
    {
        using namespace detail;
        std::size_t const BUFFER_SIZE = 2048u;
        g_buffer.pfirst = new char[BUFFER_SIZE];
        g_buffer.plast = g_buffer.pfirst + BUFFER_SIZE;
        g_buffer.pflush_start = g_buffer.pfirst;
        g_buffer.pflush_end = g_buffer.pfirst;
        g_buffer.pwritten_end = g_buffer.pfirst;
    }

    template <typename... Args>
    void write(Args&&... args)
    {
        using namespace dlog::detail;
        //std::unique_lock<std::mutex> hold(buffer_mutex);
        using formal_args = typelist<typename make_pointer_from_array<Args>::type...>;
        using argument_binder = bind_args<formal_args>;
        //using bound_args = typename argument_binder::type;    // fails in gcc 4.7.3
        using bound_args = typename bind_args<formal_args>::type;
        std::size_t const frame_size = argument_binder::frame_size;
        using frame = detail::frame<bound_args>;

        char* pwrite_start = align(g_buffer.pwritten_end, FRAME_ALIGNMENT);
        //while(pwrite_start + frame_size > plast)
        //    commit_finish_condition.wait();
        //store_arg(bound_args(), 
        frame::store_args(pwrite_start, args...);
        g_buffer.pwritten_end = pwrite_start + frame_size;
        frame::dispatch(pwrite_start);
    }

    template <typename... Args>
    void line(Args... args)
    {
    }

    inline void flush()
    {
    }
}
