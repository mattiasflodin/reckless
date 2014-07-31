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

template <typename T>
typename std::make_unsigned<T>::type unsigned_cast(T v)
{
    return static_cast<typename std::make_unsigned<T>::type>(v);
}

template <typename T>
typename std::make_signed<T>::type signed_cast(T v)
{
    return static_cast<typename std::make_signed<T>::type>(v);
}

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
        str[pos] = '0' + unsigned_cast(value);
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
        str[pos] = '0' + unsigned_cast(value % 10);
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

long double fxtract(long double v, long double* exp)
{
    long double significand;
    asm("fxtract" : "=t"(significand), "=u"(*exp) : "0"(v));
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

std::int64_t binary64_to_decimal18(double input, int* pexponent)
{
    long double e2;
    long double m2 = fxtract(static_cast<long double>(input), &e2);

    // We have
    //   input = m2 * 2^e2  [1 <= m2 < 2] (1)
    // and want a new representation
    //   input = m10 * 10^e10  [1 <= m10 < 10].
    //   
    // (1) can be rewritten as
    //   m2 * 10^(C*e2)  [C = log(2)/log(10)] =
    //   m2 * 10^(e10i + e10f)  [e10i = trunc(C*e2), e10f=frac(C*e2)] =
    //   m2 * 10^e10i * 10^e10f.
    //   
    // If m2 * 10^e10f turns out to be in the range [1, 10) then this gives us
    // a value for m10 (and consequently for e10=C*e2i). But assuming e2f is a
    // positive fraction, we have
    //   0 <= e2f < 1
    //   1 <= 10^e2f < 10.
    //   1 <= m2 * 10^e2f < 20.
    //   
    // We can adjust for m2 * 10^e2f >= 10 by shifting it to the right and
    // increasing e10 by one. Hence, with D = e2*log(2)/log(10) we have
    //   m10 = m2 * 10^(frac(D))
    //   e10 = trunc(D).
    // when m2*10^(frac(D)) < 10, or
    //   m10 = m2 * 10^(frac(D)) / 10
    //   e10 = trunc(D) + 1.
    // when m2*10^(frac(D)) >= 10, i.e. normalization requires at most a shift
    // to the right by 1 digit.
    // 
    // If we e2f is a negative fraction we get
    //   -1 < e2f <= 0
    //   0.1 <= 10^e2f <= 1.
    //   0.1 <= m2 * 10^e2f < 2.
    // Note that this is the range obtained for a positive exponent except
    // divided by 10. If we multiply this by 10 and subtract 1 from the
    // exponent then we get the same situation as above, i.e.
    //   1 <= 10 * m2 * 10^e2f < 20.

    long double d = e2;
    d *=  0.30102999566398119521L;
    long double e10i;
    long double e10f = std::modf(d, &e10i);
    long double m10;
    if(e2 < 0) {
        e10f += 1;
        e10i -= 1;
    }
    //m10 = m2*powl(10, e10f + 17);
    //std::int64_t mantissa = std::llrint(m10);
    //if(mantissa >= 1000000000000000000) {
    //    m10 /= 10.0L;
    //    mantissa = std::llrint(m10);
    //    e10i += 1.0L;
    //}
    m10 = m2*powl(10, e10f + 16);
    std::int64_t mantissa = std::llrint(m10);
    if(mantissa < 100000000000000000)
        mantissa *= 10;
    else
        e10i += 1.0L;
    // FIXME rounding of m10 might cause it to overflow, right?. We need to
    // adjust again in that case. But try to reproduce this scenario first so
    // we know it's needed.

    *pexponent = static_cast<int>(e10i);
    return mantissa;
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
    100000000000000000,  // 17
    1000000000000000000   // 20
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
        unsigned uv = unsigned_cast(-value);
        unsigned size = log10(uv) + 1;
        char* str = pbuffer->reserve(size);
        utoa_generic_base10(str, size, uv);
        pbuffer->commit(size);
        str[0] = '-';
    } else {
        utoa_base10(pbuffer, unsigned_cast(value));
    }
}

void ftoa_base10_precision(output_buffer* pbuffer, double value, unsigned precision)
{
    int exponent;
    auto mantissa_signed = binary64_to_decimal18(value, &exponent);
    std::uint64_t mantissa;
    if(mantissa_signed>=0) {
        mantissa = unsigned_cast(mantissa_signed);
    } else {
        char* s = pbuffer->reserve(1);
        *s = '-';
        pbuffer->commit(1);
        mantissa = unsigned_cast(-mantissa_signed);
    }

    unsigned mantissa_digits_before_dot = 0;
    unsigned zeroes_before_dot = 0;
    unsigned mantissa_digits_after_dot = 0;
    unsigned zeroes_before_mantissa = 0;
    unsigned zeroes_after_mantissa = 0;

    if(exponent >= 0) {
        // There are exponent+1 digits before the dot.
        unsigned digits_before_dot = unsigned_cast(exponent) + 1;
        mantissa_digits_before_dot = std::max(18, digits_before_dot);
        zeroes_before_dot = digits_before_dot - mantissa_digits_before_dot;
        mantissa_digits_after_dot = std::max(precision, 18-mantissa_digits_before_dot);
        zeroes_after_mantissa = precision - mantissa_digits_after_dot;
    } else {
        zeroes_before_mantissa = std::max(precision, unsigned_cast(-exponent) - 1);
        // No digits of the mantissa are on the left hand side of the dot.
        // 0.0000mmmm000
        //   [precision]
        //   [-(exponent+1)] [18 digits + zeroes | significant digits]
        int a = -(exponent+1);
        int zeroes_after_mantissa = precision - a - 18;
        if(precision >= a+18) {
            // The entire mantissa is visible.
        } else {
            if(precision > a) {
                int significant_digits = precision - a;
            } else {

            }
            int significant_digits = precision - a;
        }

        int significant_digits = std::min(18, signed_cast(precision) - signed_cast(zeroes_before_mantissa));
        if(significant_digits<0) {
            // The precision is low enough that we will get only zeroes.
        } else {
            // The precision is high enough that we will see the mantissa.
            if(precision > significant_digits) {
                mantissa_digits_after_dot = significant_digits;
                zeroes_after_mantissa = precision - significant_digits;
            } else {
                mantissa_digits_after_dot = precision;
            }
        }
    }
}

void ftoa_base10_precision2(output_buffer* pbuffer, double value, unsigned precision)
{
    int exponent;
    auto mantissa_signed = binary64_to_decimal18(value, &exponent);
    std::uint64_t mantissa;
    if(mantissa_signed>=0) {
        mantissa = unsigned_cast(mantissa_signed);
    } else {
        char* s = pbuffer->reserve(1);
        *s = '-';
        pbuffer->commit(1);
        mantissa = unsigned_cast(-mantissa_signed);
    }
    if(exponent >= 0) {
        // There are exponent+1 digits before the dot.
        unsigned digits_before_dot = unsigned_cast(exponent) + 1;
        if(digits_before_dot >= 18) {
            // Number is large enough that no digits from the mantissa will be
            // to the right of the dot, so we just fill out the right-hand
            // side with zeroes.
            unsigned zeroes_before_dot = digits_before_dot - 18;
            unsigned zeroes_after_dot = precision;
            unsigned size = digits_before_dot;
            if(precision != 0)
                size += 1 + zeroes_after_dot;
            char* s = pbuffer->reserve(size);
            unsigned pos = size;
            for(unsigned i=0;i!=zeroes_after_dot;++i)
                s[pos-i-1] = '0';
            pos -= zeroes_after_dot;
            if(precision != 0) {
                s[--pos] = '.';
                for(unsigned i=0;i!=zeroes_before_dot;++i)
                    s[pos-i-1] = '0';
            }
            pos -= zeroes_before_dot;
            utoa_generic_base10(s, pos, mantissa);
            pbuffer->commit(size);
        } else {
            // Some digits of the mantissa are on the right side of the dot.
            unsigned mantissa_digits_after_dot = 18-digits_before_dot;
            unsigned zeroes_after_dot;
            unsigned significant_digits_after_dot;
            if(precision > mantissa_digits_after_dot) {
                // Precision is high enough that there will be trailing zeroes
                // that are not part of the mantissa's significant digits. Use
                // the entire mantissa and fill out with zeroes after.
                zeroes_after_dot = precision - mantissa_digits_after_dot;
                significant_digits_after_dot = mantissa_digits_after_dot;
            } else {
                // Precision is so low that some digits from the mantissa will
                // be cut off. We need to divide (and round).
                zeroes_after_dot = 0;
                significant_digits_after_dot = precision;
                auto power = power_lut[mantissa_digits_after_dot - precision];
                // FIXME can mantissa overflow due to rounding and cause too many
                // digits here?
                mantissa = (mantissa + power/2)/power;
            }

            unsigned size = digits_before_dot;
            if(precision != 0) {
                size += + 1 + precision;
                char* s = pbuffer->reserve(size);
                unsigned pos = size;
                for(unsigned i=0;i!=zeroes_after_dot;++i)
                    s[pos-i-1] = '0';
                pos -= zeroes_after_dot;
                mantissa = utoa_generic_base10(s, pos, mantissa, significant_digits_after_dot);
                pos -= significant_digits_after_dot;
                s[--pos] = '.';
                utoa_generic_base10(s, pos, mantissa);
                pbuffer->commit(size);
            } else {
                char* s = pbuffer->reserve(size);
                utoa_generic_base10(s, size, mantissa);
                pbuffer->commit(size);
            }
        }
    } else {
        // exponent < 0. No digits of the mantissa are on the left hand side of
        // the dot.
        unsigned zeroes_before_mantissa = std::max(precision, unsigned_cast(-exponent) - 1);
        int significant_digits = std::min(18, signed_cast(precision) - signed_cast(zeroes_before_mantissa));
        if(significant_digits<0) {
            // The precision is low enough that we will get only zeroes.
            unsigned size = precision + 2;
            char* s = pbuffer->reserve(size);
            unsigned pos = size;
            for(unsigned i=0;i!=zeroes_before_mantissa;++i)
                s[pos-i-1] = '0';
            s[1] = '.';
            s[0] = '0';
            pbuffer->commit(size);
        } else {
            // The precision is high enough that we will see the mantissa.
            int zeroes_after_mantissa = precision - significant_digits;
            if(zeroes_after_mantissa>0) {
                unsigned size = unsigned_cast(2 + significant_digits + zeroes_after_mantissa);
                unsigned pos = size;
                char* s = pbuffer->reserve(pos);
                for(int i=0; i!=zeroes_after_mantissa;++i)
                    s[pos-i-1] = '0';
                pos -= zeroes_after_mantissa;
                utoa_generic_base10(s, pos, mantissa);
                pos -= 18;
                s[1] = '.';
                s[0] = '0';
                pbuffer->commit(size);
            } else {
                unsigned size = unsigned_cast(2 + significant_digits);
                char* s = pbuffer->reserve(size);
                auto power = power_lut[18 - significant_digits];
                // FIXME can mantissa overflow due to rounding and cause too many
                // digits here?
                mantissa = (mantissa + power/2)/power;
                utoa_generic_base10(s, size, mantissa);
                s[1] = '.';
                s[0] = '0';
                pbuffer->commit(size);
            }
        }
    }
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
        utoa_generic_base10(s, size, unsigned_cast(exponent), exponent_digits);
        s[size-exponent_digits-1] = exponent_sign;
        s[size-exponent_digits-2] = 'e';
        if(dot) {
            ivalue = utoa_generic_base10(s, size-exponent_digits-2, ivalue, significant_digits-1);
            s[1] = '.';
        }
        s[0] = '0' + static_cast<char>(ivalue);
        pbuffer->commit(size);
    } else if(exponent < 0) {
        unsigned zeroes = unsigned_cast(-exponent - 1);
        unsigned size = 2 + zeroes + significant_digits;
        char* str = pbuffer->reserve(size);
        utoa_generic_base10(str, size, ivalue);
        std::memset(str+2, '0', zeroes);
        str[1] = '.';
        str[0] = '0';
        pbuffer->commit(size);
    } else if(unsigned_cast(exponent+1) >= significant_digits) {
        unsigned zeroes = exponent+1 - significant_digits;
        unsigned size = zeroes + significant_digits;
        char* str = pbuffer->reserve(size);
        std::memset(str + significant_digits, '0', zeroes);
        utoa_generic_base10(str, significant_digits, ivalue);
        pbuffer->commit(size);
    } else {
        unsigned before_dot = unsigned_cast(exponent)+1;
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

    //void decimal()
    //{
    //    int exponent;
    //    std::int64_t mantissa = binary64_to_decimal18(3.0, &exponent);
    //    mantissa = binary64_to_decimal18(6.0, &exponent);
    //    mantissa = binary64_to_decimal18(0.75, &exponent);
    //}

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

    void precision()
    {
        TEST(convert_prec(1.5, 2) == "1.50");
        TEST(convert_prec(1.234567890, 4) == "1.2346");
        TEST(convert_prec(1.2345678901234567, 16) == "1.2345678901234567");
        TEST(convert_prec(1.2345678901234567, 17) == "1.23456789012345670");
        TEST(convert_prec(1.2345678901234567, 25) == "1.2345678901234567000000000");
        TEST(convert_prec(1.7976931348623157e308, 3) == "179769313486231563000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000.000");
        TEST(convert_prec(1234.5678, 0) == "1235");
        TEST(convert_prec(1234.5678, 1) == "1234.6");

        TEST(convert_prec(0.3, 1) == "0.3");
        TEST(convert_prec(0.3, 2) == "0.30");
        TEST(convert_prec(0.3, 20) == "0.29999999999999999000");

        TEST(convert_prec(1.2345e20, 5) == "123450000000000000000.00000");
        TEST(convert_prec(1.2345e20, 0) == "123450000000000000000");
        TEST(convert_prec(1.2345e2, 5) == "123.45000");
        TEST(convert_prec(1.2345e-20, 5) == "0.00000");
        TEST(convert_prec(0.5, 0) == "1");
        TEST(convert_prec(0.05, 1) == "0.1");
        TEST(convert_prec(1.23456789012345670, 20) == "1.23456789012345670000");
        TEST(convert_prec(0.123456789012345670, 20) == "0.12345678901234567000");
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
            unsigned exponent = static_cast<unsigned>(bits >> 52);
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

    std::string convert_prec(double number, unsigned precision)
    {
        writer_.reset();
        ftoa_base10_precision(&output_buffer_, number, precision);
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
    //TESTCASE(ftoa::decimal)
    TESTCASE(ftoa::precision)
    //TESTCASE(ftoa::greater_than_one),
    //TESTCASE(ftoa::subnormals)
    //TESTCASE(ftoa::special)
    //TESTCASE(ftoa::scientific)
    //TESTCASE(ftoa::random)
};

}   // namespace detail
}   // namespace asynclog

UNIT_TEST_MAIN();
#endif
