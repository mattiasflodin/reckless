#include "dlog.hpp"

#include <memory>
#include <algorithm>
#include <new>
#include <thread>
#include <deque>

#include <sstream> // TODO probably won't need this when all is said and done

#include <cstring>
#include <cassert>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

namespace dlog {
    namespace detail {
        void output_worker();

        int const g_page_size = static_cast<int>(sysconf(_SC_PAGESIZE));

        thread_local input_buffer tls_input_buffer; 
        std::size_t g_input_buffer_size;   // static

        std::mutex g_input_queue_mutex;
        std::deque<flush_extent> g_input_queue;
        std::condition_variable g_input_available_condition;

        std::unique_ptr<output_buffer> g_poutput_buffer;
        std::thread g_output_thread;
    }
}

void dlog::initialize(writer* pwriter)
{
    initialize(pwriter, 0, 0);
}

void dlog::initialize(writer* pwriter, std::size_t input_buffer_size,
        std::size_t max_output_buffer_size)
{
    using namespace detail;

    if(input_buffer_size == 0)
        input_buffer_size = g_page_size;
    if(max_output_buffer_size == 0)
        max_output_buffer_size = 1024*1024;
    g_input_buffer_size = input_buffer_size;

    g_poutput_buffer.reset(new output_buffer(pwriter, max_output_buffer_size));

    g_output_thread = move(std::thread(&output_worker));
}

void dlog::cleanup()
{
    using namespace detail;
    {
        std::unique_lock<std::mutex> lock(g_input_queue_mutex);
        g_input_queue.push_back({nullptr, 0});
    }
    g_input_available_condition.notify_one();
    g_output_thread.join();
    g_poutput_buffer.reset();
}

dlog::writer::~writer()
{
}

dlog::file_writer::file_writer(char const* path) :
    fd_(-1)
{
    fd_ = open(path, O_WRONLY | O_CREAT);
    // TODO proper exception here.
    if(fd_ == -1)
        throw std::runtime_error("cannot open file");
    lseek(fd_, 0, SEEK_END);
}

dlog::file_writer::~file_writer()
{
    if(fd_ != -1)
        close(fd_);
}

auto dlog::file_writer::write(void const* pbuffer, std::size_t count) -> Result
{
    char const* p = static_cast<char const*>(pbuffer);
    while(count != 0) {
        ssize_t written = ::write(fd_, p, count);
        if(written == -1) {
            if(errno != EINTR)
                break;
        } else {
            p += written;
            count -= written;
        }
    }
    if(count == 0)
        return SUCCESS;

    // TODO handle broken pipe signal?
    switch(errno) {
    case EFBIG:
    case EIO:
    case EPIPE:
    case ERANGE:
    case ECONNRESET:
    case EINVAL:
    case ENXIO:
    case EACCES:
    case ENETDOWN:
    case ENETUNREACH:
        // TODO handle this error by not writing to the buffer any more.
        return ERROR_GIVE_UP;
    case ENOSPC:
        return ERROR_TRY_LATER;
    default:
        // TODO throw proper error
        throw std::runtime_error("cannot write to file descriptor");
    }
}

dlog::output_buffer::output_buffer(writer* pwriter, std::size_t max_capacity) :
    pwriter_(pwriter),
    pbuffer_(nullptr),
    pcommit_end_(nullptr),
    pbuffer_end_(nullptr)
{
    using namespace detail;
    pbuffer_ = static_cast<char*>(malloc(max_capacity));
    pcommit_end_ = pbuffer_;
    pbuffer_end_ = pbuffer_ + max_capacity;
    madvise(pbuffer_ + g_page_size, max_capacity - g_page_size, MADV_DONTNEED);
}

dlog::output_buffer::~output_buffer()
{
    free(pbuffer_);
}

char* dlog::output_buffer::reserve(std::size_t size)
{
    if(pbuffer_end_ - pcommit_end_ < size) {
        flush();
        // TODO if the flush fails above, the only thing we can do is discard
        // the data. But perhaps we should invoke a callback that can do
        // something, such as log a message about the discarded data.
        if(pbuffer_end_ - pbuffer_ < size)
            throw std::bad_alloc();
    }
    return pcommit_end_;
}

void dlog::output_buffer::commit(std::size_t size)
{
    pcommit_end_ += size;
}

void dlog::output_buffer::flush()
{
    // TODO keep track of a high watermark, i.e. max value of pcommit_end_.
    // Clear every second or some such. Use madvise to release unused memory.
    pwriter_->write(pbuffer_, pcommit_end_ - pbuffer_);
    pcommit_end_ = pbuffer_;
}

namespace dlog {
namespace {
    template <typename T>
    bool generic_format_int(output_buffer* pbuffer, char const*& pformat, T v)
    {
        char f = *pformat;
        if(f == 'd') {
            std::ostringstream ostr;
            ostr << v;
            std::string const& s = ostr.str();
            char* p = pbuffer->reserve(s.size());
            std::memcpy(p, s.data(), s.size());
            pbuffer->commit(s.size());
            pformat += 1;
            return true;
        } else if(f == 'x') {
            // FIXME
            return false;
        } else if(f == 'b') {
            // FIXME
            return false;
        } else {
            return false;
        }
    }

    template <typename T>
    bool generic_format_float(output_buffer* pbuffer, char const*& pformat, T v)
    {
        char f = *pformat;
        if(f != 'f')
            return false;

        std::ostringstream ostr;
        ostr << v;
        std::string const& s = ostr.str();
        char* p = pbuffer->reserve(s.size());
        std::memcpy(p, s.data(), s.size());
        pbuffer->commit(s.size());
        pformat += 1;
        return true;
    }

    template <typename T>
    bool generic_format_char(output_buffer* pbuffer, char const*& pformat, T v)
    {
        char f = *pformat;
        if(f == 's') {
            char* p = pbuffer->reserve(1);
            *p = static_cast<char>(v);
            pbuffer->commit(1);
            pformat += 1;
            return true;
        } else {
            return generic_format_int(pbuffer, pformat, static_cast<int>(v));
        }
    }
}   // anonymous namespace
}   // namespace dlog

bool dlog::format(output_buffer* pbuffer, char const*& pformat, char v)
{
    return generic_format_char(pbuffer, pformat, v);
}

bool dlog::format(output_buffer* pbuffer, char const*& pformat, signed char v)
{
    return generic_format_char(pbuffer, pformat, v);
}

bool dlog::format(output_buffer* pbuffer, char const*& pformat, unsigned char v)
{
    return generic_format_char(pbuffer, pformat, v);
}

//bool dlog::format(output_buffer* pbuffer, char const*& pformat, wchar_t v);
//bool dlog::format(output_buffer* pbuffer, char const*& pformat, char16_t v);
//bool dlog::format(output_buffer* pbuffer, char const*& pformat, char32_t v);

bool dlog::format(output_buffer* pbuffer, char const*& pformat, short v)
{
    return generic_format_int(pbuffer, pformat, v);
}

bool dlog::format(output_buffer* pbuffer, char const*& pformat, unsigned short v)
{
    return generic_format_int(pbuffer, pformat, v);
}

bool dlog::format(output_buffer* pbuffer, char const*& pformat, int v)
{
    return generic_format_int(pbuffer, pformat, v);
}

bool dlog::format(output_buffer* pbuffer, char const*& pformat, unsigned int v)
{
    return generic_format_int(pbuffer, pformat, v);
}

bool dlog::format(output_buffer* pbuffer, char const*& pformat, long v)
{
    return generic_format_int(pbuffer, pformat, v);
}

bool dlog::format(output_buffer* pbuffer, char const*& pformat, unsigned long v)
{
    return generic_format_int(pbuffer, pformat, v);
}

bool dlog::format(output_buffer* pbuffer, char const*& pformat, long long v)
{
    return generic_format_int(pbuffer, pformat, v);
}

bool dlog::format(output_buffer* pbuffer, char const*& pformat, unsigned long long v)
{
    return generic_format_int(pbuffer, pformat, v);
}

bool dlog::format(output_buffer* pbuffer, char const*& pformat, float v)
{
    return generic_format_float(pbuffer, pformat, v);
}

bool dlog::format(output_buffer* pbuffer, char const*& pformat, double v)
{
    return generic_format_float(pbuffer, pformat, v);
}

bool dlog::format(output_buffer* pbuffer, char const*& pformat, long double v)
{
    return generic_format_float(pbuffer, pformat, v);
}

bool dlog::format(output_buffer* pbuffer, char const*& pformat, char const* v)
{
    if(*pformat != 's')
        return false;
    auto len = std::strlen(v);
    char* p = pbuffer->reserve(len);
    std::memcpy(p, v, len);
    pbuffer->commit(len);
    ++pformat;
    return true;
}

bool dlog::format(output_buffer* pbuffer, char const*& pformat, std::string const& v)
{
    if(*pformat != 's')
        return false;
    auto len = v.size();
    char* p = pbuffer->reserve(len);
    std::memcpy(p, v.data(), len);
    pbuffer->commit(len);
    ++pformat;
    return true;
}

dlog::detail::input_buffer::input_buffer() :
    pbegin_(allocate_buffer())
{
    pinput_start_ = pbegin_;
    pinput_end_ = pbegin_;
}

dlog::detail::input_buffer::~input_buffer()
{
    flush();
    while(pinput_start_.load(std::memory_order_acquire) != pinput_end_)
        wait_input_consumed();

    free(pbegin_);
}

// Helper for allocating aligned ring buffer in ctor.
char* dlog::detail::input_buffer::allocate_buffer()
{
    void* pbuffer = nullptr;
    posix_memalign(&pbuffer, FRAME_ALIGNMENT, g_input_buffer_size);
    return static_cast<char*>(pbuffer);
}


// Moves an input-buffer pointer forward by the given distance while
// maintaining the invariant that:
//
// * p is aligned by FRAME_ALIGNMENT
// * p never points at the end of the buffer; it always wraps around to the
//   beginning of the circular buffer.
// * p does not point to a wraparound marker. If it would end at a wraparound
//   marker, then that marker is honored and the returned pointer will point to
//   the beginning of the buffer
//
// The distance must never be so great that the pointer moves *past* the end of
// the buffer. To do so would be an error in our context, since no input frame
// is allowed to be discontinuous.
char* dlog::detail::input_buffer::advance_frame_pointer(char* p, std::size_t distance)
{
    p = align(p + distance, FRAME_ALIGNMENT);
    auto remaining = p - (pbegin_ + g_input_buffer_size);
    assert(remaining >= 0);
    if(remaining == 0 or WRAPAROUND_MARKER ==
            *reinterpret_cast<dispatch_function_t**>(p))
    {
        p = pbegin_;
    }
    return p;
}

char* dlog::detail::input_buffer::allocate_input_frame(std::size_t size)
{
    // Conceptually, we have the invariant that
    //   pinput_start <= pflush_end <= pinput_end,
    // and the memory area after pinput_end is free for us to use for
    // allocating a frame. However, the fact that it's a circular buffer means
    // that:
    // 
    // * The area after pinput_end is actually non-contiguous, wraps around
    //   at the end of the buffer and ends at pinput_start.
    //   
    // * Except, when pinput_end itself has fallen over the right edge and we
    //   have the case pinput_end <= pinput_start. Then the free memory is
    //   contiguous (it still starts at pinput_end and ends at pinput_start).
    //   
    // (This is much easier to understand by drawing it on a paper than
    // by reading comment text).
    while(true) {
        auto pinput_end = pinput_end_;
        assert(pinput_end - pbegin_ != g_input_buffer_size);
        assert(is_aligned(pinput_end, FRAME_ALIGNMENT));

        auto pinput_start = pinput_start_.load(std::memory_order_acquire);
        auto free = pinput_start - pinput_end;
        if(free >= 0) {
            // Free space is contiguous.
            if(size <= free) {
                return pinput_end;
            } else {
                // Not enough room. Wait for the output thread to consume some
                // input.
                wait_input_consumed();
            }
        } else {
            // Free space is non-contiguous.
            free += g_input_buffer_size;
            if(size <= free) {
                // There's enough room in the first segment.
                return pinput_end;
            } else {
                std::size_t free2 = pinput_start - pbegin_;
                if(size <= free2) {
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
                    pinput_end_ = pbegin_;
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

char* dlog::detail::input_buffer::input_start() const
{
    return pinput_start_.load(std::memory_order_relaxed);
}

char* dlog::detail::input_buffer::input_end() const
{
    return pinput_end_;
}

char* dlog::detail::input_buffer::discard_input_frame(std::size_t size)
{
    // We can use relaxed memory ordering everywhere here because there is
    // nothing being written of interest that the pointer update makes visible;
    // all it does is *discard* data, not provide any new data (besides,
    // signaling the event is likely to create a full memory barrier anyway).
    auto p = pinput_start_.load(std::memory_order_relaxed);
    p = advance_frame_pointer(p, size);
    pinput_start_.store(p, std::memory_order_relaxed);
    signal_input_consumed();
    return p;
}

void dlog::detail::input_buffer::signal_input_consumed()
{
    input_consumed_event_.notify_one();
}

void dlog::detail::input_buffer::wait_input_consumed()
{
    // This is kind of icky, we need to lock a mutex just because the condition
    // variable requires it. There would be less overhead if we could just use
    // something like Windows event objects.
    std::unique_lock<std::mutex> lock(mutex_);
    input_consumed_event_.wait(lock);
}
void dlog::detail::output_worker()
{
    using namespace detail;
    while(true) {
        flush_extent fe;
        {
            std::unique_lock<std::mutex> lock(g_input_queue_mutex);
            g_input_available_condition.wait(lock, []()
            {
                return not g_input_queue.empty();
            });
            fe = g_input_queue.front();
            g_input_queue.pop_front();
        }
        if(not fe.pinput_buffer)
            // Request to shut down thread.
            return;

        char* pinput_start = fe.pinput_buffer->input_start();
        while(pinput_start != fe.pflush_end) {
            auto pdispatch = *reinterpret_cast<dispatch_function_t**>(pinput_start);
            auto frame_size = (*pdispatch)(g_poutput_buffer.get(), pinput_start);
            pinput_start = fe.pinput_buffer->discard_input_frame(frame_size);
        }
        // TODO we *could* do something like flush on a timer instead when we're getting a lot of writes / sec.
        g_poutput_buffer->flush();
    }


}
