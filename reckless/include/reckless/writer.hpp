#ifndef RECKLESS_WRITER_HPP
#define RECKLESS_WRITER_HPP

#include <cstdlib>  // size_t
#include <system_error>     // error_code, error_condition

// TODO synchronous log for wrapping a channel and calling the formatter immediately. Or, just add a bool to basic_log?

namespace reckless {

// TODO this is a bit vague, rename to e.g. log_target or someting?
class writer {
public:
    enum errc
    {
        temporary_failure = 1,
        permanent_failure = 2
    };
    static std::error_category const& error_category();

    virtual ~writer() = 0;
    virtual std::size_t write(void const* pbuffer, std::size_t count,
            std::error_code& ec) noexcept = 0;
};

inline std::error_condition make_error_condition(writer::errc ec)
{
    return std::error_condition(ec, writer::error_category());
}

inline std::error_code make_error_code(writer::errc ec)
{
    return std::error_code(ec, writer::error_category());
}
}   // namespace reckless

namespace std
{
    template <>
    struct is_error_condition_enum<reckless::writer::errc> : public true_type {};
}
#endif  // RECKLESS_WRITER_HPP
