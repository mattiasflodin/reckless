#include "asynclog/detail/thread_object.hpp"

namespace asynclog {
namespace detail {

class thread_input_buffer;

class log_base {
public:
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

    detail::thread_object<detail::thread_input_buffer> pthread_input_buffer_;
};

}
}

