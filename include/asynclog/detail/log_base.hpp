#ifndef ASYNCLOG_LOG_BASE_HPP
#define ASYNCLOG_LOG_BASE_HPP

#include "asynclog/detail/thread_object.hpp"
#include "asynclog/detail/input.hpp"
#include "asynclog/output_buffer.hpp"

#include <boost/lockfree/queue.hpp>

#include <thread>
#include <functional>

namespace asynclog {
namespace detail {

class log_base {
public:
    log_base(writer* pwriter, 
            std::size_t output_buffer_max_capacity,
            std::size_t shared_input_queue_size) :
        shared_input_queue_(shared_input_queue_size),
        output_buffer_(pwriter, output_buffer_max_capacity),
        output_thread_(std::mem_fn(&log_base::output_worker), this)
    {
    }

    ~log_base()
    {
        using namespace detail;
        commit();
        queue_commit_extent({nullptr, 0});
        output_thread_.join();
        assert(shared_input_queue_.empty());
    }

    void commit()
    {
        using namespace detail;
        thread_input_buffer* pib = pthread_input_buffer_.get();
        auto pcommit_end = pib->input_end();
        queue_commit_extent({pib, pcommit_end});
    }

protected:
    void queue_commit_extent(detail::commit_extent const& ce)
    {
        using namespace detail;
        if(unlikely(not shared_input_queue_.push(ce)))
            queue_commit_extent_slow_path(ce);
    }

    void queue_commit_extent_slow_path(detail::commit_extent const& ce)
    {
        do {
            shared_input_queue_full_event_.signal();
            shared_input_consumed_event_.wait();
        } while(not shared_input_queue_.push(ce));
    }

    void output_worker()
    {
        using namespace detail;
        while(true) {
            commit_extent ce;
            unsigned wait_time_ms = 0;
            while(not shared_input_queue_.pop(ce)) {
                shared_input_queue_full_event_.wait(wait_time_ms);
                wait_time_ms = (wait_time_ms == 0)? 1 : 2*wait_time_ms;
                wait_time_ms = std::min(wait_time_ms, 1000u);
            }
            shared_input_consumed_event_.signal();
                
            if(not ce.pinput_buffer)
                // Request to shut down thread.
                return;

            char* pinput_start = ce.pinput_buffer->input_start();
            while(pinput_start != ce.pcommit_end) {
                auto pdispatch = *reinterpret_cast<dispatch_function_t**>(pinput_start);
                if(WRAPAROUND_MARKER == pdispatch) {
                    pinput_start = ce.pinput_buffer->wraparound();
                    pdispatch = *reinterpret_cast<dispatch_function_t**>(pinput_start);
                }
                auto frame_size = (*pdispatch)(&output_buffer_, pinput_start);
                auto mask = input_frame_alignment_mask_;
                auto frame_size_aligned = (frame_size + mask) & ~mask;
                pinput_start = ce.pinput_buffer->discard_input_frame(
                        frame_size_aligned);
            }
            // TODO we *could* do something like flush on a timer instead when we're getting a lot of writes / sec.
            // OR, we should at least keep on dumping data without flush as long as the input queue has data to give us.
            output_buffer_.flush();
        }
    }

    typedef boost::lockfree::queue<detail::commit_extent, boost::lockfree::fixed_sized<true>> shared_input_queue_t;

    detail::thread_object<detail::thread_input_buffer> pthread_input_buffer_;
    shared_input_queue_t shared_input_queue_;
    spsc_event shared_input_queue_full_event_;
    spsc_event shared_input_consumed_event_;
    std::size_t const input_frame_alignment_mask_;
    output_buffer output_buffer_;
    std::thread output_thread_;
};

}
}

#endif  // ASYNCLOG_LOG_BASE_HPP
