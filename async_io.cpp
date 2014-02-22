#include <thread>
#include <cstddef>
#include <condition_variable>
#include <mutex>
#include <deque>
#include <vector>

namespace async_io {
    void initialize();
    void cleanup();
    void pwrite(int fd, void const* buf, std::size_t count,
            std::off_t offset, std::uintptr_t key);
    struct event
    {
        std::uintptr_t key;
        int result;
    };
    bool read_event(event* pevent);

    namespace detail {
        struct io_request
        {
            int fildes;
            void const* buf;
            std::size_t nbyte;
            std::uintptr_t key;
        };

        std::condition_variable g_io_request_available_cv;
        std::mutex g_io_request_queue_mutex;
        std::deque<io_request> g_io_request_queue;
        
        std::deque<io_result> g_io_result_queue;
    }
}

inline void async_io::write(int fildes, void const* buf, std::size_t nbyte,
        std::uintptr_t key)
{
    std::unique_lock<std::mutex> hold(detail::g_io_request_queue_mutex);
    detail::g_io_request_queue.emplace_back(detail::io_request{fildes, buf, nbyte, key});
}

namespace {
    void io_worker_thread();

    std::vector<std::thread> g_thread_pool;
}

void async_io::initialize()
{
    g_thread_pool.emplace_back(&io_worker_thread);
}

namespace {
    using namespace async_io;
    void io_worker_thread()
    {
        detail::io_request r;
        {
            std::unique_lock<std::mutex> hold(detail::g_io_request_queue_mutex);
            while(detail::g_io_request_queue.empty())
                detail::g_io_request_available_cv.wait(hold);
            r = g_io_request_queue.front();
            g_io_request_queue.pop_front();
        }

        for(detail::io_request& request : detail::g_io_request_queue) {
            int res = write(r.fildes, r.buf, r.nbyte);
            g_io_result_queue.emplace_back(

        }
    }
}
