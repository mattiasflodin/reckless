#include <reckless/policy_log.hpp>
#include <unistd.h>

#include "unreliable_writer.hpp"

unreliable_writer writer;
int main()
{
    reckless::policy_log<> log(&writer);
    
    sleep(2);
    log.write("(2) Write by log");
    std::cout << "(1) Write by cout " << std::endl;
    
    sleep(2);
    log.write("(3) Write by log, then flush");
    log.flush();
    std::cout << "(4) Write by cout after flush " << std::endl;
    
    sleep(2);
    writer.error_code.assign(static_cast<int>(std::errc::no_space_on_device),
            get_error_category());
    std::cout <<"Simulating disk full" << std::endl;
    log.write("(6) Write by log, then flush");
    try {
        log.flush();
    } catch(reckless::writer_error const& e) {
        std::cout << "(5) Catch writer_error " << e.code() << " on flush"
            << std::endl;
    }
    sleep(2);
    std::cout <<"Simulating disk no longer full" << std::endl;
    writer.error_code.clear();
    log.flush();
    
    sleep(2);
    std::cout <<"Simulating disk full" << std::endl;
    writer.error_code.assign(static_cast<int>(std::errc::no_space_on_device),
            get_error_category());
    log.write("(8) Write by log, then flush");
    std::error_code error;
    log.flush(error);
    if(error) {
        std::cout << "(7) Obtain error code " << error << " on flush"
            << std::endl;
    }
    writer.error_code.clear();
    log.flush();

    return 0;
}
