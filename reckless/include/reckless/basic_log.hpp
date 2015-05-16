#ifndef RECKLESS_BASIC_LOG_HPP
#define RECKLESS_BASIC_LOG_HPP

#include <reckless/detail/thread_input_buffer.hpp>
#include <reckless/detail/spsc_event.hpp>
#include <reckless/detail/branch_hints.hpp> // likely
#include <reckless/detail/optional.hpp>
#include <reckless/output_buffer.hpp>

#include <boost_1_56_0/lockfree/queue.hpp>

#include <thread>
#include <functional>
#include <tuple>

#include <pthread.h>    // pthread_key_t

namespace reckless {
namespace detail {
    template <class Formatter, typename... Args>
    std::size_t formatter_dispatch(output_buffer* poutput, char* pinput);
}

// TODO generic_log better name?
class basic_log {
public:
    basic_log();
    // FIXME shared_input_queue_size seems like the least interesting of these
    // and should be moved to the end.
    basic_log(writer* pwriter, 
            std::size_t output_buffer_max_capacity = 0,
            std::size_t shared_input_queue_size = 0,
            std::size_t thread_input_buffer_size = 0);
    virtual ~basic_log();
    
    basic_log(basic_log const&) = delete;
    basic_log& operator=(basic_log const&) = delete;

    virtual void open(writer* pwriter, 
            std::size_t output_buffer_max_capacity = 0,
            std::size_t shared_input_queue_size = 0,
            std::size_t thread_input_buffer_size = 0);
    virtual void close();

    void panic_flush();

protected:
    template <class Formatter, typename... Args>
    void write(Args&&... args)
    {
        using namespace detail;
        typedef std::tuple<typename std::decay<Args>::type...> args_t;
        std::size_t const args_align = alignof(args_t);
        std::size_t const args_offset = (sizeof(formatter_dispatch_function_t*) + args_align-1)/args_align*args_align;
        std::size_t const frame_size = args_offset + sizeof(args_t);

        auto pbuffer = get_input_buffer();
        char* pframe = pbuffer->allocate_input_frame(frame_size);
        *reinterpret_cast<formatter_dispatch_function_t**>(pframe) =
            &detail::formatter_dispatch<Formatter, typename std::decay<Args>::type...>;

        // FIXME exception safety when copy constructing arguments, both here
        // and in the output thread.
        new (pframe + args_offset) args_t(std::forward<Args>(args)...);

        // TODO ideally queue_commit_extent would be called in a separate
        // commit() or flush() function, but then we have to call
        // get_input_buffer() twice which bloats the code at the call site. But
        // if we make get_input_buffer() protected (i.e. move
        // thread_input_buffer from the detail namespace) then we can delegate
        // the call to get_input_buffer to the derived class, which could then
        // call write multiple times followed by commit() if it wants to
        // without having to fetch the TLS variable every time.
        queue_commit_extent({pbuffer, pbuffer->input_end()});
    }

private:
    void output_worker();
    void queue_commit_extent(detail::commit_extent const& ce);
    void reset_shared_input_queue(std::size_t node_count);
    detail::thread_input_buffer* get_input_buffer()
    {
        detail::thread_input_buffer* p = static_cast<detail::thread_input_buffer*>(pthread_getspecific(thread_input_buffer_key_));
        if(detail::likely(p != nullptr)) {
            return p;
        } else {
            return init_input_buffer();
        }
    }
    detail::thread_input_buffer* init_input_buffer();
    void on_panic_flush_done();
    bool is_open()
    {
        return output_thread_.joinable();
    }

    typedef boost_1_56_0::lockfree::queue<detail::commit_extent, boost_1_56_0::lockfree::fixed_sized<true>> shared_input_queue_t;

    //typedef detail::thread_object<detail::thread_input_buffer, std::size_t, std::size_t> thread_input_buffer_t;
    //thread_input_buffer_t pthread_input_buffer_;
    
    std::experimental::optional<shared_input_queue_t> shared_input_queue_;
    spsc_event shared_input_queue_full_event_;
    spsc_event shared_input_consumed_event_;
    pthread_key_t thread_input_buffer_key_;
    std::size_t thread_input_buffer_size_;
    output_buffer output_buffer_;
    std::thread output_thread_;
    spsc_event panic_flush_done_event_;
    bool panic_flush_;
};

namespace detail {
template <class Formatter, typename... Args, std::size_t... Indexes>
void call_formatter(output_buffer* poutput, std::tuple<Args...>& args, index_sequence<Indexes...>)
{
    Formatter::format(poutput, std::move(std::get<Indexes>(args))...);
}

template <class Formatter, typename... Args>
std::size_t formatter_dispatch(output_buffer* poutput, char* pinput)
{
    using namespace detail;
    typedef std::tuple<Args...> args_t;
    std::size_t const args_align = alignof(args_t);
    std::size_t const args_offset = (sizeof(formatter_dispatch_function_t*) + args_align-1)/args_align*args_align;
    std::size_t const frame_size = args_offset + sizeof(args_t);
    args_t& args = *reinterpret_cast<args_t*>(pinput + args_offset);

    typename make_index_sequence<sizeof...(Args)>::type indexes;
    call_formatter<Formatter>(poutput, args, indexes);

    args.~args_t();
    return frame_size;
}

}   // namespace detail
}   // namespace reckless

#endif  // RECKLESS_BASIC_LOG_HPP
