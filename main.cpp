#include "asynclog.hpp"
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


char const* format(asynclog::output_buffer* pbuffer, char const* pformat, Object const& v)
{
    if(*pformat != 's')
        return nullptr;
    char const* fmt = "d";
    format(pbuffer, fmt, v.get());
    return pformat+1;
}

Object obj(3);

void thread_test(asynclog::log<>* log)
{
    log->write("three numbers: %s %d %d %s\n", 'B', 66, 3.0, obj);
}

int main()
{
    // FIXME need to be able to move log object
    asynclog::file_writer writer("dlog.txt");
    asynclog::log<> log(&writer, 64);
    std::thread thread(&thread_test, &log);
    //log.write("three numbers: %s %d %d %s\n", 'A', 66, 3.0, obj);
    thread.join();
    return 0;
}
