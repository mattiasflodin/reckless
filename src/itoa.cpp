#include "itoa.hpp"

#include "asynclog/detail/utility.hpp"

#include <type_traits>  // is_unsigned
#include <cmath>        // log10
#include <cassert>
#include <cstring>      // memset

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
        str[pos] = '0' + static_cast<unsigned char>(value);
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

inline std::uint_fast64_t u64_rint(double value)
{
    if(std::is_convertible<std::uint64_t, long>::value)
        return static_cast<std::uint_fast64_t>(std::lrint(value));
    else
        return static_cast<std::uint_fast64_t>(std::llrint(value));
}

int descale_pow5(double value, unsigned significant_digits, std::uint_fast64_t& ivalue)
{
    static std::uint64_t const sig_power_lut[] = {
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
    auto e2 = naive_ilogb(value);
    static double const C = 0.301029995663981195; // log(2)/log(10)
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
    double descaled = std::scalbn(value, shift);
    descaled *= std::pow(5.0, shift);
    ivalue = u64_rint(descaled);

    auto power = sig_power_lut[significant_digits];
    while(ivalue >= power)
    {
        descaled /= 10.0;
        ivalue = u64_rint(descaled);
        e10 += 1;
    }
    assert(ivalue >= power/10 && ivalue < power);
    return static_cast<int>(e10);
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

void ftoa_base10_natural(output_buffer* pbuffer, double value, unsigned significant_digits)
{
    std::uint_fast64_t ivalue;
    int exponent = descale_pow5(value, significant_digits, ivalue);

    if(exponent < 0) {
        unsigned zeroes = static_cast<unsigned>(-exponent + 1);
        unsigned size = 2 + zeroes + significant_digits;
        char* str = pbuffer->reserve(size);
        utoa_generic_base10(str, size, ivalue);
        std::memset(str+2, '0', zeroes);
        str[1] = '.';
        str[0] = '0';
        pbuffer->commit(size);
    } else if(static_cast<unsigned>(exponent+1) >= significant_digits) {
        unsigned zeroes = exponent+1;
        unsigned size = zeroes + significant_digits;
        char* str = pbuffer->reserve(size);
        utoa_generic_base10(str, size, ivalue);
        std::memset(str, '0', zeroes);
        pbuffer->commit(size);
    } else {
        unsigned before_dot = static_cast<unsigned>(exponent)+1;
        unsigned after_dot = significant_digits - before_dot;
        unsigned size = before_dot + 1 + after_dot;
        char* str = pbuffer->reserve(size);
        ivalue = utoa_generic_base10(str, size, ivalue, after_dot);
        str[before_dot] = '.';
        utoa_generic_base10(str, before_dot, ivalue);
    }
}

}   // namespace detail
}   // namespace asynclog

#ifdef UNIT_TEST

#define TEST(name) {&name, ##name}

namespace unit_test {

class test_suite;
void register_test_suite(test_suite* ptest_suite)
{
}

struct no_context;

template <typename Context>
class test {
public:
    test(void (Context::*ptest_function)()) :
        ptest_function_(pf)
    {
    }
    void operator()(Context& ctx)
    {
        (ctx.*ptest_function_)();
    }

private:
    void (Context::*ptest_function)();
};

template <>
class test<no_context> {
public:
    test(void (*ptest_function)()) :
        ptest_function_(pf)
    {
    }
    void operator()(no_context&)
    {
        (*ptest_function_)();
    }

private:
    void (Context::*ptest_function)();
};

template <typename Context>
class test_suite {
public:
    test_suite(std::initializer_list<test<Context>> tests) :
        tests_(tests),
        succeeded_(0)
    {
    }

    void operator()()
    {
        Context c;
        for(test& t : tests_) {
            try {
                t(c);
            } catch(std::exception&) {
                continue;
            }
            ++succeeded_;
        }
    }
};

}   // namespace unit_test

#define TEST_SUITE(name)
extern Test test_suite ## name [];
Test test_suite ## name [] =



template <typename Context, std::size_t Number>
void test(Context& context);

template <std::size_t Number>
void test();

template <>
void test<0>()
{
}

#endif
