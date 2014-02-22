#include <mutex>
#include <condition_variable>
#include <thread>
#include <unordered_set>

#include <unistd.h>

class log_buffer {
public:
    log_buffer(std::size_t size_);
    ~log_buffer();

    char* pbuffer_begin;
    char* pbuffer_end;
    char* pdata_begin;
    char* pflush_end;
    char* pdata_end;
};

namespace {
    thread_local log_buffer tls_log_buffer;

    // for do_write_background_thread
    std::thread g_write_thread;
    void writer();

    std::mutex g_pending_buffers_mutex;
    std::condition_variable g_pending_buffer_condition;
    std::unordered_set<log_buffer*> g_pending_buffers;
    std::deque<log_buffer*> g_pending_buffers_queue;
}

// Buffering and then flushing in a separate thread might seem like it just
// replicates what the OS file cache does. But a call to kernel mode for the
// write operation is expensive and, if the kernel is not preemptible, might
// cause undue congestion between threads. By comparison, a logging function
// that buffers in thread-local memory and merely takes a very short-lived lock
// (with no expectation of congestion due to how rarely the lock is held) will
// run extremely fast.
// 
// Actually this isn't exactly true, we still need to signal a condition
// variable (or event) when there is data available to write, and that requires
// a kernel operation. We'll simply have to measure to determine if there's
// any point in asynchronous writes.

void do_write_simple()
{
    log_buffer& lb = tls_log_buffer;
    write(g_fd, lb.pdata_begin, lb.pflush_end - lb.pdata_begin);
    lb.pdata_begin = lb.pflush_end;
    if(lb.pdata_begin == lb.pdata_end)
        lb.pdata_begin = lb.pdata_end = lb.pbuffer_begin;
};

void do_write_background_thread()
{
    tls_log_buffer.j
    std::unique_lock<std::mutex> hold(g_pending_buffers_mutex);
    bool inserted = get<1>(g_pending_buffers.insert(&tls_plog_buffer));
    if(inserted) {
        g_pending_buffers_queue.push_back(tls_plog_buffer);
        g_pending_buffer_condition.notify();
    }
}
void cleanup_writer_background_thread()
{
    {
        std::unique_lock<std::mutex> hold(g_pending_buffers_mutex);
        g_pending_buffers_queue.push_back(nullptr);
        g_pending_buffer_condition.notify();
    }
    g_write_thread.join();
}


log_buffer::log_buffer(std::size_t size_) :
    size(size),
    buffer(new char[size])
{
    next_position = buffer;
}

log_buffer::~log_buffer()
{
}

void dlog_init()
{
    g_write_thread = std::thread(&writer);
}

void dlog_cleanup()
{
    write_cleanup();
}


void dlog(char const* s)
{
    auto len = std::strlen(s);
    std::memcpy(tls_log_buffer.pdata_end, s, len);
    tls_plog_buffer.pdata_end += len;
}

void dlog_flush()
{
    tls_log_buffer.pflush_end = tls_log_buffer.pdata_end;
    do_write_simple();
}

namespace {
    log_buffer* get_pending_buffer()
    {
        std::unique_lock<std::mutex> hold(g_pending_buffers_mutex);
        while(g_pending_buffers.empty())
            g_pending_buffer_condition.wait();
        ppending_buffer = g_pending_buffers_queue.front();
        g_pending_buffers_queue.pop_front();
        if(ppending_buffer == nullptr)
            return nullptr;
        g_pending_buffers.erase(ppending_buffer);
        return ppending_buffer;
    }

    void writer()
    {
        while(true) {
            auto ppending_buffer = get_pending_buffer();
            if(not ppending_buffer)
                return;
            auto pbegin = ppending_buffer->pdata_begin;
            auto pend = ppending_buffer->pflush_end;
            write(g_fd, pbegin, pend - pbegin);
            ppending_buffer->pending_write_begin = pend;
        }
    }
}
