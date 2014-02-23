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

        template <typename T>
        T align(T p, std::size_t alignment)
        {
            std::uintptr_t v = reinterpret_cast<std::uintptr_t>(p);
            v = ((v + alignment-1)/alignment)*alignment;
            return reinterpret_cast<T>(v);
        }

        template <class... Args>
        void evaluate(Args&&...)
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

        typedef std::size_t dispatch_function_t(char*, char*);
        template <class Formatter, std::size_t FrameSize, class BoundArgs>
        struct frame;
        template <class Formatter, std::size_t FrameSize, class... BoundArgs>
        struct frame<Formatter, FrameSize, typelist<BoundArgs...>> {
            template<typename... Args>
            static void store_args(char* pbuffer, Args&&... args)
            {
                *reinterpret_cast<dispatch_function_t**>(pbuffer) = &frame::dispatch;
                // TODO we should do this recursively probably, because g++
                // seems to push arguments backwards and i'm not sure that's
                // good for the cache.
                evaluate(new (pbuffer + BoundArgs::offset) typename BoundArgs::type(args)...);
            }
            static std::size_t dispatch(char* poutput, char* pinput)
            {
                Formatter::format(poutput,
                    std::forward<typename BoundArgs::type>(
                        *reinterpret_cast<typename BoundArgs::type*>(pinput + BoundArgs::offset)
                    )...);
                evaluate(destroy(reinterpret_cast<typename BoundArgs::type*>(pinput + BoundArgs::offset))...);
                return FrameSize;
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
            using Value = typename std::remove_reference<typename make_pointer_from_array<Arg>::type>::type;
            static std::size_t const my_offset = (Offset + alignof(Value)-1)/alignof(Value)*alignof(Value);
            using next = bind_args_helper<
                typelist<Accumulator..., bound_argument<my_offset, Value>>,
                my_offset + sizeof(Value),
                RemainingArgs...>;
            using type = typename next::type;
            static std::size_t const frame_size = next::frame_size;
        };

        template <class... Args>
        class bind_args {
        private:
            // Note that we start not at offset 0 but at sizeof(dispatch_function_t*).
            // This is to make room for the pointer to frame::dispatch, which
            // takes care of picking up all the values from the buffer before
            // calling the proper output function.
            using helper = bind_args_helper<typelist<>, sizeof(dispatch_function_t*), Args...>;
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

    template <class Formatter>
    class logger {
    public:
        template <typename... Args>
        static void write(Args&&... args)
        {
            using namespace dlog::detail;
            //std::unique_lock<std::mutex> hold(buffer_mutex);
            using argument_binder = bind_args<Args...>;
            //using bound_args = typename argument_binder::type;    // fails in gcc 4.7.3
            using bound_args = typename bind_args<Args...>::type;
            std::size_t const frame_size = argument_binder::frame_size;
            using frame = detail::frame<Formatter, frame_size, bound_args>;

            char* pwrite_start = align(g_buffer.pwritten_end, FRAME_ALIGNMENT);
            //while(pwrite_start + frame_size > plast)
            //    commit_finish_condition.wait();
            //store_arg(bound_args(), 
            frame::store_args(pwrite_start, std::forward<Args>(args)...);
            g_buffer.pwritten_end = pwrite_start + frame_size;
            char buf[2048];
            frame::dispatch(buf, pwrite_start);
            std::puts(buf);
        }
    };

    template <typename... Args>
    void line(Args... args)
    {
    }

    inline void flush()
    {
    }
}
