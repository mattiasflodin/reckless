#include <cmath>
#include <cstdint>
#include <iostream>
#include <cassert>

#include <sstream>

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

/*double exp10(unsigned exponent)
{
    if(exponent == 0)
        return 1;
    double x = 1;
    for(unsigned i=0; i!=exponent; ++i)
        x *= 10;
    return x;
}*/

double iexp10(unsigned exponent)
{
    static double const lut1[] = {
        1e0,
        1e1,
        1e2,
        1e3,
        1e4,
        1e5,
        1e6,
        1e7,
    };
    static double const lut2[] = {
        1e32,
        1e64,
        1e96,
        1e128,
        1e160,
        1e192,
        1e224,
        1e256
    };

    if(exponent < 8) {
        return lut1[exponent];
    } else if(exponent % 32 == 0) {
        return lut2[exponent/32 - 1];
    } else if(exponent % 2 == 0) {
        double x = iexp10(exponent/2);
        return x*x;
    } else {
        double x = iexp10((exponent-1)/2);
        return 10.0*x*x;
    }
}

//double naive_ilogb(double v)
//{
//    asm("fxtract" : "=t
//}

// probably faster to do this with fxtract, gotta figure out how to do the
// inline asm for it.
int naive_ilogb(double v)
{
    std::uint16_t const* pv = reinterpret_cast<std::uint16_t const*>(&v);
    // Highest 16 bits is seeeeeee eeeemmmm. We want the e bit which is the
    // exponent.
    unsigned exponent = pv[3];
    exponent = (exponent >> 4) & 0x7ff;
    return static_cast<int>(exponent) - 1023;
}

std::uint64_t const sig_power_lut[] = {
    1,  // 0
    10,  // 1
    100,  // 2
    1000,  // 3
    10000,  // 4
    100000,  // 5
    1000000,  // 6
    10000000,  // 7
    100000000,  // 8
    1000000000,  // 9
    10000000000,  // 10
    100000000000,  // 11
    1000000000000,  // 12
    10000000000000,  // 13
    100000000000000,  // 14
    1000000000000000,  // 15
    10000000000000000,  // 16
    100000000000000000   // 17
};

// m*2^e / 10^300
// 10^300=2^300*5^300
// m*2^e/2^300 = m*2^(e-300)/5^300
// 
// log2(5^300) = 300 log(5)/log(2) = 696
// We can shift away the 696 aswell. Will get something in [1,2).
// Then we can adjust by log(5)/log(2) right? So...
// 
// 1.23456e300 / 1e300 = 1.23456e300/(2^300*5^300)
// With x = 1.23456e300/2^300:
// x/5^300 = log(2)/log(5) * x/2^696 *BEEP wrong*
// 
// On one side we have
// 1.23456*5^300 / 5^300
// On the other side we have
// 1.23456*5^300 / 2^696
// What factor do we need to add to the right side so that it will become equal
// to the left side?
// (1.23456*5^300 / 2^696) * 1/(5^300/2^696)
// (1.23456*5^300 / 2^696) * (2^696/5^300)
// 
// Not sure we need the split-up of 10^x into 2^x*5^x? What if we just try:
// 1.23456e300 / 2^996 * (2^996/10^300)
// We can compute 1.23456e300 / 2^996 exactly. Now the problem becomes how well
// we can compute the factor 2^996/10^300.
//
// 2^996/10^300 = 5^-300 * 2^(996 - 300)
// 

int descale2(double value, unsigned sig, std::uint64_t& ivalue)
{
    int exponent2 = naive_ilogb(value);
    int exponent10 = exponent2*301/1000;   // approximation of log(2)/log(10)
    double ref = iexp10(exponent10);
    
    double normalized = std::scalbn(value, -exponent2);
    normalized *= ref/std::scalbn(1.0, exponent2);
    return 0;
}

int descale(double value, unsigned sig, std::uint64_t& ivalue)
{
    assert(sig <= 17);
    assert(value >= 0.0);

    int exponent = naive_ilogb(value);
    exponent = exponent*30103/100000;   // approximation of log(2)/log(10)

    // 1.234 with sig = 4 needs to be multiplied by 1000 or 1*10^3 to get 1234.
    // In other words we need to subtract one from sig to get the factor.
    int rshift = exponent - (sig-1);

    double power;
    double descaled_value;
    //power = std::pow(10.0, -rshift);
    //descaled_value = value*power;
    if(rshift >= 0) {
        power = iexp10(static_cast<unsigned>(rshift));
        descaled_value = value/power;
    } else {
        power = iexp10(static_cast<unsigned>(-rshift));
        descaled_value = value*power;
    }

    std::uint64_t sig_power = sig_power_lut[sig];
    ivalue = static_cast<std::uint64_t>(descaled_value);
    if( ivalue < sig_power) {
        ivalue = std::lrint(descaled_value);
        return exponent;
    }

    // 123.45 -> 123
    // 123.45 -> 123.5 -> 124
    
    // TODO the /10 isn't good enough; we need proper rounding. Also can't just use static_cast above.

    std::uint64_t previous_ivalue = ivalue;
    int count = 0;
    while(ivalue >= sig_power) {
        ivalue /= 10;
        exponent += 1;
        ++count;
    }
    assert(count == 1);
    ivalue += (previous_ivalue % 10) >= 5;
    return exponent;
}

std::string ftoa(double value, unsigned sig)
{
    std::ostringstream ostr;
    std::uint64_t ivalue;
    int exponent = descale(value, sig, ivalue);
    if(exponent < 0) {
        ostr << "0.";
        for(int i=0; i!=-(exponent+1); ++i)
            ostr << '0';
        ostr << ivalue;
    } else if(static_cast<unsigned>(exponent+1) >= sig) {
        ostr << ivalue;
        for(int i=sig; i!=exponent+1; ++i)
            ostr << '0';
    } else {
        unsigned before_dot = static_cast<unsigned>(exponent)+1;
        unsigned after_dot = sig - before_dot;
        std::string s(sig + 1, '.');
        for(unsigned i=0; i!=after_dot; ++i) {
            s[sig - i] = '0' + static_cast<char>(ivalue % 10);
            ivalue /= 10;
        }
        s[before_dot] = '.';
        for(unsigned i=before_dot; i!=0; --i) {
            s[i-1] = '0' + static_cast<char>(ivalue % 10);
            ivalue /= 10;
        }
        ostr << s;
    }
    return ostr.str();
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
    std::uint64_t ivalue;
    descale2(1.23456789e300, 6, ivalue);
#if 0
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
        {12345678901234567890.0, 19, 123456},
        {1.23456789e300, 300, 123456}
    };
    for(auto test : tests) {
        std::uint64_t ivalue;
        int exponent = descale(test.value, 6, ivalue);
        if(exponent != test.exponent || ivalue != test.ivalue)
            std::cout << "! ";
        else
            std::cout << "  ";
        std::cout << test.value << '\t' << "-> " << exponent << ' ' << ivalue << std::endl;
    }

    assert( 3 == descale(0.0012345678, 6, iv) );
    descale(1.0, 6, iv);
    descale(10.0, 6, iv);
    descale(123.0, 6, iv);
    //foo(1234567800000000.0, 6, buf);
    //foo(0.01234567800000000, 6, buf);
    //foo(0.0012345678, 6);
#endif
#if 1
    static double const tests[] = {
        0.0012345678,
        1.0,
        10.0,
        123.0,
        123456,
        12345678,
        12345678901234567890.0,
        1.23456789e300,
        1.2345678901234567,
        1.2345678901234567e308,
        1.7976931348623157e308
    };
    for(double v : tests) {
        std::cout << v << '\t' << ftoa(v, 16) << std::endl;
    }
#endif

    return 0;
}
