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

        template <typename T>
        T&& pop_arg(char* pinput)
        {
            return static_cast<T&&>(*reinterpret_cast<T*>(pinput));
        }

        template <typename... Args>
        void dispatch(char* pinput)
        {
            dlog_output(pop_arg<Args>(pinput)...);
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
        auto const& pdispatch = &dispatch<typename std::remove_reference<typename make_pointer_from_array_ref<Args>::type>::type...>;
        std::size_t const frame_size = compute_frame_size<0u, decltype(pdispatch),
            typename make_pointer_from_array_ref<Args>::type...>::value;
        char* pwrite_start = align(g_buffer.pwritten_end, MAX_ALIGN);
        //while(pwrite_start + frame_size > plast)
        //    commit_finish_condition.wait();
        push_args<0u, decltype(pdispatch), typename make_pointer_from_array_ref<Args>::type...>(
                pwrite_start, pdispatch, std::forward<Args>(args)...);
        g_buffer.pwritten_end = pwrite_start + frame_size;
        (*pdispatch)(pwrite_start);
    }

    template <typename... Args>
    void line(Args... args)
    {
    }

    inline void flush()
    {
    }
}
