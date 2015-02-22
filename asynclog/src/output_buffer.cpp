#include "asynclog.hpp"
#include "asynclog/detail/utility.hpp"

#include <sys/mman.h>   // madvise()

asynclog::output_buffer::output_buffer() :
    pwriter_(nullptr),
    pbuffer_(nullptr),
    pcommit_end_(nullptr),
    pbuffer_end_(nullptr)
{
}

asynclog::output_buffer::output_buffer(writer* pwriter, std::size_t max_capacity) :
    pwriter_(nullptr),
    pbuffer_(nullptr),
    pcommit_end_(nullptr),
    pbuffer_end_(nullptr)
{
    reset(pwriter, max_capacity);
}

asynclog::output_buffer::output_buffer(output_buffer&& other)
{
    pwriter_ = other.pwriter_;
    pbuffer_ = other.pbuffer_;
    pcommit_end_ = other.pcommit_end_;
    pbuffer_end_ = other.pbuffer_end_;

    other.pwriter_ = nullptr;
    other.pbuffer_ = nullptr;
    other.pcommit_end_ = nullptr;
    other.pbuffer_end_ = nullptr;
}

asynclog::output_buffer& asynclog::output_buffer::operator=(output_buffer&& other)
{
    std::free(pbuffer_);

    pwriter_ = other.pwriter_;
    pbuffer_ = other.pbuffer_;
    pcommit_end_ = other.pcommit_end_;
    pbuffer_end_ = other.pbuffer_end_;

    other.pwriter_ = nullptr;
    other.pbuffer_ = nullptr;
    other.pcommit_end_ = nullptr;
    other.pbuffer_end_ = nullptr;

    return *this;
}

void asynclog::output_buffer::reset(writer* pwriter, std::size_t max_capacity)
{
    using namespace detail;
    std::free(pbuffer_);

    pwriter_ = pwriter;
    pbuffer_ = static_cast<char*>(std::malloc(max_capacity));
    // FIXME check return value of malloc here 
    pcommit_end_ = pbuffer_;
    pbuffer_end_ = pbuffer_ + max_capacity;
    auto page = detail::get_page_size();
    madvise(pbuffer_ + page, max_capacity - page, MADV_DONTNEED);
}

asynclog::output_buffer::~output_buffer()
{
    std::free(pbuffer_);
}

void asynclog::output_buffer::flush()
{
    // TODO keep track of a high watermark, i.e. max value of pcommit_end_.
    // Clear every second or some such. Use madvise to release unused memory.
    
    // NOTE if you get a crash here, it could be because your log object has a
    // longer lifetime than the writer (i.e. the writer has been destroyed
    // already).
    // 
    // TODO the above error happens if you have g_log as a global object and
    // have a writer with local scope (e.g. in main()), *even if you do not
    // write to the log after the writer goes out of scope*, because there can
    // be stuff lingering in the async queue. This makes the error pretty
    // obscure, and we should guard against it. Perhaps by taking the writer as
    // a shared_ptr, or at least by leaving a huge warning in the
    // documentation.
    pwriter_->write(pbuffer_, pcommit_end_ - pbuffer_);
    pcommit_end_ = pbuffer_;
}


