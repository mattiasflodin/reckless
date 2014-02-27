#include "dlog.hpp"
#include <iostream>
#include <sstream>
#include <cstring>

class Object {
public:
    Object(int x_) : x(x_) {}
    Object(Object const& rhs) : x(-rhs.x)
    {
        std::cout << "Object::Object(Object const&)" << std::endl;
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


bool format(dlog::output_buffer* pbuffer, char const*& pformat, Object const& v);
//{
//    if(*pformat != 's')
//        return false;
//    ++pformat;
//    char const* fmt = "d";
//    return format(pbuffer, fmt, v.get());
//}

Object obj(3);

typedef dlog::logger<dlog::formatter> logger;

int main()
{
    dlog::file_writer writer("dlog.txt");
    dlog::initialize(&writer);
    logger::write("three numbers: %s %s %s", 'A', 66, 67L, obj);
    return 0;
}
