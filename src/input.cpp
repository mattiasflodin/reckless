#include "asynclog.hpp"

asynclog::detail::thread_input_buffer::thread_input_buffer() :
    pbegin_(allocate_buffer())
{
    pinput_start_ = pbegin_;
    pinput_end_ = pbegin_;
    pcommit_end_ = pbegin_;
}

asynclog::detail::thread_input_buffer::~thread_input_buffer()
{
    commit();
    // Both commit() and wait_input_consumed should create full memory barriers,
    // so no need for strict memory ordering in this load.
    while(pinput_start_.load(std::memory_order_relaxed) != pinput_end_)
        wait_input_consumed();

    free(pbegin_);
}

// Helper for allocating aligned ring buffer in ctor.
char* asynclog::detail::thread_input_buffer::allocate_buffer()
{
    void* pbuffer = nullptr;
    // TODO proper error here. bad_alloc?
    if(0 != posix_memalign(&pbuffer, FRAME_ALIGNMENT, TLS_INPUT_BUFFER_SIZE))
        throw std::runtime_error("cannot allocate input frame");
    *static_cast<char*>(pbuffer) = 'X';
    return static_cast<char*>(pbuffer);
}

char* asynclog::detail::thread_input_buffer::allocate_input_frame(std::size_t size)
{
    // Conceptually, we have the invariant that
    //   pinput_start_ <= pinput_end_,
    // and the memory area after pinput_end is free for us to use for
    // allocating a frame. However, the fact that it's a circular buffer means
    // that:
    // 
    // * The area after pinput_end is actually non-contiguous, wraps around
    //   at the end of the buffer and ends at pinput_start.
    //   
    // * Except, when pinput_end itself has fallen over the right edge and we
    //   have the case pinput_end <= pinput_start. Then the *used* memory is
    //   non-contiguous, and the free memory is contiguous (it still starts at
    //   pinput_end and ends at pinput_start modulo circular buffer size).
    //   
    // (This is easier to understand by drawing it on a paper than by reading
    // the comment text).
    while(true) {
        auto pinput_end = pinput_end_;
        assert(pinput_end - pbegin_ != TLS_INPUT_BUFFER_SIZE);
        assert(is_aligned(pinput_end, FRAME_ALIGNMENT));

        // Even if we get an "old" value for pinput_start_ here, that's OK
        // because other threads will never cause the amount of available
        // buffer space to shrink. So either there is enough buffer space and
        // we're done, or there isn't and we'll wait for an input-consumption
        // event which creates a full memory barrier and hence gives us an
        // updated value for pinput_start_. So memory_order_relaxed should be
        // fine here.
        auto pinput_start = pinput_start_.load(std::memory_order_relaxed);
        //std::cout << "W " <<  std::hex << (pinput_start - pbegin_) << " - " << std::hex << (pinput_end - pbegin_) << std::endl;
        auto free = pinput_start - pinput_end;
        if(free > 0) {
            // Free space is contiguous.
            // Technically, there is enough room if size == free. But the
            // problem with using the free space in this situation is that when
            // we increase pinput_end_ by size, we end up with pinput_start_ ==
            // pinput_end_. Now, given that state, how do we know if the buffer
            // is completely filled or empty? So, it's easier to just check for
            // size < free instead of size <= free, and pretend we're out
            // of space if size == free. Same situation applies in the else
            // clause below.
            if(likely(size < free)) {
                pinput_end_ = advance_frame_pointer(pinput_end, size);
                return pinput_end;
            } else {
                // Not enough room. Wait for the output thread to consume some
                // input.
                wait_input_consumed();
            }
        } else {
            // Free space is non-contiguous.
            std::size_t free1 = TLS_INPUT_BUFFER_SIZE - (pinput_end - pbegin_);
            if(likely(size < free1)) {
                // There's enough room in the first segment.
                pinput_end_ = advance_frame_pointer(pinput_end, size);
                return pinput_end;
            } else {
                std::size_t free2 = pinput_start - pbegin_;
                if(likely(size < free2)) {
                    // We don't have enough room for a continuous input frame
                    // in the first segment (at the end of the circular
                    // buffer), but there is enough room in the second segment
                    // (at the beginning of the buffer). To instruct the output
                    // thread to skip ahead to the second segment, we need to
                    // put a marker value at the current position. We're
                    // supposed to be guaranteed enough room for the wraparound
                    // marker because FRAME_ALIGNMENT is at least the size of
                    // the marker.
                    *reinterpret_cast<dispatch_function_t**>(pinput_end_) =
                        WRAPAROUND_MARKER;
                    pinput_end_ = advance_frame_pointer(pbegin_, size);
                    return pbegin_;
                } else {
                    // Not enough room. Wait for the output thread to consume
                    // some input.
                    wait_input_consumed();
                }
            }
        }
    }
}

