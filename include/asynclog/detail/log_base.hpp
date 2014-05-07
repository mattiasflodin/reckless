#ifndef ASYNCLOG_DETAIL_LOG_BASE_HPP
#define ASYNCLOG_DETAIL_LOG_BASE_HPP

#include "asynclog/detail/thread_object.hpp"
#include "asynclog/detail/input.hpp"
#include "asynclog/output_buffer.hpp"

#include <boost/lockfree/queue.hpp>

#include <thread>
#include <functional>

namespace asynclog {
namespace detail {

typedef std::size_t formatter_dispatch_function_t(output_buffer*, char*);
// TODO these checks need to be done at runtime now
//static_assert(alignof(dispatch_function_t*) <= ASYNCLOG_FRAME_ALIGNMENT,
//        "ASYNCLOG_FRAME_ALIGNMENT must at least match that of a function pointer");
//// We need the requirement below to ensure that, after alignment, there
//// will either be 0 free bytes available in the circular buffer, or
//// enough to fit a dispatch pointer. This simplifies the code a bit.
//static_assert(sizeof(dispatch_function_t*) <= FRAME_ALIGNMENT,
//        "ASYNCLOG_FRAME_ALIGNMENT must at least match the size of a function pointer");
formatter_dispatch_function_t* const WRAPAROUND_MARKER = reinterpret_cast<
    formatter_dispatch_function_t*>(0);

class log_base {
public:
    log_base() :
        shared_input_queue_(0)
    {
    }
    log_base(writer* pwriter, 
            std::size_t input_frame_alignment,
            std::size_t output_buffer_max_capacity,
            std::size_t shared_input_queue_size,
            std::size_t thread_input_buffer_size);
    ~log_base();

    void open(writer* pwriter, 
            std::size_t input_frame_alignment,
            std::size_t output_buffer_max_capacity,
            std::size_t shared_input_queue_size,
            std::size_t thread_input_buffer_size);
    void close();

    void commit();

protected:
    bool is_open()
    {
        return output_thread_.joinable();
    }
    void output_worker();
    void queue_commit_extent(detail::commit_extent const& ce);
    char* allocate_input_frame(std::size_t frame_size);
    void reset_shared_input_queue(std::size_t node_count);

    typedef boost::lockfree::queue<detail::commit_extent, boost::lockfree::fixed_sized<true>> shared_input_queue_t;

    typedef detail::thread_object<detail::thread_input_buffer, log_base*, std::size_t, std::size_t> thread_input_buffer_t;
    thread_input_buffer_t pthread_input_buffer_;
    shared_input_queue_t shared_input_queue_;
    spsc_event shared_input_queue_full_event_;
    spsc_event shared_input_consumed_event_;
    output_buffer output_buffer_;
    std::thread output_thread_;

private:
    log_base(log_base const&);  // not defined
    log_base& operator=(log_base const&); // not defined
};

}
}

#endif  // ASYNCLOG_DETAIL_LOG_BASE_HPP
