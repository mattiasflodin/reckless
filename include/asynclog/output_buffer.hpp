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
    output_buffer(writer* pwriter, std::size_t max_capacity);
    ~output_buffer();
    char* reserve(std::size_t size);
    void commit(std::size_t size)
    {
        pcommit_end_ += size;
    }
    void flush();

private:
    writer* pwriter_;
    char* pbuffer_;
    char* pcommit_end_;
    char* pbuffer_end_;
};

}

#endif  // ASYNCLOG_OUTPUT_BUFFER_HPP
