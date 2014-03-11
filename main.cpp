#include "dlog.hpp"
#include <iostream>
#include <sstream>
#include <cstring>

#include <unistd.h>

class Object {
public:
    Object(int x_) : x(x_) {}
    Object(Object const& rhs) : x(-rhs.x)
    {
        //std::cout << "Object::Object(Object const&)" << std::endl;
    }
    ~Object() {}
    Object& operator=(Object const& rhs)
    {
        x = rhs.x;
        return *this;
    }

    int get() const { return x; }

    //{
    //    return (os << x);
    //}
private:
    int x;
};


bool format(dlog::output_buffer* pbuffer, char const*& pformat, Object const& v)
{
    if(*pformat != 's')
        return false;
    ++pformat;
    char const* fmt = "d";
    return format(pbuffer, fmt, v.get());
}

Object obj(3);

typedef dlog::logger<dlog::formatter> logger;

int main()
{
    dlog::file_writer writer("dlog.txt");
    dlog::initialize(&writer);
    for(std::size_t i=0; i!=10000; ++i) {
        logger::write("three numbers: %s %d %d %s\n", 'A', 66, i, obj);
        //dlog::flush();
        //usleep(1000);
    }
    dlog::cleanup();
    return 0;
}
