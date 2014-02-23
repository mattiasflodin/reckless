#include "input.hpp"
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

std::ostream& operator<<(std::ostream& os, Object const& obj)
{
    return (os << obj.get());
}

Object obj(3);

struct formatter
{
    template <typename... Args>
    static void internal_format(std::ostringstream& ostr)
    {
    }
    template <typename T, typename... Args>
    static void internal_format(std::ostringstream& ostr, T&& first, Args&&... rest)
    {
        ostr << std::forward<T>(first) << ' ';
        internal_format(ostr, std::forward<Args>(rest)...);
    }

    template <typename... Args>
    static void format(char* pbuffer, Args&&... rest)
    {
        std::ostringstream ostr;
        internal_format(ostr, std::forward<Args>(rest)...);
        std::strcpy(pbuffer, ostr.str().c_str());
    }
};

typedef dlog::logger<formatter> logger;

int main()
{
    dlog::initialize();
    logger::write("three numbers: %s %s %s", 'A', 66, 67L, obj);
    return 0;
}
