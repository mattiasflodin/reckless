#ifndef ASYNCLOG_DETAIL_INPUT_HPP
#define ASYNCLOG_DETAIL_INPUT_HPP

#include "asynclog/detail/spsc_event.hpp"

namespace asynclog {

class basic_log;

namespace detail {

class thread_input_buffer {
public:
    thread_input_buffer(std::size_t size, std::size_t frame_alignment);
    ~thread_input_buffer();
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

private:
    static char* allocate_buffer(std::size_t size, std::size_t alignment);
    char* advance_frame_pointer(char* p, std::size_t distance);
    void wait_input_consumed();
    void signal_input_consumed();
    bool is_aligned(void* p)
    {
        return (reinterpret_cast<std::uintptr_t>(p) & frame_alignment_mask_) == 0;
    }
    bool is_aligned(std::size_t v)
    {
        return (v & frame_alignment_mask_) == 0;
    }

    basic_log* plog_;                 // Owner log instance
    spsc_event input_consumed_event_;
    std::size_t size_;                // number of chars in buffer
    std::size_t frame_alignment_mask_;     // alignment mask for input frames

    char* const pbegin_;              // fixed value
    std::atomic<char*> pinput_start_; // moved forward by output thread, read by logger::write (to determine free space left)
    char* pinput_end_;                // moved forward by logger::write, never read by anyone else
};

struct commit_extent {
    thread_input_buffer* pinput_buffer;
    char* pcommit_end;
};

}
}

#endif  // ASYNCLOG_DETAIL_INPUT_HPP
