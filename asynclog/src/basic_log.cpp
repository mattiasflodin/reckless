#include "asynclog/detail/basic_log.hpp"

#include <unordered_set>

asynclog::basic_log::basic_log(writer* pwriter, 
        std::size_t output_buffer_max_capacity,
        std::size_t shared_input_queue_size,
        std::size_t thread_input_buffer_size,
        std::size_t input_frame_alignment) :
    shared_input_queue_(0)
{
    open(pwriter, output_buffer_max_capacity, shared_input_queue_size, thread_input_buffer_size, input_frame_alignment);
}

asynclog::basic_log::~basic_log()
{
    if(is_open())
        close();
}

void asynclog::basic_log::open(writer* pwriter, 
        std::size_t input_frame_alignment,
        std::size_t output_buffer_max_capacity,
        std::size_t shared_input_queue_size,
        std::size_t thread_input_buffer_size)
{
    if(output_buffer_max_capacity == 0 or shared_input_queue_size == 0
            or thread_input_buffer_size == 0)
    {
        std::size_t page_size = detail::get_page_size();
        if(output_buffer_max_capacity == 0)
            output_buffer_max_capacity = page_size;
        // TODO is it right to just do g_page_size/sizeof(commit_extent) if we want
        // the buffer to use up one page? There's likely more overhead in the
        // buffer.
        if(shared_input_queue_size == 0)
            shared_input_queue_size = page_size / sizeof(detail::commit_extent);
        if(thread_input_buffer_size == 0)
            thread_input_buffer_size = page_size;
    }

    if(input_frame_alignment == 0)
        input_frame_alignment = detail::get_cache_line_size();

    assert(detail::is_power_of_two(input_frame_alignment));
    // input_frame_alignment must at least match that of a function pointer
    assert(input_frame_alignment >= sizeof(detail::formatter_dispatch_function_t*));
    // We need the requirement below to ensure that, after alignment, there
    // will either be 0 free bytes available in the input buffer, or
    // enough to fit a function pointer. This simplifies the code.
    assert(input_frame_alignment >= alignof(detail::formatter_dispatch_function_t*));

    pthread_input_buffer_ = thread_input_buffer_t(thread_input_buffer_size, input_frame_alignment);
    reset_shared_input_queue(shared_input_queue_size);
    output_buffer_ = output_buffer(pwriter, output_buffer_max_capacity);
    output_thread_ = std::thread(std::mem_fn(&basic_log::output_worker), this);
}

void asynclog::basic_log::close()
{
    using namespace detail;
    assert(is_open());
    // FIXME always signal a buffer full event, so we don't have to wait 1
    // second before the thread exits.
    queue_commit_extent({nullptr, 0});
    output_thread_.join();
    assert(shared_input_queue_.empty());
}

void asynclog::basic_log::output_worker()
{
    // TODO if possible we should call signal_input_consumed() whenever the
    // output buffer is flushed, so threads aren't kept waiting indefinitely if
    // the queue never clears up.
    using namespace detail;
    std::unordered_set<thread_input_buffer*> touched_input_buffers;
    while(true) {
        commit_extent ce;
        unsigned wait_time_ms = 0;
        if(not shared_input_queue_.pop(ce)) {
            shared_input_consumed_event_.signal();
            for(thread_input_buffer* pinput_buffer : touched_input_buffers)
                pinput_buffer->signal_input_consumed();
            touched_input_buffers.clear();
            if(!output_buffer_.empty())
                output_buffer_.flush();
            while(not shared_input_queue_.pop(ce)) {
                shared_input_queue_full_event_.wait(wait_time_ms);
                wait_time_ms += std::max(1u, wait_time_ms/4);
                wait_time_ms = std::min(wait_time_ms, 1000u);
            }
        }
            
        if(not ce.pinput_buffer) {
            output_buffer_.flush();
            return;
        }

        char* pinput_start = ce.pinput_buffer->input_start();
        while(pinput_start != ce.pcommit_end) {
            auto pdispatch = *reinterpret_cast<formatter_dispatch_function_t**>(pinput_start);
            if(WRAPAROUND_MARKER == pdispatch) {
                pinput_start = ce.pinput_buffer->wraparound();
                pdispatch = *reinterpret_cast<formatter_dispatch_function_t**>(pinput_start);
            }
            auto frame_size = (*pdispatch)(&output_buffer_, pinput_start);
            pinput_start = ce.pinput_buffer->discard_input_frame(frame_size);
            touched_input_buffers.insert(ce.pinput_buffer);
        }
    }
}

void asynclog::basic_log::queue_commit_extent(detail::commit_extent const& ce)
{
    using namespace detail;
    if(unlikely(not shared_input_queue_.push(ce))) {
        do {
            shared_input_queue_full_event_.signal();
            shared_input_consumed_event_.wait();
        } while(not shared_input_queue_.push(ce));
    }
}

// TODO this should probably be removed.
char* asynclog::basic_log::allocate_input_frame(std::size_t frame_size)
{
    auto pib = pthread_input_buffer_.get();
    return pib->allocate_input_frame(frame_size);
}

void asynclog::basic_log::reset_shared_input_queue(std::size_t node_count)
{
    // boost's lockfree queue has no move constructor and provides no reserve()
    // function when you use fixed_sized policy. So we'll just explicitly
    // destroy the current queue and create a new one with the desired size. We
    // are guaranteed that the queue is empty since close() clears it.
    shared_input_queue_.~shared_input_queue_t();
    // TODO how do we handle an exception here?
    new (&shared_input_queue_) shared_input_queue_t(node_count);
}
