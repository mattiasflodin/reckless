#ifndef ASYNCLOG_DETAIL_INPUT_HPP
#define ASYNCLOG_DETAIL_INPUT_HPP

#include "asynclog/detail/spsc_event.hpp"

namespace asynclog {

class basic_log;

namespace detail {

class thread_input_buffer {
public:
    static thread_input_buffer2* create(std::size_t size)
    {
        std::size_t full_size = sizeof(thread_input_buffer2) + thread_input_buffer_size_ - sizeof(buffer_start_);
        char* buf = new char[full_size];
        try {
            return new (buf) thread_input_buffer(size);
        } catch(...) {
            delete [] buf;
            throw;
        }
    }

    static void destroy(thread_input_buffer2* p)
    {
        p->~thread_input_buffer2();
        delete [] static_cast<char*>(static_cast<void*>(p));
    }
    // returns pointer to allocated input frame, moves input_end() forward.
    char* allocate_input_frame(std::size_t size);
    // returns pointer to following input frame
    char* discard_input_frame(std::size_t size);
    char* wraparound();
    char* input_start() const
    {
        return pinput_start_.load(std::memory_order_relaxed);
    }
    char* input_end() const
    {
        return pinput_end_;
    }
    void signal_input_consumed();

private:
    thread_input_buffer(std::size_t size);
    ~thread_input_buffer();
    
    char* advance_frame_pointer(char* p, std::size_t distance);
    void wait_input_consumed();
    bool is_aligned(void* p)
    {
        return (reinterpret_cast<std::uintptr_t>(p) & alignof(frame_alignment_mask())) == 0;
    }
    bool is_aligned(std::size_t v)
    {
        return (v & frame_alignment_mask()) == 0;
    }

    std::uintptr_t frame_alignment_mask()
    {
        static_assert(is_power_of_two(alignof(formatter_dispatch_function_t)));
        return alignof(formatter_dispatch_function_t)-1;
    }

    basic_log* plog_;                 // Owner log instance
    spsc_event input_consumed_event_;
    std::size_t size_;                // number of chars in buffer

    char* const pbegin_;              // fixed value
    std::atomic<char*> pinput_start_; // moved forward by output thread, read by logger::write (to determine free space left)
    char* pinput_end_;                // moved forward by logger::write, never read by anyone else
    formatter_dispatch_function_t buffer_start_;
};

struct commit_extent {
    thread_input_buffer* pinput_buffer;
    char* pcommit_end;
};

}
}

#endif  // ASYNCLOG_DETAIL_INPUT_HPP
