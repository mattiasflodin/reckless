#include <asynclog.hpp>

// It is possible to build custom loggers for various ways of formatting the
// log. The severity log is a stock policy-based logger that allows you to
// configure fields that should be put on each line, including a severity
// marker for debug/info/warning/error.
using log_t = asynclog::severity_log<
    asynclog::indent<4>,       // 4 spaces of indent
    ' ',                       // Field separator
    asynclog::severity_field,  // Show severity marker (D/I/W/E) first
    asynclog::timestamp_field  // Then timestamp field
    >;
    
asynclog::file_writer writer("log.txt");
log_t g_log(&writer);

int main()
{
    std::string s("Hello World!");
    g_log.debug("Pointer: %p\n", s.c_str());
    g_log.info("Info line: %s\n", s);
    for(int i=0; i!=4; ++i) {
        asynclog::scoped_indent indent;  // The indent object causes the lines
        g_log.warn("Warning: %d\n", i);  // within this scope to be indented
    }
    g_log.error("Error: %f\n", 3.14);

    return 0;
}
