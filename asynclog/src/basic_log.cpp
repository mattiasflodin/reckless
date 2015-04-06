#include "asynclog/detail/basic_log.hpp"

#include <thread>       // this_thread::yield
#include <vector>
#include <ciso646>

#include <unistd.h>     // sleep

namespace {
void destroy_thread_input_buffer(void* p)
{
    using asynclog::detail::thread_input_buffer;
    thread_input_buffer* pbuffer = static_cast<thread_input_buffer*>(p);
    thread_input_buffer::destroy(pbuffer);
}
}

// FIXME we need to destroy the pthreads key in dtor

asynclog::basic_log::basic_log() :
    shared_input_queue_(0),
    thread_input_buffer_size_(0),
    panic_flush_(false)
{
    if(0 != pthread_key_create(&thread_input_buffer_key_, &destroy_thread_input_buffer))
        throw std::bad_alloc();
}

asynclog::basic_log::basic_log(writer* pwriter, 
        std::size_t output_buffer_max_capacity,
        std::size_t shared_input_queue_size,
        std::size_t thread_input_buffer_size) :
    shared_input_queue_(0),
    thread_input_buffer_size_(0),
    panic_flush_(false)
{
    if(0 != pthread_key_create(&thread_input_buffer_key_, &destroy_thread_input_buffer))
        throw std::bad_alloc();
    open(pwriter, output_buffer_max_capacity, shared_input_queue_size, thread_input_buffer_size);
}

asynclog::basic_log::~basic_log()
{
    if(panic_flush_)
        return;
    if(is_open())
        close();
}

void asynclog::basic_log::open(writer* pwriter, 
        std::size_t output_buffer_max_capacity,
        std::size_t shared_input_queue_size,
        std::size_t thread_input_buffer_size)
{
    // FIXME we should base defaults on the common disk block size instead of
    // page sizes (8KiB).
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
    reset_shared_input_queue(shared_input_queue_size);
    thread_input_buffer_size_ = thread_input_buffer_size;
    output_buffer_ = output_buffer(pwriter, output_buffer_max_capacity);
    output_thread_ = std::thread(std::mem_fn(&basic_log::output_worker), this);
}

void asynclog::basic_log::close()
{
    using namespace detail;
    assert(is_open());
    // FIXME always signal a buffer full event, so we don't have to wait 1
    // second before the thread exits.
    queue_commit_extent({nullptr, nullptr});
    output_thread_.join();
    assert(shared_input_queue_.empty());
    // FIXME reverse everything that open() does, including getting rid of the
    // buffers etc.
}

void asynclog::basic_log::panic_flush()
{
    panic_flush_ = true;
    shared_input_queue_full_event_.signal();
    panic_flush_done_event_.wait();
}

void asynclog::basic_log::output_worker()
{
    // TODO if possible we should call signal_input_consumed() whenever the
    // output buffer is flushed, so threads aren't kept waiting indefinitely if
    // the queue never clears up.
    using namespace detail;
    std::vector<thread_input_buffer*> touched_input_buffers;
    touched_input_buffers.reserve(std::max(8u, 2*std::thread::hardware_concurrency()));
    while(true) {
        commit_extent ce;
        unsigned wait_time_ms = 0;
        if(not shared_input_queue_.pop(ce)) {
            if(unlikely(panic_flush_)) {
                on_panic_flush_done();
            } else {
                shared_input_consumed_event_.signal();
                for(thread_input_buffer* pinput_buffer : touched_input_buffers)
                    pinput_buffer->signal_input_consumed();
                for(thread_input_buffer* pbuffer : touched_input_buffers)
                    pbuffer->input_consumed_flag = false;
                touched_input_buffers.clear();
                if(not output_buffer_.empty())
                    output_buffer_.flush();
                while(not shared_input_queue_.pop(ce)) {
                    shared_input_queue_full_event_.wait(wait_time_ms);
                    wait_time_ms += std::max(1u, wait_time_ms/4);
                    wait_time_ms = std::min(wait_time_ms, 1000u);
                }
            }
        }
            
        if(not ce.pinput_buffer) {
            if(unlikely(panic_flush_))
                on_panic_flush_done();
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
            if(likely(!panic_flush_)) {
                // If we're in panic-flush mode then we don't try to touch the
                // heap-allocated vector.
                if(not ce.pinput_buffer->input_consumed_flag) {
                    touched_input_buffers.push_back(ce.pinput_buffer);
                    ce.pinput_buffer->input_consumed_flag = true;
                }
            }
        }
    }
}

void asynclog::basic_log::queue_commit_extent(detail::commit_extent const& ce)
{
    using namespace detail;
    if(unlikely(panic_flush_))
    {
        // Another visitor! Stay a while; stay forever!
        // When we are in panic mode because of a crash, we want to flush all
        // the log entries that were produced before the crash, but no more
        // than that. We could put a watermark in the queue and whatnot, but
        // really, when there's a crash in progress we will just confound the
        // problem if we keep trying to push stuff on the queue and muck about
        // with the heap.  Better to just suspend anything that tries, and wait
        // for the kill.
        while(true)
            sleep(3600);
    }
    if(unlikely(not shared_input_queue_.push(ce))) {
        do {
            shared_input_queue_full_event_.signal();
            shared_input_consumed_event_.wait();
        } while(not shared_input_queue_.push(ce));
    }
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

asynclog::detail::thread_input_buffer* asynclog::basic_log::init_input_buffer()
{
    auto p = detail::thread_input_buffer::create(thread_input_buffer_size_);
    try {
        int result = pthread_setspecific(thread_input_buffer_key_, p);
        if(detail::likely(result == 0))
            return p;
        else if(result == ENOMEM)
            throw std::bad_alloc();
        else
            throw std::system_error(result, std::system_category());
    } catch(...) {
        detail::thread_input_buffer::destroy(p);
        throw;
    }
}

void asynclog::basic_log::on_panic_flush_done()
{
    output_buffer_.flush();
    panic_flush_done_event_.signal();
    // Sleep and wait for death.
    while(true)
    {
        sleep(3600);
    }
}
