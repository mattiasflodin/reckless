#include <mutex>
#include <condition_variable>
#include <cstddef>
#include <iostream>

namespace dlog {
    namespace detail {
        struct buffer_descriptor {
            char* pfirst;
            char* plast;
            char* pflush_start;
            char* pflush_end;
            char* pwritten_end;
        };
        buffer_descriptor g_buffer; 
        //std::mutex buffer_mutex;

        template <std::size_t Offset>
        void push_args(char* pbuffer)
        {
        }

        template <std::size_t Offset, typename T, typename... Args>
        void push_args(char* pbuffer, T const& first, Args... rest)
        {
            std::size_t const write_offset =
                ((Offset + alignof(T) - 1)/alignof(T)) * alignof(T);
            new (pbuffer+write_offset) T(first);
            push_args<write_offset + sizeof(T), Args...>(pbuffer, rest...);
        }

        template <typename... Args>
        void dispatch(Args... args)
        {
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

    template <typename... Args>
    void write(Args... args)
    {
        using namespace dlog::detail;
        //std::unique_lock<std::mutex> hold(buffer_mutex);
        auto pdispatch = &dispatch<Args...>;
        std::size_t const frame_size = compute_frame_size<0u, decltype(pdispatch), Args...>::value;
        char* pwrite_start = align(g_buffer.pwritten_end,
                alignof(max_align_t));
        //while(pwrite_start + frame_size > plast)
        //    commit_finish_condition.wait();
        push_args<0u, decltype(pdispatch), Args...>(pwrite_start, pdispatch, args...);
        g_buffer.pwritten_end = pwrite_start + frame_size;
    }

    template <typename... Args>
    void line(Args... args)
    {
    }

    inline void flush()
    {
    }
}
