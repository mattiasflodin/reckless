#include <reckless/writer.hpp>
#include <iostream>

class error_category : public std::error_category {
public:
    char const* name() const noexcept override
    {
        return "unreliable_writer";
    }
    std::error_condition default_error_condition(int code) const noexcept override
    {
        return std::system_category().default_error_condition(code);
    }
    bool equivalent(int code, std::error_condition const& condition) const noexcept override
    {
        if(condition.category() == reckless::writer::error_category())
            return errc_to_writer_category(code) == condition.value();
        else
            return std::system_category().equivalent(code, condition);
    }
    bool equivalent(std::error_code const& code, int condition) const noexcept override
    {
        if(code.category() == reckless::writer::error_category())
            return errc_to_writer_category(condition) == code.value();
        else
            return std::system_category().equivalent(code, condition);
    }
    std::string message(int condition) const override
    {
        return std::system_category().message(condition);
    }
private:
    int errc_to_writer_category(int code) const
    {
        if(code == static_cast<int>(std::errc::no_space_on_device))
            return reckless::writer::temporary_failure;
        else
            return reckless::writer::permanent_failure;
    }
};
    
inline error_category const& get_error_category()
{
    static error_category cat;
    return cat;
}

class unreliable_writer : public reckless::writer {
public:
    std::size_t write(void const* data, std::size_t size, std::error_code& ec) noexcept override
    {
        ec = error_code;
        if(!ec) {
            std::cout.write(static_cast<char const*>(data), size);
            return size;
        }
        return 0;
    }

    std::error_code error_code;
};

