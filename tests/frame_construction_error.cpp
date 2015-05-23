#include "memory_writer.hpp"
#include <reckless/policy_log.hpp>

#include <vector>
#include <cassert>
#include <stdexcept>

#include <iostream>

memory_writer<std::string> g_writer;
reckless::policy_log<> g_log;

struct Object {
    Object()
    {
    }
    Object(Object const&)
    {
        throw std::runtime_error("runtime error");
    }
};

char const* format(reckless::output_buffer* poutput, char const* fmt, Object)
{
    if(*fmt != 's')
        return nullptr;
    poutput->write('X');
    return fmt+1;
}

int main()
{
    g_log.open(&g_writer);
    g_log.write("Hello");
    Object object;
    try {
        g_log.write("%s", object);
    } catch(std::runtime_error const&) {
    }
    sleep(3);
    g_log.write("World!");
    g_log.close();
    std::cout << g_writer.container;
    assert(g_writer.container == "Hello\nWorld!\n");
    
    return 0;
}
