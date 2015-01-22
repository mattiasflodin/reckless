#include "ntoa.hpp"

#include "asynclog/detail/utility.hpp"

#include <type_traits>  // is_unsigned
#include <cassert>
#include <cstring>      // memset
#include <cmath>        // lrint, llrint
#include <iomanip>

namespace asynclog {
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


void prefetch_digits()
{
    detail::prefetch(decimal_digits, sizeof(decimal_digits));
}

template <typename Unsigned>
typename std::enable_if<std::is_unsigned<Unsigned>::value, unsigned>::type
utoa_generic_base10_preallocated(char* str, unsigned pos, Unsigned value)
{
    if(value == 0)
        return pos;
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
utoa_generic_base10_preallocated(char* str, unsigned pos, Unsigned value, unsigned digits)
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

template <bool Uppercase, typename Unsigned>
typename std::enable_if<std::is_unsigned<Unsigned>::value, unsigned>::type
utoa_generic_base16_preallocated(char* str, unsigned pos, Unsigned value)
{
    char const digit_offset = '0';
    char const hexdigit_offset = (Uppercase? 'A' : 'a') - 10;
    while(value) {
        --pos;
        char v = value % 0x10;
        value /= 0x10;
        str[pos] = v + (v<10? digit_offset : hexdigit_offset);
    }
    return pos;   
}


unsigned log2(std::uint32_t v)
{
    // From Sean Eron Anderson's "Bit Twiddling Hacks" collection, except for
    // the +1 at the end. Our bastardized logarithms all return the number of
    // digits required in a string, not the "real" logarithm.
    unsigned r;
    unsigned shift;

    r =     (v > 0xFFFF) << 4; v >>= r;
    shift = (v > 0xFF  ) << 3; v >>= shift; r |= shift;
    shift = (v > 0xF   ) << 2; v >>= shift; r |= shift;
    shift = (v > 0x3   ) << 1; v >>= shift; r |= shift;
                                            r |= (v >> 1);
    return r+1;
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
            // It's 0 or 1.
            return x != 0;
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

template <typename Unsigned>
typename std::enable_if<std::is_unsigned<Unsigned>::value, Unsigned>::type
log16(Unsigned v)
{
    return (log2(v)+3)/4;
}

template <typename Unsigned>
void itoa_generic_base10(output_buffer* pbuffer, bool negative, Unsigned value, conversion_specification const& cs)
{
    prefetch_digits();
    char sign;
    if(negative) {
        sign = true;
    } else {
        sign = cs.plus_sign != 0;
    }
    
    // We have either
    // [sign] [zeroes] [digits] [padding]
    //        [---precision---]
    // [--------------size--------------]
    // or
    // [padding] [sign] [zeroes] [digits]
    //                  [---precision---]
    // [--------------size--------------]
    // depending on if it's left-justified or not.
    unsigned digits = log10(value);
    unsigned precision = (cs.precision == UNSPECIFIED_PRECISION? 1 : cs.precision);
    precision = std::max(digits, precision);
    unsigned size = std::max(sign + precision, cs.minimum_field_width);
    unsigned zeroes = precision - digits;
    unsigned padding = size - sign - precision;
    
    char* str = pbuffer->reserve(size);
    
    if(cs.left_justify) {
        if(sign)
            str[0] = negative? '-' : cs.plus_sign;
        std::memset(str+sign, '0', zeroes);
        utoa_generic_base10_preallocated(str, sign+precision, value);
        std::memset(str+sign+precision, ' ', padding);
    } else {
        std::memset(str, ' ', padding);
        if(sign)
            str[padding] = negative? '-' : cs.plus_sign;
        std::memset(str+padding+sign, '0', zeroes);
        utoa_generic_base10_preallocated(str, size, value);
    }
    pbuffer->commit(size);
}

template <typename Integer>
typename std::enable_if<std::is_signed<Integer>::value, void>::type
itoa_generic_base10(output_buffer* pbuffer, Integer value, conversion_specification const& cs)
{
    bool negative = value < 0;
    unsigned uv;
    if(negative)
        uv = unsigned_cast(-value);
    else
        uv = unsigned_cast(value);
    itoa_generic_base10(pbuffer, negative, uv, cs);
}

template <typename Integer>
typename std::enable_if<std::is_unsigned<Integer>::value, void>::type
itoa_generic_base10(output_buffer* pbuffer, Integer value, conversion_specification const& cs)
{
    itoa_generic_base10(pbuffer, false, value, cs);
}

template <typename Unsigned>
typename std::enable_if<std::is_unsigned<Unsigned>::value, void>::type
itoa_generic_base16(output_buffer* pbuffer, Unsigned value, bool uppercase, char const* prefix)
{
    if(prefix) {
        std::size_t len = std::strlen(prefix);
        char* str = pbuffer->reserve(len);
        std::memcpy(str, prefix, len);
        pbuffer->commit(len);
    }
            
    value = 0x186a200;
    unsigned size = log16(value);
    char* str = pbuffer->reserve(size);
    if(uppercase)
        utoa_generic_base16_preallocated<true>(str, size, value);
    else
        utoa_generic_base16_preallocated<false>(str, size, value);
    pbuffer->commit(size);
}

template <typename Signed>
typename std::enable_if<std::is_signed<Signed>::value, void>::type
itoa_generic_base16(output_buffer* pbuffer, Signed value, bool uppercase, char const* prefix)
{
    if(value<0) {
        std::size_t prefix_length = std::strlen(prefix);
        auto uv = unsigned_cast(-value);
        unsigned size = 1 + prefix_length + log16(uv);
        char* str = pbuffer->reserve(size);
        if(uppercase)
            utoa_generic_base16_preallocated<true>(str, size, uv);
        else
            utoa_generic_base16_preallocated<false>(str, size, uv);
        memcpy(str + 1, prefix, prefix_length);
        str[0] = '-';
        pbuffer->commit(size);
    } else {
        itoa_generic_base16(pbuffer, unsigned_cast(value), uppercase, prefix);
    }
}

// TODO this should probably be removed in favor of fxtract. When we replace
// descale with binary64_to_decimal18 we won't need naive_ilogb anymore.
int naive_ilogb(double v)
{
    std::uint16_t const* pv = reinterpret_cast<std::uint16_t const*>(&v);
    // Highest 16 bits is seeeeeee eeeemmmm. We want the e part which is the
    // exponent.
    unsigned exponent = pv[3];
    exponent = (exponent >> 4) & 0x7ff;
    return static_cast<int>(exponent) - 1023;
}

template <typename Float>
Float fxtract(Float v, Float* exp)
{
    Float significand;
    asm("fxtract" : "=t"(significand), "=u"(*exp) : "0"(v));
    return significand;
}

template <typename Float>
Float fxtract(Float v, int* exp)
{
    Float exponent;
    Float significand = fxtract(v, &exponent);
    *exp = static_cast<int>(exponent);
    return significand;
}

template <typename Float>
inline std::uint_fast64_t u64_rint(Float value)
{
    if(std::is_convertible<std::uint64_t, long>::value)
        return static_cast<std::uint_fast64_t>(std::lrint(value));
    else
        return static_cast<std::uint_fast64_t>(std::llrint(value));
}

struct decimal18
{
    bool sign;
    std::uint64_t mantissa;
    int exponent;
};

// TODO some day we should probably use this everywhere instead of descale().
// It is simpler and more rigorously documented.
decimal18 binary64_to_decimal18(double input)
{
    //if(input == 0.0) {
    //    *pexponent = 0;
    //    return 0;
    //}
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
    // We can adjust for m2 * 10^e2f >= 10 by dividing m2 by 10 and increasing
    // e10 by one. Hence, with D = e2*log(2)/log(10) we have
    //   m10 = m2 * 10^(frac(D))
    //   e10 = trunc(D).
    // when m2*10^(frac(D)) < 10, or
    //   m10 = m2 * 10^(frac(D)) / 10
    //   e10 = trunc(D) + 1.
    // when m2*10^(frac(D)) >= 10, i.e. normalization requires at most a shift
    // to the right by 1 digit.
    // 
    // If e2f is a negative fraction we get
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

    long double p = powl(10, e10f + 16);
    (void) p;
    m10 = m2*powl(10, e10f + 16);
    std::uint64_t mantissa = static_cast<std::uint64_t>(std::llrint(std::abs(m10)));
    if(mantissa < 100000000000000000u)
        mantissa *= 10;
    else
        e10i += 1.0L;
    // TODO rounding of m10 might cause it to overflow, right?. We need to
    // adjust again in that case. But try to reproduce this scenario first so
    // we know it's needed.

    return {std::signbit(value), mantissa, static_cast<int>(e10i)};
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
    1000000000000000000   // 18
};

std::uint64_t rounded_divide(bool sign, std::uint64_t value, std::uint64_t divisor)
{
    if(!sign) {
        // +divisor/2 to turn truncation into rounding.
        return (value + divisor/2) / divisor;
    } else {
        // This is the same rounding as above but we must pretend that the
        // value is negative and reproduce the corresponding behavior.
        // Adding divisor-1 turns the flooring behavior of integer division
        // into a ceiling behavior.
        return (value + (divisor-1) - divisor/2) / divisor;
    }
}
std::uint64_t rounded_rshift(bool sign, std::uint64_t value, unsigned digits)
{
    return rounded_divide(sign, value, power_lut[digits]);
}


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

unsigned zero_digits(char* s, unsigned pos, unsigned count)
{
    unsigned start = pos-count;
    while(count != 0)
    {
        --count;
        s[start+count] = '0';
    }
    return start;
}

}   // anonymous namespace

// TODO define these for more integer sizes
void itoa_base10(output_buffer* pbuffer, int value, conversion_specification const& cs)
{
    itoa_generic_base10(pbuffer, value, cs);
}

void itoa_base10(output_buffer* pbuffer, unsigned int value, conversion_specification const& cs)
{
    itoa_generic_base10(pbuffer, value, cs);
}

void itoa_base10(output_buffer* pbuffer, long value, conversion_specification const& cs)
{
    itoa_generic_base10(pbuffer, value, cs);
}

void itoa_base10(output_buffer* pbuffer, unsigned long value, conversion_specification const& cs)
{
    itoa_generic_base10(pbuffer, value, cs);
}

void itoa_base10(output_buffer* pbuffer, long long value, conversion_specification const& cs)
{
    itoa_generic_base10(pbuffer, value, cs);
}

void itoa_base10(output_buffer* pbuffer, unsigned long long value, conversion_specification const& cs)
{
    itoa_generic_base10(pbuffer, value, cs);
}

void itoa_base16(output_buffer* pbuffer, unsigned long value, bool uppercase, char const* prefix)
{
    itoa_generic_base16(pbuffer, value, uppercase, prefix);
}

void itoa_base16(output_buffer* pbuffer, long value, bool uppercase, char const* prefix)
{
    itoa_generic_base16(pbuffer, value, uppercase, prefix);
}

void ftoa_base10_exponent(output_buffer* pbuffer, std::int64_t mantissa_signed,
    int exponent_signed, conversion_specification const& cs)
{
    // We have either
    // [sign] [digit] [dot] [digits_after_dot] [e] [exponent sign] [exponent_digits] [padding]
    // or
    // [padding] [sign] [zero_padding] [digit] [dot] [digits_after_dot] [e] [exponent sign] [exponent_digits]
    // depending on if it's left-justified or not.
    
    bool sign;
    std::uint64_t mantissa;
    if(mantissa_signed<0) {
        mantissa = unsigned_cast(-mantissa_signed);
        sign = true;
    } else {
        mantissa = unsigned_cast(mantissa_signed);
        sign = cs.plus_sign != 0;
    }

    char exponent_sign;
    unsigned exponent;
    if(exponent_signed < 0) {
        exponent_sign = '-';
        exponent = unsigned_cast(-exponent_signed);
    } else {
        exponent_sign = '+';
        exponent = unsigned_cast(exponent_signed);
    }
    unsigned digits_after_dot = cs.precision - 1;
    int dot = digits_after_dot!=0 || cs.alternative_form;
    
    int exponent_digits;
    // Apparently stdio never prints less than two digits for the exponent,
    // so we'll do the same to stay consistent. Maximum value of the exponent
    // in a double is 308 so we won't get more than 3 digits.
    if(exponent < 100)
        exponent_digits = 2;
    else
        exponent_digits = 3;

    unsigned content_size = sign + 1 + dot + digits_after_dot + 1 + 1 + exponent_digits;
    unsigned size = std::max(cs.minimum_field_width, content_size);
    unsigned padding = size - content_size;
    unsigned pad_zeroes = 0;
    if(cs.pad_with_zeroes) {
        pad_zeroes = padding;
        padding = 0;
    }
    
    // Get rid of digits we don't want in the mantissa.
    mantissa = rounded_rshift(mantissa_signed<0, mantissa, 18-cs.precision);
    
    char* str = pbuffer->reserve(size);

    unsigned pos = size;
    if(cs.left_justify) {
        pos -= padding;
        std::memset(str+pos, ' ', padding);
    }

    utoa_generic_base10_preallocated(str, pos, exponent, exponent_digits);
    pos -= exponent_digits;
    str[--pos] = exponent_sign;
    str[--pos] = 'e';

    mantissa = utoa_generic_base10_preallocated(str, pos, mantissa, digits_after_dot);
    pos -= digits_after_dot;
    
    if(dot)
        str[--pos] = '.';

    str[--pos] = '0' + static_cast<char>(mantissa);

    pos -= pad_zeroes;
    std::memset(str+pos, '0', pad_zeroes);
    if(sign)
        str[0] = mantissa_signed<0? '-' : cs.plus_sign;
    
    if(!cs.left_justify) {
        pos -= padding;
        std::memset(str+pos, ' ', padding);
    }
    pbuffer->commit(size);
}

unsigned count_trailing_zeroes_after_dot(bool sign, std::uint64_t mantissa, int exponent)
{
    // We need to get rid of the least significant digit because a double can't
    // always represent a zero digit at that position.
    // (e.g. 123.456 becomes 123.456000000000003).
    mantissa = rounded_divide(sign, mantissa, 10);
    unsigned digits_before_dot = unsigned_cast(std::max(0, exponent+1));
    if(digits_before_dot>17) {
        // All significant digits are in front of the dots. No trailing zeroes
        return 0;
    }
    unsigned max_trailing_zeroes = unsigned_cast(17 - digits_before_dot);
    unsigned low = 0;
    unsigned high = max_trailing_zeroes+1;
    // Find the lowest x such that mantissa % 10^x != 0.
    // Then the number of trailing zeroes is x-1.
    while(low < high) {
        unsigned pos = (low + high)/2;
        if(mantissa % power_lut[pos] == 0)
            low = pos + 1;
        else
            high = pos;
    }

    // +1 because we always treat the least significant digit (that we stripped
    // at function entry) as if it were zero.
    return low-1 + 1;
}

void write_special_category(output_buffer* pbuffer, double value, conversion_specification const& cs, char const* category)
{
    char sign = std::signbit(value)? '-' : cs.plus_sign;
    unsigned content_size = 3 + (sign? 1 : 0);
    unsigned size = std::max(content_size, cs.minimum_field_width);
    unsigned padding = size - content_size;
    char* str = pbuffer->reserve(size);
    unsigned pos = 0;
    if(cs.left_justify) {
        if(sign)
            str[pos++] = sign;
        memcpy(str+pos, category, 3);
        pos += 3;
        std::memset(str+pos, ' ', padding);
    } else {
        std::memset(str, ' ', padding);
        pos += padding;
        if(sign)
            str[pos++] = sign;
        memcpy(str+pos, category, 3);
    }
    pbuffer->commit(size);
}

void write_nan(output_buffer* pbuffer, double value, conversion_specification const& cs)
{
    return write_special_category(pbuffer, value, cs, "nan");
}

void write_inf(output_buffer* pbuffer, double value, conversion_specification const& cs)
{
    return write_special_category(pbuffer, value, cs, "inf");
}

// Corresponds to %g.
// The idea of %g is that the precision says how many significant digits we
// want in the string representation. If the number of significant digits is
// not enough to represent all the digits up to the dot (i.e. it is higher than
// the number's exponent), then %e notation is used instead.
// 
// If there are trailing zeroes in the fraction then those will be removed,
// unless alternative mode is requested. Alternative mode also means that the
// period stays, no matter if there are any fractional decimals remaining or
// not.
void ftoa_base10(output_buffer* pbuffer, double value, conversion_specification const& cs)
{
    // We have either
    // [sign] [digits_before_dot] [zeroes_before_dot] [dot] [zeroes_after_dot] [digits_after_dot] [padding]
    // or
    // [padding] [sign] [pad_zeroes] [digits_before_dot] [zeroes_before_dot] [dot] [zeroes_after_dot] [digits_after_dot]
    // depending on if it's left-justified or not.
    // 
    // zeroes_before_dot is used for filler zeroes when the number's magnitude
    // is larger than the number of available digits in the mantissa.
    // zeroes_after_dot is used when the number's magnitude is less than -1
    // (such as for 0.01). If zeroes_before_dot is nonzero then both
    // zeroes_after_dot and digits_after_dot are zero. Conversely, if
    // zeroes_after_dot is nonzero then digits_before_dot and zeroes_before_dot
    // will both be zero.
    //
    // If value == 0 then binary64_to_decimal18 generates an infinite exponent,
    // and utoa_generic_base10_preallocated will not generate any digits at all
    // for a zero mantissa (it considers this a state of "nothing more to
    // output"). So we can't use the normal path for zeroes. Instead, we encode
    // the zero digit in the [zeroes_before_dot] component .
    
    std::uint64_t mantissa;
    bool negative = std::signbit(value);
    unsigned digits_before_dot;
    unsigned zeroes_before_dot;
    bool dot;
    unsigned zeroes_after_dot;
    unsigned digits_after_dot;
    bool sign = (negative || cs.plus_sign != 0);

    auto category = std::fpclassify(value);
    if(category == FP_NAN) {
        return write_nan(pbuffer, value, cs);
    } else if(category == FP_INFINITE) {
        return write_inf(pbuffer, value, cs);
    } else {
        int const minimum_exponent = -4;
        int maximum_exponent = cs.precision;
        decimal18 dv = binary64_to_decimal18(value, &exponent);

        if(dv.exponent < minimum_exponent || dv.exponent > maximum_exponent)
            return ftoa_base10_exponent(pbuffer, mantissa_signed, exponent, cs);
        else
            return ftoa_base10_f_normal(pbuffer, signbit, mantissa, 
        

        if(mantissa_signed<0) {
            mantissa = unsigned_cast(-mantissa_signed);
            negative = true;
        } else {
            mantissa = unsigned_cast(mantissa_signed);
            negative = false;
        }
        
        if(exponent>=17) {
            digits_before_dot = 18;
            zeroes_before_dot = unsigned_cast(exponent)+1 - 18;
            zeroes_after_dot = 0;
        } else if(exponent < -1) {
            digits_before_dot = 0;
            zeroes_before_dot = 1;
            zeroes_after_dot = unsigned_cast(-exponent)-1;
        } else {
            digits_before_dot = unsigned_cast(exponent+1);
            // If there are no mantissa digits before the dot, put a filler
            // zero there so we get "0.1" instead of ".1"
            zeroes_before_dot = digits_before_dot == 0? 1 : 0;
            zeroes_after_dot = 0;
        }

        if(cs.alternative_form) {
            digits_after_dot = cs.precision - digits_before_dot;
            dot = true;
            // TODO
        } else {
            // requested_digits_after_dot is always >= 0 since we delegate to
            // ftoa_base10_exponent otherwise.
            unsigned requested_digits_after_dot = cs.precision - digits_before_dot;
            unsigned trailing_zeroes = count_trailing_zeroes_after_dot(mantissa_signed<0, mantissa, exponent);
            unsigned available_digits_after_dot = 18u - digits_before_dot - trailing_zeroes;
            digits_after_dot = std::min(available_digits_after_dot, requested_digits_after_dot);
            dot = digits_after_dot != 0;
        }
    }
    
    unsigned content_size = sign + digits_before_dot + zeroes_before_dot + dot + zeroes_after_dot + digits_after_dot;
    unsigned size = std::max(cs.minimum_field_width, content_size);

    unsigned padding = size - content_size;
    unsigned pad_zeroes = 0;
    if(cs.pad_with_zeroes) {
        pad_zeroes = padding;
        padding = 0;
    }
    
    // Get rid of digits we don't want in the mantissa.
    unsigned significant_digits = digits_before_dot + digits_after_dot;
    mantissa = rounded_rshift(negative, mantissa, 18-significant_digits);
    
    char* str = pbuffer->reserve(size);
    
    auto pos = size;
    if(cs.left_justify) {
        pos -= padding;
        std::memset(str+pos, ' ', padding);
    }
    if(dot) {
        mantissa = utoa_generic_base10_preallocated(str, pos, mantissa,
                digits_after_dot);
        pos -= digits_after_dot;
        
        pos -= zeroes_after_dot;
        std::memset(str+pos, '0', zeroes_after_dot);
        str[--pos] = '.';
    }
    pos -= zeroes_before_dot;
    std::memset(str+pos, '0', zeroes_before_dot);
    pos = utoa_generic_base10_preallocated(str, pos, mantissa);
    pos -= pad_zeroes;
    std::memset(str+pos, '0', pad_zeroes);
    if(sign)
        str[--pos] = negative? '-' : cs.plus_sign;

    if(!cs.left_justify) {
        pos -= padding;
        std::memset(str+pos, ' ', padding);
    }
    pbuffer->commit(size);
    assert(pos == 0);
}

// NEXT the point of this is to get rid of the lengthy implementation of '%g'
// and simply have one for %e and one of %f. This replaces most of the _prec
// variant.
void ftoa_base10_f_normal(output_buffer* pbuffer, bool signbit, std::uint64_t mantissa,
    int exponent, int precision, conversion_specification const& cs)
{
    // [sign] [digits_before_dot] [zeroes_before_dot] [dot] [zeroes_after_dot] [digits_after_dot] [suffix_zeroes]
    unsigned digits_before_dot;
    unsigned zeroes_before_dot;
    unsigned zeroes_after_dot;
    unsigned digits_after_dot;
    
    if(exponent>=17) {
        // All digits from the mantissa are on the left-hand side of the dot.
        digits_before_dot = 18;
        zeroes_before_dot = unsigned_cast(exponent)+1 - 18;
        zeroes_after_dot = precision;
        digits_after_dot = 0;
    } else if(exponent < 0) {
        // All digits from the mantissa are on the right-hand side of the dot.
        digits_before_dot = 0;
        zeroes_before_dot = 1;
        zeroes_after_dot = std::min(unsigned_cast(-exponent)-1, precision);
        digits_after_dot = precision - zeroes_after_dot;
    } else {
        // There are digits from the mantissa both on the left- and right-hand
        // sides of the dot.
        digits_before_dot = unsigned_cast(exponent+1);
        zeroes_before_dot = 0;
        zeroes_after_dot = 0;
        digits_after_dot = std::min(18 - digits_before_dot, precision);
    }
    unsigned suffix_zeroes = precision - (zeroes_after_dot + digits_after_dot);
    
    bool dot = cs.alternative_form || (zeroes_after_dot != 0);

    // Reduce the mantissa part to the requested number of digits.
    auto mantissa_digits = digits_before_dot + digits_after_dot;
    mantissa = rounded_rshift(signbit, mantissa, 18-mantissa_digits);
    auto order = power_lut[mantissa_digits];
    bool carry = mantissa >= order;
    if(carry) {
        // When reducing the mantissa, rounding caused it to overflow. For
        // example, when formatting 0.095 with precision=2, we have
        // zeroes_before_dot=1, zeroes_after_dot=1, mantissa_digits=1 and
        // suffix_zeroes=0. When trying to reduce the mantissa to 1 digit we
        // end up not with 9, but with 10 due to rounding. In this case we need
        // to make room for an extra digit from the mantissa, which we'll take
        // from the prefix zeroes or by moving the dot forward.
        if(zeroes_after_dot) {
            --zeroes_after_dot;
            ++digits_after_dot;
        } else {
            ++digits_before_dot;
        }
        ++mantissa_digits;
    }

    unsigned content_size = signbit + digits_before_dot + zeroes_before_dot
        + dot + zeroes_after_dot + digits_after_dot + suffix_zeroes;
    unsigned size = std::max(cs.minimum_field_width, content_size);
    unsigned padding = size - content_size;
    if(cs.pad_with_zeroes) {
        pad_zeroes = padding;
        padding = 0;
    }

    char* str = pbuffer->reserve(size);
    unsigned pos = size;
    if(cs.left_justify) {
        pos -= padding;
        std::memset(str+pos, ' ', padding);
    }

    if(dot) {
        mantissa = utoa_generic_base10_preallocated(str, pos, mantissa,
                digits_after_dot);
        pos -= digits_after_dot;
        
        pos -= zeroes_after_dot;
        std::memset(str+pos, '0', zeroes_after_dot);
        str[--pos] = '.';
    }

    pos -= zeroes_before_dot;
    std::memset(str+pos, '0', zeroes_before_dot);
    pos = utoa_generic_base10_preallocated(str, pos, mantissa);
    pos -= pad_zeroes;
    std::memset(str+pos, '0', pad_zeroes);
    if(signbit)
        str[--pos] = negative? '-' : cs.plus_sign;

    if(!cs.left_justify) {
        pos -= padding;
        std::memset(str+pos, ' ', padding);
    }
    pbuffer->commit(size);
    assert(pos == 0);
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
    
    // TODO we should have a symbolic constant for 18 but so far I can't think
    // of a name that doesn't make the code more confusing (for the record,
    // it's how many digits there are in the mantissa produced by
    // binary64_to_decimal18). DECIMAL_MANTISSA_DIGITS is my best name so far,
    // but in the same code block we deal with the count of requested mantissa
    // digits in the output string representation, and it just becomes easier
    // to confuse the two.

    // Disregarding the dot, any decimal number can be represented as 
    // [prefix zeroes] [0-18 mantissa digits] [suffix zeroes]
    // To avoid many different control paths that are difficult to verify and
    // understand, we'll try to quantify each part first and the position of
    // the dot. Then we can use these to fairly easily perform the conversion.
    unsigned prefix_zeroes;
    unsigned mantissa_digits;
    unsigned suffix_zeroes;
    unsigned dot_position;
    if(exponent+1 > 0) {
        // exponent+1 digits before the dot.
        dot_position = unsigned_cast(exponent+1);
        unsigned total_digits = dot_position + precision;
        prefix_zeroes = 0;
        mantissa_digits = std::min(18u, total_digits);
        suffix_zeroes = total_digits - mantissa_digits;
    } else {
        // No digits before the dot. -(exponent+1) zeroes before the first
        // significant digit.
        prefix_zeroes = std::min(precision, unsigned_cast(-(exponent+1)));
        mantissa_digits = std::min(18u, precision - prefix_zeroes);
        suffix_zeroes = precision - prefix_zeroes - mantissa_digits;
        prefix_zeroes += 1;  // For the additional zero before the dot.
        dot_position = 1;
    }
    
    // Reduce the mantissa part to the requested number of digits.
    mantissa = rounded_rshift(mantissa_signed<0, mantissa, 18-mantissa_digits);
    auto order = power_lut[mantissa_digits];
    bool carry = mantissa >= order;
    if(carry) {
        // When reducing the mantissa, rounding caused it to overflow.
        // For example, when formatting 0.095 with precision=2, we have
        // prefix_zeroes=2, mantissa_digits=1 and suffix_zeroes=0. When trying
        // to reduce the mantissa to 1 digit we end up not with 9, but with 10
        // due to rounding. In this case we need to make room for an extra
        // digit from the mantissa, which we'll take from the prefix zeroes or
        // by moving the dot forward.
        mantissa_digits += 1;
        if(prefix_zeroes>0)
            prefix_zeroes -= 1;
        else
            dot_position += 1;
    }

    unsigned size = prefix_zeroes + mantissa_digits + suffix_zeroes;
    unsigned digits_left_to_dot = size - dot_position;
    if(dot_position != size)
        size += 1;

    char* s = pbuffer->reserve(size);
    unsigned pos = size;

    auto fill_zeroes = [&](unsigned zeroes)
    {
        if(digits_left_to_dot <= zeroes && digits_left_to_dot != 0) {
            pos = zero_digits(s, pos, digits_left_to_dot);
            s[--pos] = '.';
            pos = zero_digits(s, pos, zeroes - digits_left_to_dot);
            digits_left_to_dot = 0;
        } else {
            pos = zero_digits(s, pos, zeroes);
            digits_left_to_dot -= zeroes;
        }
    };
    
    fill_zeroes(suffix_zeroes);

    if(digits_left_to_dot != 0 && digits_left_to_dot <= mantissa_digits) {
        mantissa = utoa_generic_base10_preallocated(s, pos, mantissa, digits_left_to_dot);
        pos -= digits_left_to_dot;
        s[--pos] = '.';
        if(mantissa_digits != digits_left_to_dot)
            pos = utoa_generic_base10_preallocated(s, pos, mantissa);
        digits_left_to_dot = 0;
    } else if(mantissa_digits != 0) {
        pos = utoa_generic_base10_preallocated(s, pos, mantissa);
        digits_left_to_dot -= mantissa_digits;
    }

    fill_zeroes(prefix_zeroes);

    assert(pos == 0);
    pbuffer->commit(size);
}

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

class itoa_base10_suite
{
public:
    itoa_base10_suite() :
        output_buffer_(&writer_, 1024)
    {
    }

    void positive()
    {
        TEST(convert(0) == "0");
        TEST(convert(1) == "1");
        TEST(convert(2) == "2");
        TEST(convert(10) == "10");
        TEST(convert(100) == "100");
        TEST(convert(std::numeric_limits<int>::max()) == std::to_string(std::numeric_limits<int>::max()));
    }

    void negative()
    {
        TEST(convert(-1) == "-1");
        TEST(convert(-2) == "-2");
        TEST(convert(-10) == "-10");
        TEST(convert(-100) == "-100");
        TEST(convert(std::numeric_limits<int>::min()) == std::to_string(std::numeric_limits<int>::min()));
    }

    void field_width()
    {
        conversion_specification cs;
        cs.left_justify = false;
        cs.alternative_form = false;
        cs.pad_with_zeroes = false;
        cs.plus_sign = 0;
        cs.minimum_field_width = 11;
        cs.precision = UNSPECIFIED_PRECISION;
        
        TEST(convert(0, cs) == "          0");
        TEST(convert(1, cs) == "          1");
        TEST(convert(2, cs) == "          2");
        TEST(convert(10, cs) == "         10");
        TEST(convert(100, cs) == "        100");
        
        TEST(convert(-1, cs) == "         -1");
        TEST(convert(-2, cs) == "         -2");
        TEST(convert(-10, cs) == "        -10");
        TEST(convert(-100, cs) == "       -100");
    }

    void left_justify()
    {
        conversion_specification cs;
        cs.left_justify = true;
        cs.alternative_form = false;
        cs.pad_with_zeroes = false;
        cs.plus_sign = 0;
        cs.minimum_field_width = 11;
        cs.precision = UNSPECIFIED_PRECISION;
        
        TEST(convert(0, cs) == "0          ");
        TEST(convert(1, cs) == "1          ");
        TEST(convert(2, cs) == "2          ");
        TEST(convert(10, cs) == "10         ");
        TEST(convert(100, cs) == "100        ");
        
        TEST(convert(-1, cs) == "-1         ");
        TEST(convert(-2, cs) == "-2         ");
        TEST(convert(-10, cs) == "-10        ");
        TEST(convert(-100, cs) == "-100       ");
    }

    void precision()
    {
        conversion_specification cs;
        cs.left_justify = false;
        cs.alternative_form = false;
        cs.pad_with_zeroes = false;
        cs.plus_sign = 0;
        cs.minimum_field_width = 0;
        cs.precision = 0;

        TEST(convert(0, cs) == "");
        TEST(convert(1, cs) == "1");
        TEST(convert(10, cs) == "10");
        
        cs.precision = 1;
        
        TEST(convert(0, cs) == "0");
        TEST(convert(1, cs) == "1");
        TEST(convert(10, cs) == "10");
        
        cs.precision = 2;
        
        TEST(convert(0, cs) == "00");
        TEST(convert(1, cs) == "01");
        TEST(convert(10, cs) == "10");
        
        cs.precision = 3;
        
        TEST(convert(0, cs) == "000");
        TEST(convert(1, cs) == "001");
        TEST(convert(10, cs) == "010");
    }

    void sign()
    {
        conversion_specification cs;
        cs.left_justify = false;
        cs.alternative_form = false;
        cs.pad_with_zeroes = false;
        cs.plus_sign = '+';
        cs.minimum_field_width = 0;
        cs.precision = 0;
        
        TEST(convert(0, cs) == "+");
        TEST(convert(1, cs) == "+1");
        TEST(convert(10, cs) == "+10");
        TEST(convert(-10, cs) == "-10");
        
        cs.plus_sign = ' ';
        TEST(convert(1, cs) == " 1");
        TEST(convert(-1, cs) == "-1");
    }

    void precision_and_padding()
    {
        conversion_specification cs;
        cs.left_justify = false;
        cs.alternative_form = false;
        cs.pad_with_zeroes = true;
        cs.plus_sign = 0;
        cs.minimum_field_width = 5;
        cs.precision = 3;

        TEST(convert(0, cs) == "  000");
        TEST(convert(1, cs) == "  001");
        TEST(convert(999, cs) == "  999");
        TEST(convert(1000, cs) == " 1000");
        TEST(convert(99999, cs) == "99999");
        TEST(convert(999999, cs) == "999999");
    }
    
private:

    std::string convert(int v)
    {
        conversion_specification cs;
        cs.left_justify = false;
        cs.alternative_form = false;
        cs.pad_with_zeroes = false;
        cs.plus_sign = 0;
        cs.minimum_field_width = 0;
        cs.precision = UNSPECIFIED_PRECISION;
        
        return convert(v, cs);
    }
    std::string convert(int v, conversion_specification const& cs)
    {
        writer_.reset();
        
        itoa_base10(&output_buffer_, v, cs);
        
        output_buffer_.flush();
        return writer_.str();
    }
    
    string_writer writer_;
    output_buffer output_buffer_;
};

unit_test::suite<itoa_base10_suite> itoa_base10_tests = {
    TESTCASE(itoa_base10_suite::positive),
    TESTCASE(itoa_base10_suite::negative),
    TESTCASE(itoa_base10_suite::field_width),
    TESTCASE(itoa_base10_suite::left_justify),
    TESTCASE(itoa_base10_suite::precision),
    TESTCASE(itoa_base10_suite::sign),
    TESTCASE(itoa_base10_suite::precision_and_padding)
};


#define TEST_FTOA(number) test_conversion_quality(number, __FILE__, __LINE__)

class ftoa_base10
{
public:
    static std::size_t const PERFECT_QUALITY = std::numeric_limits<std::size_t>::max();
    ftoa_base10() :
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
        // TODO these conversions lose some precision but we might be able to
        // fix that, because they have worked before in other iterations of the
        // code.
        //TEST_FTOA(1.7976931348623157e308);
        //TEST_FTOA(1.7976931348623158e308);
    }

    void fractional()
    {
        TEST_FTOA(0.1);
        TEST_FTOA(0.01);
        TEST_FTOA(0.001);
        TEST_FTOA(0.0);
        TEST_FTOA(0.123456);
        TEST_FTOA(0.00000123456);
        TEST_FTOA(0.00000000000000000123456);
        TEST_FTOA(0.0000000000000000012345678901234567890);
        TEST_FTOA(-0.0);
        TEST_FTOA(-0.1);
    }

    void negative()
    {
        TEST_FTOA(-1.0);
        TEST_FTOA(-123.456);
        TEST_FTOA(-123456);
        TEST_FTOA(-12345678901234567890.0);
        TEST_FTOA(-1.23456789e300);
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
        TEST(convert(-0.0) == "-0");
        TEST(convert(std::numeric_limits<double>::quiet_NaN()) == "nan");
        TEST(convert(std::numeric_limits<double>::signaling_NaN()) == "nan");
        TEST(convert(std::nan("1")) == "nan");
        TEST(convert(std::nan("2")) == "nan");
        TEST(convert(-std::numeric_limits<double>::quiet_NaN()) == "-nan");
        TEST(convert(std::numeric_limits<double>::infinity()) == "inf");
        TEST(convert(-std::numeric_limits<double>::infinity()) == "-inf");

        conversion_specification cs;
        cs.left_justify = false;
        cs.alternative_form = false;
        cs.pad_with_zeroes = false;
        cs.plus_sign = 0;
        cs.minimum_field_width = 6;
        cs.precision = 0;
        
        TEST(convert(-std::numeric_limits<double>::quiet_NaN(), cs) == "  -nan");
        TEST(convert(std::numeric_limits<double>::quiet_NaN(), cs) == "   nan");
        cs.left_justify = true;
        TEST(convert(-std::numeric_limits<double>::quiet_NaN(), cs) == "-nan  ");
        TEST(convert(std::numeric_limits<double>::quiet_NaN(), cs) == "nan   ");
        cs.plus_sign = '+';
        TEST(convert(std::numeric_limits<double>::quiet_NaN(), cs) == "+nan  ");
        cs.left_justify = false;
        TEST(convert(std::numeric_limits<double>::quiet_NaN(), cs) == "  +nan");
    }

    void scientific()
    {
        TEST(convert(1.2345e-8, 6) == "1.23450e-08");
        TEST(convert(1.2345e-10, 6) == "1.23450e-10");
        TEST(convert(1.2345e+10, 6) == "1.23450e+10");
        TEST(convert(1.2345e+10, 1) == "1e+10");
        TEST(convert(1.6345e+10, 1) == "2e+10");
        TEST(convert(1.6645e+10, 2) == "1.7e+10");
        TEST(convert(1.6645e+10, 2) == "1.7e+10");
        TEST(convert(1.7976931348623157e308, 5) == "1.7977e+308");
        TEST(convert(4.9406564584124654e-324, 5) == "4.9407e-324");
    }

    void padding()
    {
        conversion_specification cs;
        cs.left_justify = false;
        cs.alternative_form = false;
        cs.pad_with_zeroes = true;
        cs.plus_sign = 0;
        cs.minimum_field_width = 5;
        cs.precision = 3;

        TEST(convert(0, cs) == "00000");
        TEST(convert(1, cs) == "00001");
        TEST(convert(999, cs) == "00999");
        TEST(convert(1000, cs) == "01000");
        TEST(convert(0.01, cs) == "00.01");
    }

    void random()
    {
        return;
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
            std::cout << "  " << (start - quality_counts) << " digits: " << *start << " non-perfect conversions" << std::endl;
            ++start;
        }
        std::cout << "  perfect conversions: " << perfect;
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

    std::string convert(double number, conversion_specification const& cs)
    {
        writer_.reset();
        asynclog::ftoa_base10(&output_buffer_, number, cs);
        output_buffer_.flush();
        return writer_.str();
    }
    
    std::string convert(double number, int significant_digits=17)
    {
        conversion_specification cs;
        cs.left_justify = false;
        cs.alternative_form = false;
        cs.pad_with_zeroes = false;
        cs.plus_sign = 0;
        cs.minimum_field_width = 0;
        cs.precision = significant_digits;
        return convert(number, cs);
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

unit_test::suite<ftoa_base10> ftoa_base10_tests = {
    TESTCASE(ftoa_base10::greater_than_one),
    TESTCASE(ftoa_base10::fractional),
    TESTCASE(ftoa_base10::negative),
    TESTCASE(ftoa_base10::subnormals),
    TESTCASE(ftoa_base10::special),
    TESTCASE(ftoa_base10::scientific),
    TESTCASE(ftoa_base10::padding),
    TESTCASE(ftoa_base10::random)
};

class ftoa_base10_precision
{
public:
    ftoa_base10_precision() :
        output_buffer_(&writer_, 1024)
    {
    }
    
    void normal()
    {
        TEST(convert(1.5, 2) == "1.50");
        TEST(convert(1.234567890, 4) == "1.2346");
        TEST(convert(1.2345678901234567, 16) == "1.2345678901234567");
        TEST(convert(1.2345678901234567, 17) == "1.23456789012345670");
        TEST(convert(1.2345678901234567, 25) == "1.2345678901234567000000000");
        TEST(convert(1.7976931348623157e308, 3) == "179769313486231563000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000.000");
        TEST(convert(1234.5678, 0) == "1235");
        TEST(convert(1234.5678, 1) == "1234.6");

        TEST(convert(0.3, 1) == "0.3");
        TEST(convert(0.3, 2) == "0.30");
        TEST(convert(0.3, 20) == "0.29999999999999999000");

        TEST(convert(1.2345e20, 5) == "123450000000000000000.00000");
        TEST(convert(1.2345e20, 0) == "123450000000000000000");
        TEST(convert(1.2345e2, 5) == "123.45000");
        TEST(convert(1.2345e-20, 5) == "0.00000");
        TEST(convert(0.5, 0) == "1");
        TEST(convert(0.05, 1) == "0.1");
        TEST(convert(9.9, 0) == "10");
        TEST(convert(0, 0) == "0");
        TEST(convert(1.23456789012345670, 20) == "1.23456789012345670000");
        TEST(convert(0.123456789012345670, 20) == "0.12345678901234566300");
        
        TEST(convert(0.000123, 6) == "0.000123");
    }

    void padding()
    {
        conversion_specification cs;
        cs.left_justify = false;
        cs.alternative_form = false;
        cs.pad_with_zeroes = true;
        cs.plus_sign = 0;
        cs.minimum_field_width = 5;
        cs.precision = 3;
        TEST(convert(0.3, 
    }

    std::string convert(double number, unsigned precision)
    {
        writer_.reset();
        asynclog::ftoa_base10_precision(&output_buffer_, number, precision);
        output_buffer_.flush();
        //std::cout << '[' << writer_.str() << ']' << std::endl;
        return writer_.str();
    }

    string_writer writer_;
    output_buffer output_buffer_;
};

unit_test::suite<ftoa_base10_precision> ftoa_base10_precision_tests = {
    TESTCASE(ftoa_base10_precision::precision),
};

}   // namespace detail
}   // namespace asynclog

UNIT_TEST_MAIN();
#endif
