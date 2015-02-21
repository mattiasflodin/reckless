#ifndef ASYNCLOG_DETAIL_BASIC_LOG_HPP
#define ASYNCLOG_DETAIL_BASIC_LOG_HPP

#include "asynclog/detail/thread_object.hpp"
#include "asynclog/detail/input.hpp"
#include "asynclog/detail/formatter.hpp"
#include "asynclog/output_buffer.hpp"

#include <boost_1_56_0/lockfree/queue.hpp>

#include <thread>
#include <functional>

namespace asynclog {

// TODO generic_log better name?
class basic_log {
public:
    basic_log() :
        shared_input_queue_(0)
    {
    }
    basic_log(writer* pwriter, 
            std::size_t output_buffer_max_capacity = 0,
            std::size_t shared_input_queue_size = 0,
            std::size_t thread_input_buffer_size = 0,
            std::size_t input_frame_alignment = 0);
    virtual ~basic_log();
    
    basic_log(basic_log const&) = delete;
    basic_log& operator=(basic_log const&) = delete;

    virtual void open(writer* pwriter, 
            std::size_t output_buffer_max_capacity = 0,
            std::size_t shared_input_queue_size = 0,
            std::size_t thread_input_buffer_size = 0,
            std::size_t input_frame_alignment = 0);
    virtual void close();

    bool is_open()
    {
        return output_thread_.joinable();
    }

protected:
    template <class Formatter, typename... Args>
    void write(Args&&... args)
    {
        using namespace detail;
        assert(is_open());
        typedef std::tuple<typename std::decay<Args>::type...> args_t;
        std::size_t const args_align = alignof(args_t);
        std::size_t const args_offset = (sizeof(formatter_dispatch_function_t*) + args_align-1)/args_align*args_align;
        std::size_t const frame_size = args_offset + sizeof(args_t);

        auto pbuffer = pthread_input_buffer_.get();
        char* pframe = pbuffer->allocate_input_frame(frame_size);
        *reinterpret_cast<formatter_dispatch_function_t**>(pframe) =
            &formatter_dispatch<Formatter, typename std::decay<Args>::type...>;

        // FIXME exception safety when copy constructing arguments, both here
        // and in the output thread.
        new (pframe + args_offset) args_t(std::forward<Args>(args)...);

        queue_commit_extent({pbuffer, pbuffer->input_end()});
    }

private:
    void output_worker();
    void queue_commit_extent(detail::commit_extent const& ce);
    char* allocate_input_frame(std::size_t frame_size);
    void reset_shared_input_queue(std::size_t node_count);
    thread_input_buffer2* get_input_buffer()
    {
        thread_input_buffer2* p = static_cast<thread_input_buffer2*>(pthread_getspecific(key_));
        if(likely(p != nullptr)) {
            return p;
        } else {
            return create_input_buffer();
        }
    }
    thread_input_buffer2* create_input_buffer();

    typedef boost_1_56_0::lockfree::queue<detail::commit_extent, boost_1_56_0::lockfree::fixed_sized<true>> shared_input_queue_t;

    //typedef detail::thread_object<detail::thread_input_buffer, std::size_t, std::size_t> thread_input_buffer_t;
    //thread_input_buffer_t pthread_input_buffer_;
    
    shared_input_queue_t shared_input_queue_;
    spsc_event shared_input_queue_full_event_;
    spsc_event shared_input_consumed_event_;
    output_buffer output_buffer_;
    std::thread output_thread_;
};

}

#endif  // ASYNCLOG_DETAIL_BASIC_LOG_HPP
