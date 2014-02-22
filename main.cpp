#include "input.hpp"

char p1;
int p2;
long p3;
int main()
{
    dlog::initialize();
    dlog::write("three numbers: %s %s %s", p1, p2, p3);
    return 0;
}
