// Example of a formatter for a user-defined type.
#include <reckless/policy_log.hpp>
#include <reckless/file_writer.hpp>
#include <reckless/ntoa.hpp>
    
reckless::file_writer writer("log.txt");
reckless::policy_log<> g_log(&writer);

struct Coordinate
{
    int x;
    int y;
};

char const* format(reckless::output_buffer* pbuffer, char const* fmt, Coordinate const& c)
{
    pbuffer->write("Hello ");
    if(*fmt == 'd')
        reckless::template_formatter::format(pbuffer, "(%d, %d)", c.x, c.y);
    else if(*fmt == 'x')
        reckless::template_formatter::format(pbuffer, "(%#x, %x)", c.x, c.y);
    else
        return nullptr;
    return fmt+1;
}

int main()
{
    Coordinate c{13, 17};
    g_log.write("%d", c);
    g_log.write("%x", c);

    return 0;
}

