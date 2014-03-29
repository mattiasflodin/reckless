#ifndef ASYNCLOG_DETAIL_INPUT_HPP
#define ASYNCLOG_DETAIL_INPUT_HPP

namespace asynclog {
namespace detail {

struct commit_extent {
    thread_input_buffer* pinput_buffer;
    char* pcommit_end;
};

typedef boost::lockfree::queue<commit_extent, boost::lockfree::capacity<INPUT_QUEUE_SIZE>> shared_input_queue_t;
extern shared_input_queue_t g_shared_input_queue;

inline void queue_commit_extent(commit_extent const& ce)
{
    extern void queue_commit_extent_slow_path(commit_extent const& ce);
    if(unlikely(not g_shared_input_queue.push(ce)))
        queue_commit_extent_slow_path(ce);
}

class thread_input_buffer {
public:
    thread_input_buffer();
    ~thread_input_buffer();
    char* allocate_input_frame(std::size_t size);
    // returns pointer to following input frame
    char* discard_input_frame(std::size_t size);
    char* wraparound();
    char* input_start() const;
    void commit()
    {
        auto pcommit_end = pinput_end_;
        queue_commit_extent({this, pcommit_end});
        pcommit_end_ = pcommit_end;
    }

private:
    static char* allocate_buffer();
    char* advance_frame_pointer(char* p, std::size_t distance);
    void wait_input_consumed();
    void signal_input_consumed();

    spsc_event input_consumed_event_;

    char* const pbegin_;              // fixed value
    std::atomic<char*> pinput_start_; // moved forward by output thread, read by logger::write (to determine free space left)
    char* pinput_end_;                // moved forward by logger::write, never read by anyone else
    char* pcommit_end_;               // moved forward by commit(), read by wait_input_consumed (same thread so no race)
};

#ifdef ASYNCLOG_USE_CXX11_THREAD_LOCAL
extern thread_local thread_input_buffer tls_input_buffer; 
extern thread_local thread_input_buffer* tls_pinput_buffer; 
#else
extern thread_input_buffer* tls_pinput_buffer; 
#endif
inline thread_input_buffer* dlog::detail::get_input_buffer()
{
#ifdef ASYNCLOG_USE_CXX11_THREAD_LOCAL
    if(likely(tls_pinput_buffer != nullptr))
        return tls_pinput_buffer;
    else
        return tls_pinput_buffer = &tls_input_buffer;
#else
    if(likely(tls_pinput_buffer != nullptr)) {
        return tls_pinput_buffer;
    } else {
        auto p = new thread_input_buffer();
        tls_pinput_buffer = p;
        return p;
    }
#endif
}


}
}

#endif  // ASYNCLOG_DETAIL_INPUT_HPP
