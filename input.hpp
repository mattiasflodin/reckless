#include <mutex>
#include <condition_variable>
#include <utility>
#include <iostream>

namespace dlog {
    namespace detail {
        // max_align_t is not available in clang?
        std::size_t const MAX_ALIGN = 16u;
        struct buffer_descriptor {
            char* pfirst;
            char* plast;
            char* pflush_start;
            char* pflush_end;
            char* pwritten_end;
        };
        buffer_descriptor g_buffer; 
        //std::mutex buffer_mutex;

        template <typename T>
        struct make_pointer_from_array_ref
        {
            using type = T;
        };

        template <typename T, std::size_t Size>
        struct make_pointer_from_array_ref<T (&) [Size]>
        {
            using type = T*;
        };


        template <std::size_t Offset>
        void push_args(char* pbuffer)
        {

        }

        template <std::size_t Offset, typename T, typename... Args>
        void push_args(char* pbuffer, T&& first, Args&&... rest)
        {
            using U = typename std::remove_reference<T>::type;
            std::size_t const write_offset =
                ((Offset + alignof(U) - 1)/alignof(U)) * alignof(U);
            new (pbuffer+write_offset) U(std::forward<T>(first));
            push_args<write_offset + sizeof(U), Args...>(pbuffer, std::forward<Args>(rest)...);
        }

        void dlog_output()
        {
        }
        template <typename T, typename... Args>
        void dlog_output(T&& first, Args... rest)
        {
            std::cout << first << std::endl;
            dlog_output(rest...);
        }

        template <class ArgumentInfo>
        typename ArgumentInfo::type&& pop_arg(char* pinput)
        {
            using T = typename ArgumentInfo::type;
            return static_cast<T&&>(*reinterpret_cast<T*>(pinput + ArgumentInfo::offset));
        }

        template <typename... Args>
        struct typelist {
        };

        template <std::size_t Offset, typename Type>
        struct ArgumentInfo {
            static std::size_t const offset = Offset;
            using type = Type;
        };

        template <std::size_t CurrentOffset,
                 class ArgumentInfoList,
                 typename... Args>
        struct arg_offsets;

        template <std::size_t CurrentOffset,
                 class ArgumentInfoList>
        struct arg_offsets<CurrentOffset, ArgumentInfoList>
        {
            using type = ArgumentInfoList;
        };

        template <std::size_t CurrentOffset,
                 class... ArgumentInfoList,
                 typename T, typename... RemainingArgs>
        struct arg_offsets<CurrentOffset, typelist<ArgumentInfoList...>,
            T, RemainingArgs...>
        
        {
            static std::size_t const my_offset = (CurrentOffset + alignof(T)-1)/alignof(T)*alignof(T);
            using type = typename arg_offsets<my_offset + sizeof(T),
                  typelist<ArgumentInfoList..., ArgumentInfo<my_offset, T>>,
                  RemainingArgs...>::type;
        };

        template <class ArgumentInfoList>
        struct DispatchHelper;
        template <class... ArgumentInfoList>
        struct DispatchHelper<typelist<ArgumentInfoList...>> {
            static void foo(char* pinput)
            {
                dlog_output(pop_arg<ArgumentInfoList>(pinput)...);
            }
        };

        template <typename... Args>
        void dispatch(char* pinput)
        {
            using ArgumentInfoList = typename arg_offsets<0u, typelist<>, Args...>::type;
            DispatchHelper<ArgumentInfoList>::foo(pinput);
        }


        template <std::size_t Offset, typename... Args>
        class compute_frame_size;
        
        template <std::size_t Offset, typename T, typename... Args>
        class compute_frame_size<Offset, T, Args...>
        {
        public:
            static std::size_t const value = compute_frame_size<
                ((Offset+alignof(T)-1)/alignof(T))*alignof(T) + sizeof(T),
                Args...>::value;
        };

        template <std::size_t Offset>
        class compute_frame_size<Offset> {
        public:
            static std::size_t const value = Offset;
        };

        template <typename T>
        T align(T p, std::size_t alignment)
        {
            std::uintptr_t v = reinterpret_cast<std::uintptr_t>(p);
            v = ((v + alignment-1)/alignment)*alignment;
            return reinterpret_cast<T>(v);
        }

        int doit(char* pbuffer);
        template <class... Args>
        void dummy(Args...)
        {
        }

        template <class Args, class BoundArgs>
        struct frame;
        
        template <class... Args, class... BoundArgs>
        struct frame<typelist<Args...>, typelist<BoundArgs...>> {
            static void store_args(char* pbuffer, Args... args)
            {
                dummy(new (pbuffer + BoundArgs::offset) int...);
            }
            static void dispatch(char* pbuffer);
        };

        template <std::size_t Offset, typename Type>
        struct bound_argument {
            using type = Type;
            static std::size_t const offset = Offset;
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
            using helper = bind_args_helper<typelist<>, 0u, Args...>;
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
        using formal_args = typelist<typename make_pointer_from_array_ref<Args>::type...>;
        using argument_binder = bind_args<formal_args>;
        using bound_args = typename argument_binder::type;
        std::size_t const frame_size = argument_binder::frame_size;
        using frame = detail::frame<formal_args, bound_args>;

        char* pwrite_start = align(g_buffer.pwritten_end, MAX_ALIGN);
        //while(pwrite_start + frame_size > plast)
        //    commit_finish_condition.wait();
        //store_arg(bound_args(), 
        frame::store_args(pwrite_start, args...);
        g_buffer.pwritten_end = pwrite_start + frame_size;
        //(*pdispatch)(pwrite_start + sizeof(pdispatch));
    }

    template <typename... Args>
    void line(Args... args)
    {
    }

    inline void flush()
    {
    }
}
