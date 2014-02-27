#include "dlog.hpp"

#include <cstring>
#include <memory>
#include <algorithm>
#include <new>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

namespace dlog {
    namespace detail {
        input_buffer g_input_buffer; 
    }
    namespace {
        std::unique_ptr<output_buffer> g_poutput_buffer;

        int const g_page_size = static_cast<int>(sysconf(_SC_PAGESIZE));
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
    g_input_buffer.pfirst = new char[BUFFER_SIZE];
    g_input_buffer.plast = g_input_buffer.pfirst + BUFFER_SIZE;
    g_input_buffer.pflush_start = g_input_buffer.pfirst;
    g_input_buffer.pflush_end = g_input_buffer.pfirst;
    g_input_buffer.pwritten_end = g_input_buffer.pfirst;

    g_poutput_buffer.reset(new output_buffer(pwriter, max_capacity));
}

void dlog::cleanup()
{
    using namespace detail;
    delete [] g_input_buffer.pfirst;
    g_poutput_buffer.reset();
    g_input_buffer.pfirst = nullptr;
    g_input_buffer.plast = nullptr;
    g_input_buffer.pflush_start = nullptr;
    g_input_buffer.pflush_end = nullptr;
    g_input_buffer.pwritten_end = nullptr;
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

//bool dlog::format(output_buffer* pbuffer, char const*& pformat, std::uint8_t v);
//bool dlog::format(output_buffer* pbuffer, char const*& pformat, std::uint16_t v);
//bool dlog::format(output_buffer* pbuffer, char const*& pformat, std::uint32_t v);
//bool dlog::format(output_buffer* pbuffer, char const*& pformat, std::uint64_t v);
