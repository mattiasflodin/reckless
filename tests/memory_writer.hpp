#ifndef TESTS_MEMORY_WRITER_HPP
#define TESTS_MEMORY_WRITER_HPP
#include <reckless/writer.hpp>

template <class Container>
class memory_writer : public reckless::writer {
public:
    Result write(void const* data, std::size_t size) override
    {
        char const* p = static_cast<char const*>(data);
        container.insert(container.end(), p, p+size);
        return SUCCESS;
    }
    Container container;
};

#endif  // TESTS_MEMORY_WRITER_HPP
