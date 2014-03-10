#define USE_THREAD_LOCAL
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <utility>
#include <deque>

#include <cstring>

namespace dlog {
    template <class Formatter>
    class logger;
    class formatter;
    class writer {
    public:
        enum Result
        {
            SUCCESS,
            ERROR_TRY_LATER,
            ERROR_GIVE_UP
        };
        virtual ~writer() = 0;
        virtual Result write(void const* pbuffer, std::size_t count) = 0;
    };

    class file_writer : public writer {
    public:
        file_writer(char const* path);
        ~file_writer();
        Result write(void const* pbuffer, std::size_t count);
    private:
        int fd_;
    };

    class output_buffer {
    public:
        output_buffer(writer* pwriter, std::size_t max_capacity);
        ~output_buffer();
        char* reserve(std::size_t size);
        void commit(std::size_t size);
        void flush();

    private:
        writer* pwriter_;
        char* pbuffer_;
        char* pcommit_end_;
        char* pbuffer_end_;
    };

    void initialize(writer* pwriter);
    void initialize(writer* pwriter, std::size_t input_buffer_size, std::size_t max_output_buffer_size);
    void cleanup();

    //class initializer {
    //public:
    //    initializer(writer* pwriter)
    //    {
    //        initialize(pwriter);
    //    }
    //    initializer(writer* pwriter, std::size_t max_capacity)
    //    {
    //        initialize(pwriter, max_capacity);
    //    }
    //    ~initializer()
    //    {
    //        cleanup();
    //    }
    //};

    namespace detail {
        class input_buffer;
        struct flush_extent {
            input_buffer* pinput_buffer;
            char* pflush_end;
        };
        // 64 bytes is the common cache-line size on modern x86 CPUs, which
        // avoids false sharing between the main thread and the output thread.
        // TODO we could perhaps detect the cache-line size instead (?).
        std::size_t const FRAME_ALIGNMENT = 64u;
        extern std::mutex g_input_queue_mutex;
        extern std::deque<flush_extent> g_input_queue;
        extern std::condition_variable g_input_available_condition;
        class input_buffer {
        public:
            input_buffer();
            ~input_buffer();
            char* allocate_input_frame(std::size_t size);
            // returns pointer to following input frame
            char* discard_input_frame(std::size_t size);
            char* wraparound();
            char* input_start() const;
            char* input_end() const;

        private:
            static char* allocate_buffer();
            char* advance_frame_pointer(char* p, std::size_t distance);
            void wait_input_consumed();
            void signal_input_consumed();

            std::mutex mutex_;
            std::condition_variable input_consumed_event_;

            char* const pbegin_; // fixed value
            std::atomic<char*> pinput_start_; // moved forward by output thread, read by logger::write (to determine free space left)
            char* pinput_end_;                // moved forward by logger::write, never read by anyone else
        };
#ifdef USE_THREAD_LOCAL
        extern thread_local input_buffer tls_input_buffer; 
#else
        extern input_buffer* tls_pinput_buffer; 
#endif
        input_buffer* get_input_buffer();
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

        // TODO are both of these still in use?
        template <typename T>
        T align(T p, std::size_t alignment)
        {
            std::uintptr_t v = reinterpret_cast<std::uintptr_t>(p);
            v = ((v + alignment-1)/alignment)*alignment;
            return reinterpret_cast<T>(v);
        }
        template <typename T>
        T align_down(T p, std::size_t alignment)
        {
            std::uintptr_t v = reinterpret_cast<std::uintptr_t>(p);
            v = (v/alignment)*alignment;
            return reinterpret_cast<T>(v);
        }
        template <typename T>
        bool is_aligned(T p, std::size_t alignment)
        {
            std::uintptr_t v = reinterpret_cast<std::uintptr_t>(p);
            return v % alignment == 0;
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

        typedef std::size_t dispatch_function_t(output_buffer*, char*);
        static_assert(alignof(dispatch_function_t*) <= FRAME_ALIGNMENT,
                "FRAME_ALIGNMENT must at least match that of a function pointer");
        // We need the requirement below to ensure that, after alignment, there
        // will either be 0 free bytes available in the circular buffer, or
        // enough to fit a disptch pointer. This simplifies the code a bit.
        static_assert(sizeof(dispatch_function_t*) <= FRAME_ALIGNMENT,
                "FRAME_ALIGNMENT must at least match the size of a function pointer");
        dispatch_function_t* const WRAPAROUND_MARKER = reinterpret_cast<
            dispatch_function_t*>(0);

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
                // good for the cache. See wikipedia page on varadic templates,
                // it has info on how to get deterministic evaluation order.
                evaluate(new (pbuffer + BoundArgs::offset) typename BoundArgs::type(args)...);
            }
            static std::size_t dispatch(output_buffer* poutput, char* pinput)
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

    template <typename... Args>
    void line(Args... args)
    {
    }

    inline void flush()
    {
        using namespace detail;
        auto pib = get_input_buffer();
        {
            std::unique_lock<std::mutex> lock(g_input_queue_mutex);
            g_input_queue.push_back({pib, pib->input_end()});
        }
        g_input_available_condition.notify_one();
    }

    bool format(output_buffer* pbuffer, char const*& pformat, char v);
    bool format(output_buffer* pbuffer, char const*& pformat, signed char v);
    bool format(output_buffer* pbuffer, char const*& pformat, unsigned char v);
    bool format(output_buffer* pbuffer, char const*& pformat, wchar_t v);
    bool format(output_buffer* pbuffer, char const*& pformat, char16_t v);
    bool format(output_buffer* pbuffer, char const*& pformat, char32_t v);

    bool format(output_buffer* pbuffer, char const*& pformat, short v);
    bool format(output_buffer* pbuffer, char const*& pformat, unsigned short v);
    bool format(output_buffer* pbuffer, char const*& pformat, int v);
    bool format(output_buffer* pbuffer, char const*& pformat, unsigned int v);
    bool format(output_buffer* pbuffer, char const*& pformat, long v);
    bool format(output_buffer* pbuffer, char const*& pformat, unsigned long v);
    bool format(output_buffer* pbuffer, char const*& pformat, long long v);
    bool format(output_buffer* pbuffer, char const*& pformat, unsigned long long v);

    bool format(output_buffer* pbuffer, char const*& pformat, float v);
    bool format(output_buffer* pbuffer, char const*& pformat, double v);
    bool format(output_buffer* pbuffer, char const*& pformat, long double v);

    bool format(output_buffer* pbuffer, char const*& pformat, char const* v);
    bool format(output_buffer* pbuffer, char const*& pformat, std::string const& v);

    template <typename T>
    bool invoke_custom_format(output_buffer* pbuffer, char const*& pformat, T&& v)
    {
        return format(pbuffer, pformat, std::forward<T>(v));
    }
}
template <class Formatter>
class dlog::logger {
public:
    template <typename... Args>
    static void write(Args&&... args)
    {
        using namespace dlog::detail;
        //std::unique_lock<std::mutex> hold(buffer_mutex);
        using argument_binder = bind_args<Args...>;
        // fails in gcc 4.7.3
        //using bound_args = typename argument_binder::type;
        using bound_args = typename bind_args<Args...>::type;
        std::size_t const frame_size = argument_binder::frame_size;
        using frame = detail::frame<Formatter, frame_size, bound_args>;

        auto pib = get_input_buffer();
        char* pframe = pib->allocate_input_frame(frame_size);
        frame::store_args(pframe, std::forward<Args>(args)...);
    }
};

class dlog::formatter {
public:
    // TODO this should perhapss be non-inline
    static void format(output_buffer* pbuffer, char const* pformat)
    {
        auto len = std::strlen(pformat);
        char* p = pbuffer->reserve(len);
        std::memcpy(p, pformat, len);
        pbuffer->commit(len);
    }

    template <typename T, typename... Args>
    static void format(output_buffer* pbuffer, char const* pformat,
            T&& value, Args&&... args)
    {
        while(true) {
            pformat = next_specifier(pbuffer, pformat);
            if(not pformat)
                return;

            if(not invoke_custom_format(pbuffer, pformat,
                    std::forward<T>(value)))
            {
                append_percent(pbuffer);
            }
            return formatter::format(pbuffer, pformat,
                    std::forward<Args>(args)...);
        }
    }

private:
    // TODO these should perhaps be non-inline
    static void append_percent(output_buffer* pbuffer)
    {
        auto p = pbuffer->reserve(1u);
        *p = '%';
        pbuffer->commit(1u);
    }
    static void specifier_error(output_buffer* pbuffer)
    {
        //static char const s[] = "<unrecognized format specifier> %";
        //auto p = pbuffer->reserve(sizeof(s)-1);
        //std::memcpy(p, s, sizeof(s)-1);
        //pbuffer->commit(sizeof(s)-1);
    }

    static char const* next_specifier(output_buffer* pbuffer,
            char const* pformat)
    {
        while(true) {
            char const* pspecifier = std::strchr(pformat, '%');
            if(pspecifier == nullptr) {
                format(pbuffer, pformat);
                return nullptr;
            }

            auto len = pspecifier - pformat;
            auto p = pbuffer->reserve(len);
            std::memcpy(p, pformat, len);
            pbuffer->commit(len);

            pformat = pspecifier + 1;

            if(*pformat != '%')
                return pformat;

            ++pformat;
            append_percent(pbuffer);
        }
    }
};

inline auto dlog::detail::get_input_buffer() -> input_buffer*
{
#ifdef USE_THREAD_LOCAL
    return &tls_input_buffer;
#else
    if(__builtin_expect(tls_pinput_buffer != nullptr, 1)) {
        return tls_pinput_buffer;
    } else {
        auto p = new input_buffer();
        tls_pinput_buffer = p;
        return p;
    }
#endif
}
