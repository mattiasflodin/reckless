#include <cmath>
#include <cstdint>
#include <iostream>
#include <cassert>

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

double exp10(unsigned exponent)
{
    if(exponent == 0)
        return 1;
    double x = 1;
    for(unsigned i=0; i!=exponent; ++i)
        x *= 10;
    return x;
}

int descale(double value, unsigned sig, std::uint64_t& ivalue)
{
    assert(value >= 0.0);

    int exponent = std::ilogb(value);
    exponent = exponent/3 - 1;

    // 1.234 with sig = 4 needs to be multiplied by 1000 or 1*10^3 to get 1234.
    // In other words we need to subtract one from sig to get the factor.
    int rshift = exponent - (sig-1);

    double power;
    double descaled_value;
    if(rshift >= 0) {
        power = exp10(static_cast<unsigned>(rshift));
        descaled_value = value/power;
    } else {
        power = exp10(static_cast<unsigned>(-rshift));
        descaled_value = value*power;
    }

    double p2;
    double v2;
    if(exponent >= 0) {
        p2 = exp10(static_cast<unsigned>(exponent));
        v2 = value/p2;
    } else {
        p2 = exp10(static_cast<unsigned>(-exponent));
        v2 = value*p2;
    }

    unsigned sig_power = exp10(sig);
    ivalue = static_cast<std::uint64_t>(descaled_value);
    while(ivalue >= sig_power) {
        ivalue /= 10;
        exponent += 1;
    }
    return exponent;
}

//int magnitude(double value)
//{
//    std::cout << value << '\t';
//    int exponent = std::ilogb(value);
//    exponent /= 3;
//    std::cout << exponent << '\t';
//    double nvalue = exponent >= 0? value*exp10(exponent) : value/exp10(-exponent);
//    std::cout << nvalue << '\t';
//    if(nvalue < 1.0) {
//        std::cout << 'S';
//        nvalue *= 10;
//        exponent -= 1;
//    } else if(nvalue >= 10.0) {
//        nvalue /= 10;
//        std::cout << 'S';
//        exponent += 1;
//    } else {
//        std::cout << ' ';
//    }
//
//    std::cout << '\t' << nvalue;
//    double vvalue = value*pow(10, -exponent);
//    std::cout << '\t' << " -> " << exponent << ' ' << vvalue << std::endl;
//    return exponent;
//}

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
    assert(-0.0 < 0.0);
    std::uint64_t iv;
    struct testspec {
        double value;
        int exponent;
        std::uintptr_t ivalue;
    };
        
    testspec const tests[] = {
        {0.0012345678, -3, 123456},
        {1.0, 0, 100000},
        {10.0, 1, 100000},
        {123.0, 2, 123000},
        {123456, 5, 123456},
        {12345678, 7, 123456},
        {12345678901234567890.0, 19, 123456}
    };
    for(auto test : tests) {
        std::uint64_t ivalue;
        int exponent = descale(test.value, 6, ivalue);
        if(exponent != test.exponent || ivalue != test.ivalue)
            std::cout << '!';
        else
            std::cout << ' ';
        std::cout << '\t' << test.value << '\t' << "-> " << exponent << ' ' << ivalue << std::endl;
    }

    assert( 3 == descale(0.0012345678, 6, iv) );
    descale(1.0, 6, iv);
    descale(10.0, 6, iv);
    descale(123.0, 6, iv);
    //foo(1234567800000000.0, 6, buf);
    //foo(0.01234567800000000, 6, buf);
    //foo(0.0012345678, 6);
    return 0;
}
