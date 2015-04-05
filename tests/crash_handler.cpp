#include <asynclog.hpp>
#include <asynclog/crash_handler.hpp>

asynclog::policy_log<> g_log;

int main()
{
    asynclog::scoped_crash_handler crash_handler({&g_log});
    asynclog::file_writer writer("log.txt");
    g_log.open(&writer);
    g_log.write("Hello World!\n");
    char* p = nullptr;
    *p = 0;
    return 0;
}
