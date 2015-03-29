#include <asynclog.hpp>

using log_t = asynclog::severity_log<
    asynclog::indent<4, '\t'>,    // 4 spaces of indent
    ' ',                    // Field separator
    asynclog::severity_field,
    asynclog::timestamp_field
    >;
    
asynclog::file_writer writer("log.txt");
log_t g_log;

int main()
{
    g_log.open(&writer);

    std::string s("Hello World!");
    g_log.debug("Pointer: %p\n", s.c_str());
    g_log.info("Info line: %s\n", s);
    g_log.warn("Warning: %d\n", 24);
    g_log.error("Error: %f\n", 3.14);

    return 0;
}
