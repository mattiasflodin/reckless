#include "input.hpp"

int main()
{
    dlog::initialize();
    dlog::write("three numbers: %s %s %s", 'A', 66, 67L);
    return 0;
}
