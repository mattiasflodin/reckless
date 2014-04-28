#include "asynclog.hpp"
#include "asynclog/detail/utility.hpp"

#include <sys/mman.h>   // madvise()

asynclog::output_buffer::output_buffer(writer* pwriter, std::size_t max_capacity) :
    pwriter_(pwriter),
    pbuffer_(nullptr),
    pcommit_end_(nullptr),
    pbuffer_end_(nullptr)
{
    using namespace detail;
    pbuffer_ = static_cast<char*>(std::malloc(max_capacity));
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
    pwriter_->write(pbuffer_, pcommit_end_ - pbuffer_);
    pcommit_end_ = pbuffer_;
}


