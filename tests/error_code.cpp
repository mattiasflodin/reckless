#include <reckless/file_writer.hpp>
#include <iostream>

int main()
{
    std::error_code code;
    std::error_condition special_ec(EINTR, code.category());
    std::cout << code.category().name() << std::endl;
    bool eq = (code == reckless::writer::temporary_failure);
    eq = (reckless::writer::temporary_failure == code);
    eq = (code == std::errc::too_many_links);
    eq = (code == std::errc::no_space_on_device);
    code.clear();
    eq = (code == special_ec);
    std::cout << eq << std::endl;
    return 0;
}
