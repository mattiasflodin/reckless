#include <cmath>
#include <cstdint>
#include <iostream>

// Situations:
// 
//   0.zzzzxxxxx
//   power = #z
//   sig = #x
//   size = 2+power+digits
//   value < 1.0
//
//   xxxxxzzzz
//   power = #z + #x
//   sig = #x
//   size = power
//   power >= sig
//   value >= 1.0
//   
//   xxxx.yyyy
//   power = #x
//   sig = #x + #y
//   size = 1 + sig
//   sig > power
//   value >= 1.0

// Returns x such that value * pow(10, -x) is in the range [1.0, 10.0)
unsigned ipow(unsigned exponent)
{
    if(exponent == 0)
        return 1;
    unsigned x = 1;
    for(unsigned i=0; i!=exponent; ++i)
        x *= 10;
    return x;
}
int magnitude(double value)
{
    std::cout << value << '\t';
    int exponent = std::ilogb(value);
    exponent /= 3;
    std::cout << exponent << '\t';
    double nvalue = exponent >= 0? value*ipow(exponent) : value/ipow(-exponent);
    std::cout << nvalue << '\t';
    if(nvalue < 1.0) {
        std::cout << 'S';
        nvalue *= 10;
        exponent -= 1;
    } else if(nvalue >= 10.0) {
        nvalue /= 10;
        std::cout << 'S';
        exponent += 1;
    } else {
        std::cout << ' ';
    }

    std::cout << '\t' << nvalue;
    double vvalue = value*pow(10, -exponent);
    std::cout << '\t' << " -> " << exponent << ' ' << vvalue << std::endl;
    return exponent;
}

void foo_fraction(double value, unsigned sig)
{
    double log10 = std::log10(value);
    double power = std::floor(log10);
    int ipower = static_cast<int>(power);
    return;
}

void foo(double value, unsigned sig)
{
    if(value<1.0)
        foo_fraction(value, sig);
}

void foo_old(double value, unsigned significant_digits, char* str)
{
    // double has 53 bits of mantissa, i.e. 53 significant digits. In decimal,
    // that translates to 53*log(2)/log(10) = 15.95 significant digits.
    // Paraphrasing Kahan's "Lecture notes on the status of IEEE Standard 754
    // for Binary Floating-Point Arithmetic":
    //   If a Double Precision floating-point number is converted to a decimal
    //   string with at least 17 significant decimals and then converted back
    //   to Double, then the final number must match the original.
    // 
    // We need to verify whether 17 is actually required for some reason I
    // don't understand, but for now we'll try with 16 digits.
    
    //unsigned p = significant_digits;
    double log10 = std::log10(value);

    double power;
    if(log10<0)
        power = std::floor(log10);
    else
        power = std::ceil(log10);

    int ipower = static_cast<int>(power);
    
    double roundoff_factor = std::pow(10, significant_digits);
    double nvalue = value*roundoff_factor*std::pow(10, -power);
    static_assert(sizeof(long)>=8, "long is too small");
    std::uint_fast64_t ivalue = std::lrint(nvalue);

    std::cout << ivalue;
    for(int i=significant_digits; i!=ipower; ++i)
    {
        std::cout << '0';
    }
    std::cout << std::endl;
    return;
}

int main()
{
    char buf[20];
    magnitude(8);
    magnitude(9);
    magnitude(12);
    magnitude(15.999999999);
    magnitude(16);
    magnitude(0.0010000000);
    magnitude(0.0012345678);
    magnitude(0.0050000000);
    magnitude(0.0099999999);
    magnitude(0.0099999999999999999999999999999);
    magnitude(0.0000000001);
    magnitude(0.0000000005);
    magnitude(0.0000000009);
    //foo(1234567800000000.0, 6, buf);
    //foo(0.01234567800000000, 6, buf);
    //foo(0.0012345678, 6);
    return 0;
}
