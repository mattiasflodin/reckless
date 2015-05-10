#ifndef RECKLESS_FILE_WRITER_HPP
#define RECKLESS_FILE_WRITER_HPP

#include <reckless/writer.hpp>

namespace reckless {
    
class file_writer : public writer {
public:
    file_writer(char const* path);
    ~file_writer();
    Result write(void const* pbuffer, std::size_t count);
private:
    int fd_;
};

}   // namespace reckless

#endif  // RECKLESS_FILE_WRITER_HPP

