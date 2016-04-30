#include "memory_writer.hpp"
#include <reckless/policy_log.hpp>

#include <vector>
#include <cassert>
#include <stdexcept>
#include <exception>    // rethrow_exception

#include <iostream>

memory_writer<std::string> g_writer;
reckless::policy_log<> g_log;

struct Object {
    Object() : copy_count_(0)
    {
    }
    Object(Object const& rhs)
    {
        copy_count_ = rhs.copy_count_ + 1;
        if(copy_count_ == 2)
            throw std::runtime_error("runtime error");
    }

    unsigned copy_count_;
};

char const* format(reckless::output_buffer* poutput, char const* fmt, Object)
{
    if(*fmt != 's')
        return nullptr;
    poutput->write('X');
    return fmt+1;
}

void format_error(reckless::output_buffer* poutput, std::exception_ptr const& pexception, std::type_info const&)
{
    poutput->write("<exception caught when calling formatter: ");
    try {
        std::rethrow_exception(pexception);
    } catch(std::exception const& e) {
        poutput->write(e.what());
    } catch(...) {
    }
    poutput->write(">\n");
}

int main()
{
    g_log.open(&g_writer);
    g_log.write("Hello");
    Object object;
    g_log.write("%s", object);
    g_log.write("World!");
    g_log.close();
    std::cout << g_writer.container;
    assert(g_writer.container == "Hello\nWorld!\n");
    
    g_writer.container.clear();
    g_log.open(&g_writer);
    g_log.format_error_callback(format_error);
    g_log.write("Hello");
    g_log.write("%s", object);
    g_log.write("World!");
    g_log.close();
    std::cout << std::endl << g_writer.container;
    assert(g_writer.container ==
        "Hello\n"
        "<exception caught when calling formatter: runtime error>\n"
        "World!\n");
    return 0;
}
