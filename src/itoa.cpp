#include "itoa.hpp"

#include "asynclog/detail/utility.hpp"

#include <type_traits>  // is_unsigned
#include <cmath>        // log10
#include <cassert>
#include <cstring>      // memset
#include <iomanip>

namespace asynclog {
namespace detail {
namespace {

char const decimal_digits[] =
    "00010203040506070809"
    "10111213141516171819"
    "20212223242526272829"
    "30313233343536373839"
    "40414243444546474849"
    "50515253545556575859"
    "60616263646566676869"
    "70717273747576777879"
    "80818283848586878889"
    "90919293949596979899";

unsigned cache_line_size = get_cache_line_size();

inline void prefetch_digits()
{
    unsigned i = 0;
    unsigned const stride = cache_line_size;
    while(i < sizeof(decimal_digits)) {
        __builtin_prefetch(decimal_digits + i);
        i += stride;
    }
}

template <typename Unsigned>
typename std::enable_if<std::is_unsigned<Unsigned>::value, unsigned>::type
utoa_generic_base10(char* str, unsigned pos, Unsigned value)
{
    // FIXME is this right? What if value /= 100 yields 0 here? Do we get an
    // additional unnecessary 0?
    while(value >= 100) {
        Unsigned remainder = value % 100;
        value /= 100;
        std::size_t offset = 2*static_cast<std::size_t>(remainder);
        char d1 = decimal_digits[offset];
        char d2 = decimal_digits[offset+1];
        str[pos-1] = d2;
        str[pos-2] = d1;
        pos -= 2;
    }
    if(value < 10) {
        --pos;
        str[pos] = '0' + static_cast<unsigned char>(value);
    } else {
        std::size_t offset = 2*value;
        char d1 = decimal_digits[offset];
        char d2 = decimal_digits[offset+1];
        pos -= 2;
        str[pos+1] = d2;
        str[pos] = d1;
    }
    return pos;
}

template <typename Unsigned>
typename std::enable_if<std::is_unsigned<Unsigned>::value, Unsigned>::type
utoa_generic_base10(char* str, unsigned pos, Unsigned value, unsigned digits)
{
    while(digits >= 2) {
        Unsigned remainder = value % 100;
        value /= 100;
        std::size_t offset = 2*static_cast<std::size_t>(remainder);
        char d1 = decimal_digits[offset];
        char d2 = decimal_digits[offset+1];
        str[pos-1] = d2;
        str[pos-2] = d1;
        pos -= 2;
        digits -= 2;
    }
    if(digits == 1) {
        --pos;
        str[pos] = '0' + static_cast<unsigned char>(value % 10);
        value /= 10;
    }
    return value;
}

unsigned log10(std::uint32_t x)
{
    // If we partition evenly (i.e. on 6) we get:
    // 123456789A
    // 12345 6789A
    // 12 345 67 89A
    // 1 2 3 45 6 7 8 9A
    //       4 5      9 A
    //
    // But if we partition on 5 instead we get
    // 123456789A
    // 1234 56789A
    // 12 34 567 89A
    // 1 2 3 4 5 67 8 9A
    //           6 7  9 A
    //
    // And if we partition on 4, we get
    // 123456789A
    // 123 456789A
    // 1 23 456 789A
    //   2 3 4 56 78 9A
    //         5 6 7 8 9 A
    //
    // In all cases we get a maximum height of 5, but when we partition on 4 we
    // get a shorter depth for small numbers, which is probably more useful.

    if(x < 1000u) {
        // It's 1, 2 or 3.
        if(x < 10u) {
            // It's 1.
            return 1u;
        } else {
            // It's 2 or 3.
            if( x < 100u) {
                // It's 2.
                return 2u;
            } else {
                // It's 3.
                return 3u;
            }
        }
    } else {
        // It's 4, 5, 6, 7, 8, 9 or 10.
        if(x < 1000000u) {
            // It's 4, 5, or 6.
            if(x < 10000u) {
                // It's 4.
                return 4;
            } else {
                // It's 5 or 6.
                if(x < 100000u) {
                    // It's 5.
                    return 5;
                } else {
                    // It's 6.
                    return 6;
                }
            }
        } else {
            // It's 7, 8, 9 or 10.
            if(x < 100000000u) {
                // It's 7 or 8.
                if(x < 10000000u) {
                    // It's 7.
                    return 7;
                } else {
                    // It's 8.
                    return 8;
                }
            } else {
                // It's 9 or 10.
                if(x < 1000000000u) {
                    // It's 9.
                    return 9;
                } else {
                    // It's 10.
                    return 10;
                }
            }
        }
    }
}

// TODO probably faster to do this with fxtract, gotta figure out how to do the
// inline asm for it.
int naive_ilogb(double v)
{
    std::uint16_t const* pv = reinterpret_cast<std::uint16_t const*>(&v);
    // Highest 16 bits is seeeeeee eeeemmmm. We want the e part which is the
    // exponent.
    unsigned exponent = pv[3];
    exponent = (exponent >> 4) & 0x7ff;
    return static_cast<int>(exponent) - 1023;
}

double fxtract(double v, int* exp)
{
    double significand, exponent;
    asm("fxtract" : "=t"(significand), "=u"(exponent) : "0"(v));
    *exp = static_cast<int>(exponent);
    return significand;
}

long double fxtract(long double v, int* exp)
{
    long double significand, exponent;
    asm("fxtract" : "=t"(significand), "=u"(exponent) : "0"(v));
    *exp = static_cast<int>(exponent);
    return significand;
}

inline std::uint_fast64_t u64_rint(double value)
{
    if(std::is_convertible<std::uint64_t, long>::value)
        return static_cast<std::uint_fast64_t>(std::lrint(value));
    else
        return static_cast<std::uint_fast64_t>(std::llrint(value));
}

inline std::uint_fast64_t u64_rint(long double value)
{
    if(std::is_convertible<std::uint64_t, long>::value)
        return static_cast<std::uint_fast64_t>(std::lrint(value));
    else
        return static_cast<std::uint_fast64_t>(std::llrint(value));
}

std::uint64_t const power_lut[] = {
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

int descale_normal(long double value, int e2, unsigned significant_digits, std::uint_fast64_t& ivalue)
{
    static long double const C = 0.3010299956639811952137388947244L; // log(2)/log(10)
    int e10;
    // 1.0*2^-10
    // 1.9999*2^-10 ~= 2*2^-10 = 1.0*2^-9
    // Increasing x means increasing exponent.
    //
    // x=9.0256105364686323*10^-285
    // log2(x) = 943.58...
    // mantissa = 1.3421..., exponent = -944
    // Increasing mantissa eventually leads to higher value for e10.
    // e10 = -944*log(2)/log(10) = -284.17...
    // e10 = -945*log(2)/log(10) = -284.47...
    // We need to truncate -284.47 downwards, to 285. But cast to int truncates
    // upwards.
    e2 -= 1;
    if(e2 < 0)
        e10 = static_cast<int>(C*e2) - 1;
    else
        e10 = static_cast<int>(C*e2);
    int shift = -e10 + static_cast<int>(significant_digits - 1);
    long double descaled = value;
    long double factor = powl(10.0L, shift);
    descaled *= factor;

    ivalue = u64_rint(descaled);

    auto power = power_lut[significant_digits];
    while(ivalue >= power)
    {
        descaled /= 10.0;
        ivalue = u64_rint(descaled);
        e10 += 1;
    }
    assert(ivalue >= power/10 && ivalue < power);
    return static_cast<int>(e10);
}

int descale_subnormal(double value, unsigned significant_digits, std::uint_fast64_t& ivalue)
{
    std::uint64_t const* pv = reinterpret_cast<std::uint64_t const*>(&value);
    // seeeeeee eeeemmmm mmmmmmmm mmmmmmmm mmmmmmmm mmmmmmmm mmmmmmmm mmmmmmmm
    std::uint64_t m = *pv & 0xfffffffffffff;
    long double x = m;
    x *= (2.2250738585072013831L / 0x10000000000000); // 2^(1-1023)
    int e2;
    fxtract(x, &e2);
    return -308 + descale_normal(x, e2, significant_digits, ivalue);
    
}

int descale(double value, unsigned significant_digits, std::uint_fast64_t& ivalue)
{
    if(value == 0.0) {
        ivalue = 0;
        return 0;
    }

    auto e2 = naive_ilogb(value);
    if(e2 == -1023)
        return descale_subnormal(value, significant_digits, ivalue);
    else
        return descale_normal(value, e2, significant_digits, ivalue);
}

}   // anonymous namespace

// TODO define these for more integer sizes
void utoa_base10(output_buffer* pbuffer, unsigned int value)
{
    prefetch_digits();
    unsigned size = log10(value);
    char* str = pbuffer->reserve(size);
    utoa_generic_base10(str, size, value);
    pbuffer->commit(size);
}

void itoa_base10(output_buffer* pbuffer, int value)
{
    prefetch_digits();
    if(value<0) {
        unsigned int uv = static_cast<unsigned int>(-value);
        unsigned size = log10(uv) + 1;
        char* str = pbuffer->reserve(size);
        utoa_generic_base10(str, size, uv);
        pbuffer->commit(size);
        str[0] = '-';
    } else {
        utoa_base10(pbuffer, static_cast<unsigned int>(value));
    }
}

void ftoa_base10_precision(output_buffer* pbuffer, double value, unsigned precision, int minimum_exponent, int maximum_exponent)
{
    std::uint64_t ivalue;
    int exponent = descale(value, 17, ivalue);
    
}

void ftoa_base10(output_buffer* pbuffer, double value, unsigned significant_digits, int minimum_exponent, int maximum_exponent)
{
    if(std::signbit(value)) {
        value = -value;
        char* s = pbuffer->reserve(1);
        *s = '-';
        pbuffer->commit(1);
    }

    auto category = std::fpclassify(value);
    if(category == FP_NAN) {
        char* s = pbuffer->reserve(3);
        s[0] = 'n';
        s[1] = 'a';
        s[2] = 'n';
        pbuffer->commit(3);
        return;
    } else if(category == FP_INFINITE) {
        char* s = pbuffer->reserve(3);
        s[0] = 'i';
        s[1] = 'n';
        s[2] = 'f';
        pbuffer->commit(3);
        return;
    }

    if(category == FP_ZERO) {
        char* s = pbuffer->reserve(1);
        *s = '0';
        pbuffer->commit(1);
        return;
    }

    std::uint_fast64_t ivalue;
    int exponent = descale(value, significant_digits, ivalue);

    if(exponent < minimum_exponent || exponent > maximum_exponent) {
        char exponent_sign = '+';
        if(exponent < 0) {
            exponent_sign = '-';
            exponent = -exponent;
        }
        int exponent_digits;
        // Apparently stdio never prints less than two digits for the exponent,
        // so we'll do the same to stay consistent.
        if(exponent < 100)
            exponent_digits = 2;
        else
            exponent_digits = 3;

        int dot = significant_digits > 1;

        int size = significant_digits + dot + 2 + exponent_digits;
        char* s = pbuffer->reserve(size);
        utoa_generic_base10(s, size, static_cast<unsigned>(exponent), exponent_digits);
        s[size-exponent_digits-1] = exponent_sign;
        s[size-exponent_digits-2] = 'e';
        if(dot) {
            ivalue = utoa_generic_base10(s, size-exponent_digits-2, ivalue, significant_digits-1);
            s[1] = '.';
        }
        s[0] = '0' + static_cast<char>(ivalue);
        pbuffer->commit(size);
    } else if(exponent < 0) {
        unsigned zeroes = static_cast<unsigned>(-exponent - 1);
        unsigned size = 2 + zeroes + significant_digits;
        char* str = pbuffer->reserve(size);
        utoa_generic_base10(str, size, ivalue);
        std::memset(str+2, '0', zeroes);
        str[1] = '.';
        str[0] = '0';
        pbuffer->commit(size);
    } else if(static_cast<unsigned>(exponent+1) >= significant_digits) {
        unsigned zeroes = exponent+1 - significant_digits;
        unsigned size = zeroes + significant_digits;
        char* str = pbuffer->reserve(size);
        std::memset(str + significant_digits, '0', zeroes);
        utoa_generic_base10(str, significant_digits, ivalue);
        pbuffer->commit(size);
    } else {
        unsigned before_dot = static_cast<unsigned>(exponent)+1;
        unsigned after_dot = significant_digits - before_dot;
        unsigned size = before_dot + 1 + after_dot;
        char* str = pbuffer->reserve(size);
        ivalue = utoa_generic_base10(str, size, ivalue, after_dot);
        str[before_dot] = '.';
        utoa_generic_base10(str, before_dot, ivalue);
        pbuffer->commit(size);
    }
}

}   // namespace detail
}   // namespace asynclog

#ifdef UNIT_TEST
#include "unit_test.hpp"

#include <string>
#include <sstream>  // istringstream, ostringstream
#include <iomanip>  // iomanip
#include <random>
#include <algorithm>    // mismatch

namespace asynclog {
namespace detail {

class string_writer : public writer {
public:
    Result write(void const* pbuffer, std::size_t count) override
    {
        auto pc = static_cast<char const*>(pbuffer);
        buffer_.insert(buffer_.end(), pc, pc + count);
        return SUCCESS;
    }

    void reset()
    {
        buffer_.clear();
    }

    std::string const& str() const
    {
        return buffer_;
    }

private:
    std::string buffer_;
};

std::size_t const PERFECT_QUALITY = std::numeric_limits<std::size_t>::max();

#define TEST_FTOA(number) test_conversion_quality(number, __FILE__, __LINE__)

class ftoa
{
public:
    ftoa() :
        output_buffer_(&writer_, 1024)
    {
    }

    void greater_than_one()
    {
        TEST_FTOA(1.0);
        TEST_FTOA(10.0);
        TEST_FTOA(123.0);
        TEST_FTOA(123.456);
        TEST_FTOA(123456);
        TEST_FTOA(12345678901234567890.0);
        TEST_FTOA(1.23456789e300);
        TEST_FTOA(1.2345678901234567e308);
        TEST_FTOA(1.7976931348623157e308);
        TEST_FTOA(1.7976931348623158e308);
    }

    void subnormals()
    {
        TEST_FTOA(2.2250738585072009e-308);
        TEST_FTOA(0.04e-308);
        TEST_FTOA(0.00001234e-308);
        TEST_FTOA(4.9406564584124654e-324);
        TEST_FTOA(0.0);
    }

    void special()
    {
        TEST(convert(std::numeric_limits<double>::quiet_NaN()) == "nan");
        TEST(convert(std::numeric_limits<double>::signaling_NaN()) == "nan");
        TEST(convert(std::nan("1")) == "nan");
        TEST(convert(std::nan("2")) == "nan");
        TEST(convert(-std::numeric_limits<double>::quiet_NaN()) == "-nan");
        TEST(convert(std::numeric_limits<double>::infinity()) == "inf");
        TEST(convert(-std::numeric_limits<double>::infinity()) == "-inf");
    }

    void scientific()
    {
        std::printf("%f\n", 1.2345e-8);
        std::printf("%f\n", 1.2345e-9);
        std::printf("%f\n", 1.2345e-10);
        std::printf("%f\n", 1.2345e-11);
        std::printf("%f\n", 1.2345e300);
        std::printf("%g\n", 1.2345e-8);
        std::printf("%g\n", 1.2345e-9);
        std::printf("%g\n", 1.2345e-10);
        std::printf("%g\n", 1.2345e-11);
        std::printf("%g\n", 1.2345e300);
        TEST(convert(1.2345e-8, -4, 5, 6) == "1.23450e-08");
        TEST(convert(1.2345e-10, -4, 5, 6) == "1.23450e-10");
        TEST(convert(1.2345e+10, -4, 5, 6) == "1.23450e+10");
        TEST(convert(1.2345e+10, -4, 5, 1) == "1e+10");
        TEST(convert(1.6345e+10, -4, 5, 1) == "2e+10");
        TEST(convert(1.6645e+10, -4, 5, 2) == "1.7e+10");
        TEST(convert(1.6645e+10, -4, 5, 2) == "1.7e+10");
        TEST(convert(1.7976931348623157e308, -4, 5, 5) == "1.7977e+308");
        TEST(convert(4.9406564584124654e-324, -4, 5, 5) == "4.9407e-324");
    }

    void random()
    {
        std::mt19937_64 rng;
        int const total = 10000000;
        int perfect = 0;
        int quality_counts[17] = {0};
        int i = 0;
        while(i != total) {
            std::uint64_t bits = rng();
            bits &= (std::uint64_t(1)<<63)-1;
            int exponent = static_cast<unsigned>(bits >> 52);
            if(exponent == 0 || exponent == 2047)
                continue;
            double const v = *reinterpret_cast<double const*>(&bits);

            auto quality = get_conversion_quality(v);
            if(quality == PERFECT_QUALITY) {
                perfect++;
            } else {
                TEST(quality < 17);
                quality_counts[quality]++;
            }
            ++i;
        }
        auto start = std::find_if(quality_counts, quality_counts+17, [](std::size_t x) {return x != 0;});
        while(start != quality_counts+17)
        {
            std::cout << (start - quality_counts) << " digits: " << *start << std::endl;
            ++start;
        }
        std::cout << "perfect conversion: " << perfect;
        std::cout << " (" << 100*static_cast<double>(perfect)/total << "%)";
        std::cout << std::endl;
    }

private:
    std::size_t get_conversion_quality(double number)
    {
        std::string const& str = convert(number);
        std::istringstream istr(str);
        double number2;
        istr >> number2;
        if(number != number2)
        {
            std::ostringstream ostr;
            ostr << std::setprecision(17) << std::fixed << number;
            std::string const& iostream_str = ostr.str();
            auto it = mismatch(iostream_str.begin(), iostream_str.end(), str.begin()).first;
            std::size_t correct_digits = it - iostream_str.begin();
            return correct_digits;
        }
        return PERFECT_QUALITY;
    }

    std::string convert(double number, int minimum_exponent=-1000, int maximum_exponent=1000, int significant_digits=17)
    {
        writer_.reset();
        ftoa_base10(&output_buffer_, number, significant_digits, minimum_exponent, maximum_exponent);
        output_buffer_.flush();
        return writer_.str();
    }

    void test_conversion_quality(double number, char const* file, int line)
    {
        if(get_conversion_quality(number) != PERFECT_QUALITY)
        {
            std::ostringstream ostr;
            ostr << "conversion of " << number;
            throw unit_test::error(ostr.str(), file, line);
        }
    }

    string_writer writer_;
    output_buffer output_buffer_;
};

unit_test::suite<ftoa> tests = {
    //TESTCASE(ftoa::greater_than_one),
    //TESTCASE(ftoa::subnormals)
    //TESTCASE(ftoa::special)
    TESTCASE(ftoa::scientific)
    //TESTCASE(ftoa::random)
};

}   // namespace detail
}   // namespace asynclog

UNIT_TEST_MAIN();
#endif
