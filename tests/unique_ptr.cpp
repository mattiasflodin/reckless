#include <reckless/policy_log.hpp>
#include <reckless/file_writer.hpp>
#include <memory>

namespace std
{
    char const* format(reckless::output_buffer* p, char const* fmt,
        std::unique_ptr<char> pc)
    {
        if(*fmt != 'c')
            return nullptr;
         p->write(*pc);
         return fmt+1;
    }
}

int main()
{
    reckless::file_writer writer("log.txt");
    reckless::policy_log<> log(&writer);
    log.write("%c", std::unique_ptr<char>(new char('A')));
    // C++14:
    //log.write("%c", std::make_unique<char>('A'));
}
