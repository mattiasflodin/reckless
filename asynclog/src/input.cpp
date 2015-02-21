#include "asynclog.hpp"
#include "asynclog/detail/utility.hpp"

asynclog::detail::thread_input_buffer::thread_input_buffer(std::size_t size, std::size_t frame_alignment) :
    size_(size),
    frame_alignment_mask_(frame_alignment-1),
    pbegin_(allocate_buffer(size, frame_alignment)),
    pinput_start_(pbegin_),
    pinput_end_(pbegin_)
{
    assert(detail::is_power_of_two(frame_alignment));
}

asynclog::detail::thread_input_buffer::~thread_input_buffer()
{
    // FIXME I don't think we are properly waiting until the output
    // worker has cleared this from touched_input_buffers. Which means
    // it can call signal_input_consumed on a dangling pointer. We need
    // to either get rid of touched_input_buffer or make sure we wait
    // for the right thing here.
 
    // Wait for the output thread to consume all the contents of the buffer
    // before release it.
    // Both write() and wait_input_consumed should create full memory barriers,
    // so no need for strict memory ordering in this load.
    while(pinput_start_.load(std::memory_order_relaxed) != pinput_end_)
        wait_input_consumed();

    free(pbegin_);
}

char* asynclog::detail::thread_input_buffer::discard_input_frame(std::size_t size)
{
    auto mask = frame_alignment_mask_;
    size = (size + mask) & ~mask;
    // We can use relaxed memory ordering everywhere here because there is
    // nothing being written of interest that the pointer update makes visible;
    // all it does is *discard* data, not provide any new data (besides,
    // signaling the event is likely to create a full memory barrier anyway).
    auto p = pinput_start_.load(std::memory_order_relaxed);
    p = advance_frame_pointer(p, size);
    pinput_start_.store(p, std::memory_order_relaxed);
    return p;
}

char* asynclog::detail::thread_input_buffer::wraparound()
{
#ifndef NDEBUG
    auto p = pinput_start_.load(std::memory_order_relaxed);
    auto marker = *reinterpret_cast<formatter_dispatch_function_t**>(p);
    assert(WRAPAROUND_MARKER == marker);
#endif
    pinput_start_.store(pbegin_, std::memory_order_relaxed);
    return pbegin_;
}

// Helper for allocating aligned ring buffer in ctor.
char* asynclog::detail::thread_input_buffer::allocate_buffer(std::size_t size, std::size_t alignment)
{
    void* pbuffer = nullptr;
    // TODO proper error here. bad_alloc?
    if(0 != posix_memalign(&pbuffer, alignment, size))
        throw std::runtime_error("cannot allocate input frame");
    return static_cast<char*>(pbuffer);
}

// Moves an input-buffer pointer forward by the given distance while
// maintaining the invariant that:
//
// * p is aligned by FRAME_ALIGNMENT
// * p never points at the end of the buffer; it always wraps around to the
//   beginning of the circular buffer.
//
// The distance must never be so great that the pointer moves *past* the end of
// the buffer. To do so would be an error in our context, since no input frame
// is allowed to be discontinuous.
char* asynclog::detail::thread_input_buffer::advance_frame_pointer(char* p, std::size_t distance)
{
    assert(is_aligned(p));
    assert(is_aligned(distance));
    p += distance;
    assert(size_ >= static_cast<std::size_t>(p - pbegin_));
    auto remaining = size_ - (p - pbegin_);
    if(remaining == 0)
        p = pbegin_;
    return p;
}

// TODO inline this function? Maybe not considering that fixme.
void asynclog::detail::thread_input_buffer::wait_input_consumed()
{
    // FIXME we need to think about what to do here, should we signal
    // g_shared_input_queue_full_event to force the output thread to wake up?
    // We probably should, or we could sit here for a full second.
    input_consumed_event_.wait();
}

void asynclog::detail::thread_input_buffer::signal_input_consumed()
{
    input_consumed_event_.signal();
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
    auto mask = frame_alignment_mask_;
    size = (size + mask) & ~mask;

    // We can't write a frame that is larger than the entire capacity of the
    // input buffer. If you hit this assert then you either need to write a
    // smaller log entry, or you need to make the input buffer larger.
    assert(size < size_);
 
    while(true) {
        auto pinput_end = pinput_end_;
        // FIXME these asserts should / can be enabled again?
        assert(static_cast<std::size_t>(pinput_end - pbegin_) < size_);
        assert(is_aligned(pinput_end));

        // Even if we get an "old" value for pinput_start_ here, that's OK
        // because other threads will never cause the amount of available
        // buffer space to shrink. So either there is enough buffer space and
        // we're done, or there isn't and we'll wait for an input-consumption
        // event which creates a full memory barrier and hence gives us an
        // updated value for pinput_start_. So memory_order_relaxed should be
        // fine here.
        auto pinput_start = pinput_start_.load(std::memory_order_relaxed);
        std::ptrdiff_t free = pinput_start - pinput_end;
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
            if(likely(static_cast<std::ptrdiff_t>(size) < free)) {
                pinput_end_ = advance_frame_pointer(pinput_end, size);
                return pinput_end;
            } else {
                // Not enough room. Wait for the output thread to consume some
                // input.
                wait_input_consumed();
            }
        } else {
            // Free space is non-contiguous.
            // TODO should we use an end pointer instead of a size_?
            std::size_t free1 = size_ - (pinput_end - pbegin_);
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
                    // marker because frame alignment is at least the size of
                    // the marker.
                    *reinterpret_cast<formatter_dispatch_function_t**>(pinput_end_) =
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

