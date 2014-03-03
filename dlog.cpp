#include "dlog.hpp"

#include <cstring>
#include <memory>
#include <algorithm>
#include <new>
#include <thread>

#include <sstream> // TODO probably won't need this when all is said and done

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

namespace dlog {
    namespace detail {
        input_buffer g_input_buffer; 
        std::mutex g_input_buffer_mutex;
        std::condition_variable g_input_available_condition;
        std::condition_variable g_input_consumed_condition;
    }
    namespace {
        std::unique_ptr<output_buffer> g_poutput_buffer;
        std::thread g_output_thread;

        int const g_page_size = static_cast<int>(sysconf(_SC_PAGESIZE));

        void output_worker();
    }
}

void dlog::initialize(writer* pwriter)
{
    initialize(pwriter, 1024*1024);
}

void dlog::initialize(writer* pwriter, std::size_t max_capacity)
{
    using namespace detail;

    std::size_t const BUFFER_SIZE = g_page_size;
    // FIXME exception safety for this pointer
    g_input_buffer.pbuffer = new char[BUFFER_SIZE];
    g_input_buffer.pbegin = align_up(g_input_buffer.pbuffer, FRAME_ALIGNMENT);
    auto pend = align_down(g_input_buffer.pbuffer + BUFFER_SIZE);
    g_input_buffer.size = pend - g_input_buffer.pbegin;
    g_input_buffer.pflush_start = g_input_buffer.pbegin;
    g_input_buffer.pflush_end = g_input_buffer.pbegin;
    g_input_buffer.pwritten_end = g_input_buffer.pbegin;

    g_poutput_buffer.reset(new output_buffer(pwriter, max_capacity));

    g_output_thread = move(std::thread(&output_worker));
}

void dlog::cleanup()
{
    using namespace detail;
    char* frame = allocate_input_frame(sizeof(CLEANUP_MARKER));
    *reinterpret_cast<dispatch_function_t**>(frame) = CLEANUP_MARKER;
    flush();
    g_output_thread.join();
    delete [] g_input_buffer.pfirst;
    g_poutput_buffer.reset();
    g_input_buffer.pfirst = nullptr;
    g_input_buffer.plast = nullptr;
    g_input_buffer.pflush_start = nullptr;
    g_input_buffer.pflush_end = nullptr;
    g_input_buffer.pwritten_end = nullptr;
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

void dlog:::detail::output_worker()
{
    char const* pflush_start = g_input_buffer.pflush_start;
    while(true) {
        char const* pflush_end;
        {
            std::unique_lock<std::mutex> lock(g_input_buffer_mutex);
            g_input_buffer.pflush_start = pflush_start;
            g_input_available_condition.wait(lock, []() {
                return g_input_buffer.pflush_start == g_input_buffer.pflush_end;
            });
            pflush_start = g_input_buffer.pflush_start;
            pflush_end = g_input_buffer.pflush_end;
        }
        while(pflush_start != pflush_end) {
            auto pdispatch = *reinterpret_cast<dispatch_function_t**>(
                    pflush_start);

            if(pdispatch == WRAPAROUND_MARKER) {
                pflush_start = g_input_buffer.pbegin;
                pdispatch = *reinterpret_cast<dispatch_function_t**>(
                        pflush_start);
            } else if(pdispatch == CLEANUP_MARKER) {
                g_poutput_buffer->flush();
                return;
            }

            auto frame_size = (*pdispatch)(g_poutput_buffer.get(),
                    pflush_start);
            pflush_start = advance_frame_pointer(frame_size);
            if(pflush_start >= g_input_buffer.pend)
                pflush_start -= g_input_buffer.size;
        }
        g_input_consumed_condition.notify();
        g_poutput_buffer->flush();
    }
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
inline char* advance_frame_pointer(char* p, std::size_t distance)
{
    p = align(p + distance, FRAME_ALIGNMENT);
    auto remaining = p - (g_input_buffer.pbegin + g_input_buffer.size);
    assert(remaining >= 0);
    if(remaining == 0)
        p = g_input_buffer.pbegin;
    return p;
}

char* dlog::detail::allocate_input_frame(std::unique_lock<std::mutex>& lock,
        std::size_t size)
{
    // Conceptually, we have the invariant that
    //   pflush_start <= pflush_end <= pfree_start,
    // and the memory area after pfree_start is free for us to use for
    // allocating a frame. However, the fact that it's a circular buffer means
    // that:
    // 
    // * The area after pfree_start is actually non-contiguous, wraps around
    //   at the end of the buffer and ends at pflush_start.
    //   
    // * Except, when pfree_start itself has fallen over the right edge and we
    //   have the case pfree_start <= pflush_start. Then the free memory is
    //   contiguous (it still starts at pfree_start and ends at pflush_start).
    //   
    // (This is much, much easier to understand by drawing it on a paper than
    // by reading comment text).
    while(true) {
        auto pfree_start = g_input_buffer.pfree_start;
        auto pflush_start = g_input_buffer.pflush_start;
        assert(pfree_start - g_input_buffer.pbegin != g_input_buffer.size);
        assert(is_aligned(pfree_start, FRAME_ALIGNMENT));

        auto free = pflush_start - pfree_start;
        if(free >= 0) {
            // Free space is contiguous.
            if(size <= free) {
                g_input_buffer.pfree_start = advance_frame_pointer(pfree_start,
                    size);
                return pfree_start;
            } else {
                // Not enough room. Wait for the output thread to consume some
                // input.
                g_input_consumed_condition.wait(lock);
            }
        } else {
            // Free space is non-contiguous.
            free += g_input_buffer.size;
            if(size <= free) {
                // There's enough room in the first segment.
                g_input_buffer.pfree_start = advance_frame_pointer(pfree_start,
                        size);
                return pfree_start;
            } else {
                std::size_t free2 = pflush_start - g_input_buffer.pbegin;
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
                    *reinterpret_cast<dispatch_function_t**>(pfree_start) =
                        WRAPAROUND_MARKER;
                    pfree_start = g_input_buffer.pbegin;
                    g_input_buffer.pfree_start = advance_frame_pointer(
                            pfree_start, size);
                    return pfree_start;
                } else {
                    // Not enough room. Wait for the output thread to consume
                    // some input.
                    g_input_consumed_condition.wait(lock);
                }
            }
        }
    }
}
