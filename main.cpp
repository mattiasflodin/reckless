#include "input.hpp"
#include <iostream>

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

int main()
{
    dlog::initialize();
    dlog::write("three numbers: %s %s %s", 'A', 66, 67L, obj);
    return 0;
}
