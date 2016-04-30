#ifndef RECKLESS_FILE_WRITER_HPP
#define RECKLESS_FILE_WRITER_HPP

#include <reckless/writer.hpp>

namespace reckless {
    
class file_writer : public writer {
public:
    file_writer(char const* path);
    ~file_writer();
    std::size_t write(void const* pbuffer, std::size_t count, std::error_code& ec) noexcept override;
private:
    int fd_;
};

}   // namespace reckless

#endif  // RECKLESS_FILE_WRITER_HPP

