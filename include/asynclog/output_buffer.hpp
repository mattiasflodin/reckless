#ifndef ASYNCLOG_OUTPUT_BUFFER_HPP
#define ASYNCLOG_OUTPUT_BUFFER_HPP

namespace asynclog {

// TODO this is a bit vague, rename to e.g. log_target or someting?
class writer {
public:
    enum Result
    {
        SUCCESS,
        ERROR_TRY_LATER,
        ERROR_GIVE_UP
    };
    virtual ~writer() = 0;
    virtual Result write(void const* pbuffer, std::size_t count) = 0;
};

class output_buffer {
public:
    output_buffer();
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
    void flush();

private:
    output_buffer(output_buffer const&);    // not defined
    output_buffer& operator=(output_buffer const&); // not defined

    writer* pwriter_;
    char* pbuffer_;
    char* pcommit_end_;
    char* pbuffer_end_;
};

}

#endif  // ASYNCLOG_OUTPUT_BUFFER_HPP
