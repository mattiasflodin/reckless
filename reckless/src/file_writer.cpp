#include "reckless/file_writer.hpp"

#include <system_error>

#include <sys/stat.h>   // open()
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

namespace {
    class error_category : public std::error_category {
    public:
        char const* name() const noexcept override
        {
            return "reckless::file_writer";
        }
        std::error_condition default_error_condition(int code) const noexcept override
        {
            return std::system_category().default_error_condition(code);
        }
        bool equivalent(int code, std::error_condition const& condition) const noexcept override
        {
            if(condition.category() == reckless::writer::error_category())
                return file_writer_to_writer_category(code) == condition.value();
            else
                return std::system_category().equivalent(code, condition);
        }
        // This overload is called if we have an error_condition that belongs to
        // this error_category, and compare it to an error_code belonging to
        // another error_category. Since the error_category is in the anonymous
        // namespace, the only way that would happen is if you convert an
        // error_code that comes from here into an error_condition, then try to
        // compare it to an error_code from another category. It's a
        // pathological situation, but the correct way to implement it should be
        // to map the condition value from this category to
        // reckless::writer::error_category() if the error_code is in that
        // category, then do the comparison. It essentially reverses the
        // error_code / error_condition relationship.
        bool equivalent(std::error_code const& code, int condition) const noexcept override
        {
            if(code.category() == reckless::writer::error_category())
                return file_writer_to_writer_category(condition) == code.value();
            else
                return std::system_category().equivalent(code, condition);
        }
        std::string message(int condition) const override
        {
            return std::system_category().message(condition);
        }
    private:
        int file_writer_to_writer_category(int code) const
        {
            switch(code) {
            case ENOSPC:
            case ENOBUFS:
            case EDQUOT:
            case EIO:
                return reckless::writer::temporary_failure;
            default:
                return reckless::writer::permanent_failure;
            }
        }
    };

    error_category const& get_error_category()
    {
        static error_category cat;
        return cat;
    }
}

reckless::file_writer::file_writer(char const* path) :
    fd_(-1)
{
    auto full_access =
        S_IRUSR | S_IWUSR | S_IXUSR |
        S_IRGRP | S_IWGRP | S_IXGRP |
        S_IROTH | S_IWOTH | S_IXOTH;
    fd_ = open(path, O_WRONLY | O_CREAT, full_access);
    if(fd_ == -1)
        throw std::system_error(errno, std::system_category());
    lseek(fd_, 0, SEEK_END);
}

reckless::file_writer::~file_writer()
{
    if(fd_ != -1)
        close(fd_);
}

std::size_t reckless::file_writer::write(void const* pbuffer, std::size_t count, std::error_code& ec) noexcept
{
    char const* p = static_cast<char const*>(pbuffer);
    char const* pend = p + count;
    ec.clear();
    while(p != pend) {
        ssize_t written = ::write(fd_, p, count);
        if(written == -1) {
            if(errno != EINTR) {
                ec.assign(errno, get_error_category());
                break;
            }
        } else {
            p += written;
        }
    }
    return p - static_cast<char const*>(pbuffer);
}
