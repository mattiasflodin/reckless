#ifndef RECKLESS_OUTPUT_BUFFER_HPP
#define RECKLESS_OUTPUT_BUFFER_HPP

#include "detail/branch_hints.hpp"

#include <cstddef>  // size_t
#include <new>      // bad_alloc
#include <cstring>  // strlen, memcpy

namespace reckless {
class writer;

class output_buffer {
public:
    output_buffer();
    // TODO hide functions that are not relevant to the client, e.g. move
    // assignment, empty(), flush etc?
    output_buffer(output_buffer&& other);
    output_buffer(writer* pwriter, std::size_t max_capacity);
    ~output_buffer();

    output_buffer& operator=(output_buffer&& other);

    void reset(writer* pwriter, std::size_t max_capacity);

    char* reserve(std::size_t size)
    {
        if(detail::unlikely(static_cast<std::size_t>(pbuffer_end_ - pcommit_end_) < size)) {
            flush();
            // TODO if the flush fails above, the only thing we can do is discard
            // the data. But perhaps we should invoke a callback that can do
            // something, such as log a message about the discarded data.
            // FIXME when does it actually fail though? Do we need an exception
            // handler? This block should perhaps be made non-inline. Looks
            // like we're not actually handling the return value of
            // pwriter_->write().
            if(static_cast<std::size_t>(pbuffer_end_ - pbuffer_) < size)
                throw std::bad_alloc();
        }
        return pcommit_end_;
    }

    void commit(std::size_t size)
    {
        pcommit_end_ += size;
    }
    
    void write(void const* buf, std::size_t count)
    {
        // TODO this could be smarter by writing from the client-provided
        // buffer instead of copying the data.
        auto const buffer_size = pbuffer_end_ - pbuffer_;
        
        char const* cp = static_cast<char const*>(buf);
        char const* end = cp + count;
        auto remaining = static_cast<std::size_t>(end - cp);
        auto available = static_cast<std::size_t>(pbuffer_end_ - pbuffer_);
        while(detail::unlikely(remaining > available)) {
            std::memcpy(pbuffer_end_, cp, available);
            cp += available;
            remaining -= available;
            available = buffer_size;
            pcommit_end_ = pbuffer_end_;
            flush();
        }
        
        std::memcpy(pcommit_end_, cp, remaining);
        pcommit_end_ += remaining;
    }
    void write(char const* s)
    {
        write(s, std::strlen(s));
    }

    void write(char c)
    {
        char* p = reserve(1);
        *p = c;
        commit(1);
    }
    
    bool empty() const
    {
        return pcommit_end_ == pbuffer_;
    }
    void flush();

private:
    output_buffer(output_buffer const&) = delete;
    output_buffer& operator=(output_buffer const&) = delete;

    writer* pwriter_;
    char* pbuffer_;
    char* pcommit_end_;
    char* pbuffer_end_;
};

}

#endif  // RECKLESS_OUTPUT_BUFFER_HPP
