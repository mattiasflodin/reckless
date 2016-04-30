#include <reckless/policy_log.hpp>
#include <reckless/file_writer.hpp>
#include <reckless/crash_handler.hpp>

reckless::policy_log<> g_log;

int main()
{
    reckless::scoped_crash_handler crash_handler({&g_log});
    reckless::file_writer writer("log.txt");
    g_log.open(&writer);
    g_log.write("Hello World!");
    char* p = nullptr;
    *p = 0;
    return 0;
}
