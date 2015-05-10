#ifndef RECKLESS_WRITER_HPP
#define RECKLESS_WRITER_HPP

#include <cstdlib>  // size_t

// TODO synchronous log for wrapping a channel and calling the formatter immediately. Or, just add a bool to basic_log?

namespace reckless {
    
// TODO this is a bit vague, rename to e.g. log_target or someting?
class writer {
public:
    enum Result
    {
        SUCCESS,
        // FIXME honor these error codes
        ERROR_TRY_LATER,
        ERROR_GIVE_UP
    };
    virtual ~writer() = 0;
    virtual Result write(void const* pbuffer, std::size_t count) = 0;
};

}   // namespace reckless

#endif  // RECKLESS_WRITER_HPP
