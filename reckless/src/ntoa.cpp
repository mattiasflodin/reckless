/* This file is part of reckless logging
 * Copyright 2015, 2016 Mattias Flodin <git@codepentry.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <reckless/ntoa.hpp>

#include <reckless/writer.hpp>
#include <reckless/detail/utility.hpp>

#include <algorithm>    // max, min
#include <type_traits>  // is_unsigned
#include <cassert>
#include <cstring>      // memset
#include <cmath>        // lrint, llrint

namespace reckless {
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


template <typename T>
typename std::make_unsigned<T>::type unsigned_cast(T v)
{
    return static_cast<typename std::make_unsigned<T>::type>(v);
}

//void prefetch_digits()
//{
//    detail::prefetch(decimal_digits, sizeof(decimal_digits));
//}
//void prefetch_power_lut()
//{
//    detail::prefetch(power_lut, sizeof(power_lut));
//}

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

// For special case v=0, this returns 0.
template <class T>
typename std::enable_if<sizeof(T) == 4 && std::is_unsigned<T>::value, unsigned>::type
log2(T v)
{
    // From Sean Eron Anderson's "Bit Twiddling Hacks" collection.
    unsigned r;
    unsigned shift;

    r =     (v > 0xFFFF) << 4; v >>= r;
    shift = (v > 0xFF  ) << 3; v >>= shift; r |= shift;
    shift = (v > 0xF   ) << 2; v >>= shift; r |= shift;
    shift = (v > 0x3   ) << 1; v >>= shift; r |= shift;
                                            r |= (v >> 1);
    return r;
}

// For special case v=0, this returns 0.
template <class T>
typename std::enable_if<sizeof(T) == 8 && std::is_unsigned<T>::value, unsigned>::type
log2(T v)
{
    // log2 for 32-bit but extended for 64 bits.
    unsigned r;
    unsigned shift;

    r =     (v > 0xFFFFFFFF) << 5; v >>= r;
    shift = (v > 0xFFFF) << 4; v >>= shift; r |= shift;
    shift = (v > 0xFF  ) << 3; v >>= shift; r |= shift;
    shift = (v > 0xF   ) << 2; v >>= shift; r |= shift;
    shift = (v > 0x3   ) << 1; v >>= shift; r |= shift;
                                            r |= (v >> 1);
    return r;
}

// For special case x=0, this returns 0.
template <class Uint32>
typename std::enable_if<std::is_unsigned<Uint32>::value && sizeof(Uint32) == 4, unsigned>::type
log10(Uint32 x)
{
    // Compute logarithm by "binary search" based on magnitude/number of digits
    // in number. If we partition evenly (i.e. on 5) we get:
    // 0123456789
    // 01234 56789
    // 01 234 56 789
    // 0 1 2 34 5 6 7 89
    //       3 4      8 9
    //
    // But if we partition on 4 instead we get
    // 0123456789
    // 0123 456789
    // 01 23 456 789
    // 0 1 2 3 4 56 7 89
    //           5 6  8 9
    //
    // And if we partition on 4, we get
    // 0123456789
    // 012 3456789
    // 0 12 345 6789
    //   1 2 3 45 67 89
    //         4 5 6 7 8 9
    //
    // In all cases we get a maximum height of 5, but when we partition on 4 we
    // get a shorter depth for small numbers, which is probably more useful.

    if(x < 1000u) {
        // It's 0, 1 or 2.
        if(x < 10u) {
            return 0;
        } else {
            // It's 1 or 2.
            if(x < 100u) {
                return 1u;
            } else {
                return 2u;
            }
        }
    } else {
        // It's 3, 4, 5, 6, 7, 8 or 9.
        if(x < 1000000u) {
            // It's 3, 4, or 5.
            if(x < 10000u) {
                return 3;
            } else {
                // It's 4 or 5.
                if(x < 100000u) {
                    return 4;
                } else {
                    return 5;
                }
            }
        } else {
            // It's 6, 7, 8 or 9.
            if(x < 100000000u) {
                // It's 6 or 7.
                if(x < 10000000u) {
                    return 6;
                } else {
                    return 7;
                }
            } else {
                // It's 8 or 9.
                if(x < 1000000000u) {
                    return 8;
                } else {
                    return 9;
                }
            }
        }
    }
}

// For special case x=0, this returns 0.
template <class Uint64>
typename std::enable_if<std::is_unsigned<Uint64>::value && sizeof(Uint64) == 8, unsigned>::type
log10(Uint64 x)
{
    // Same principle as for the uint32_t variant, but for the binary tree
    // notation we now have A through J representing 10 to 19. Split-even
    // approach gives us:
    // 0123456789ABCDEFGHIJ
    // 0123456789 ABCDEFGHIJ
    // 01234 56789 ABCDE FGHIJ
    // 01 234 56 789 AB CDE FG HIJ
    // 0 1 2 34 5 6 7 89 A B C DE F G H IJ
    //       3 4      8 9      D E      I J
    // 
    // Splitting on 4 in the first branch gives us:
    // 012 3456789ABCDEFGHIJ
    // 0 12 3456789A BCDEFGHIJ
    //   1 2 3456 789A BCDE FGHIJ
    //       34 56 78 9A BC DE FG HIJ
    //       3 4 5 6 7 8 9 A B C D E F G H IJ
    //                                     I J
    // Faster for small number and lower average height.
    if(x < 1000u) {
        // It's 0, 1 or 2.
        if(x < 10u) {
            return 0;
        } else {
            // It's 1 or 2.
            if(x < 100u) {
                return 1u;
            } else {
                return 2u;
            }
        }
    } else {
        // It's 3 through 19.
        if(x < 100000000000u) {
            // It's 3, 4, 5, 6, 7, 8, 9 or 10.
            if(x < 10000000u) {
                // It's 3, 4, 5 or 6.
                if(x < 100000u) {
                    // It's 3 or 4.
                    if(x < 10000u) {
                        return 3u;
                    } else {
                        return 4u;
                    }
                } else {
                    // It's 5 or 6.
                    if(x < 1000000u) {
                        return 5u;
                    } else {
                        return 6u;
                    }
                }
            } else {
                // It's 7, 8, 9 or 10.
                if(x < 1000000000u) {
                    // It's 7 or 8.
                    if(x < 100000000u) {
                        return 7u;
                    } else {
                        return 8u;
                    }
                } else {
                    // It's 9 or 10.
                    if(x < 10000000000u) {
                        return 9u;
                    } else {
                        return 10u;
                    }
                }
            }
        } else {
            // It's 11, 12, 13, 14, 15, 16, 17, 18, or 19.
            if(x < 1000000000000000u) {
                // It's 11, 12, 13 or 14.
                if(x < 10000000000000u) {
                    // It's 11 or 12.
                    if(x < 1000000000000u) {
                        return 11u;
                    } else {
                        return 12u;
                    }
                } else {
                    // It's 13 or 14.
                    if(x < 100000000000000u) {
                        return 13u;
                    } else {
                        return 14u;
                    }
                }
            } else {
                // It's 15, 16, 17, 18 or 19.
                if(x < 100000000000000000u) {
                    // It's 15 or 16.
                    if(x < 10000000000000000u) {
                        return 15u;
                    } else {
                        return 16u;
                    }
                } else {
                    // It's 17, 18 or 19.
                    if(x < 1000000000000000000u) {
                        return 17u;
                    } else {
                        // It's 19 or 20.
                        if(x < 10000000000000000000u) {
                            return 18u;
                        } else {
                            return 19u;
                        }
                    }
                }
            }
        }
    }
}

template <typename Unsigned>
typename std::enable_if<std::is_unsigned<Unsigned>::value, Unsigned>::type
log16(Unsigned v)
{
    return log2(v)/4;
}

template <typename Unsigned>
void itoa_generic_base10(output_buffer* pbuffer, bool negative, Unsigned value, conversion_specification const& cs)
{
    // TODO measure if these prefetches help
    //prefetch_digits();
    char sign;
    if(negative) {
        sign = '-';
    } else {
        sign = cs.plus_sign;
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
    unsigned digits = value? log10(value) + 1 : 0;
    unsigned precision = (cs.precision == UNSPECIFIED_PRECISION? 1 : cs.precision);
    unsigned zeroes = precision>digits? precision - digits : 0;
    unsigned content_size = !!sign + zeroes + digits;
    unsigned size = std::max(content_size, cs.minimum_field_width);
    unsigned padding = size - content_size;
    if(cs.pad_with_zeroes && !cs.left_justify && cs.precision == UNSPECIFIED_PRECISION)
    {
        zeroes += padding;
        padding = 0;
    }
    
    char* str = pbuffer->reserve(size);
    unsigned pos = size;
    
    if(cs.left_justify) {
        pos -= padding;
        std::memset(str+pos, ' ', padding);
    }
    pos = utoa_generic_base10_preallocated(str, pos, value);
    pos -= zeroes;
    std::memset(str+pos, '0', zeroes);
    if(sign)
        str[--pos] = sign;
    if(!cs.left_justify) {
        pos -= padding;
        std::memset(str+pos, ' ', padding);
    }
    assert(pos == 0);
    pbuffer->commit(size);
}

template <typename Integer>
typename std::enable_if<std::is_signed<Integer>::value, void>::type
itoa_generic_base10(output_buffer* pbuffer, Integer value, conversion_specification const& cs)
{
    bool negative = value < 0;
    typename std::make_unsigned<Integer>::type uv;
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
void itoa_generic_base16(output_buffer* pbuffer, bool negative, Unsigned value, conversion_specification const& cs)
{
    char sign = negative? '-' : cs.plus_sign;
    unsigned digits = 0;
    unsigned prefix = 0;
    if(value != 0) {
        digits = log16(value) + 1;
        if(cs.alternative_form)
            prefix = 2;
    }
    unsigned precision = (cs.precision == UNSPECIFIED_PRECISION? 1 : cs.precision);
    unsigned zeroes = precision>digits? precision - digits : 0;
    unsigned content_size = !!sign + prefix + zeroes + digits;
    unsigned size = std::max(content_size, cs.minimum_field_width);
    unsigned padding = size - content_size;
    if(cs.pad_with_zeroes && !cs.left_justify && cs.precision == UNSPECIFIED_PRECISION)
    {
        zeroes += padding;
        padding = 0;
    }

    char* str = pbuffer->reserve(size);
    unsigned pos = size;
    if(cs.left_justify) {
        pos -= padding;
        std::memset(str + pos, ' ', padding);
    }

    if(cs.uppercase)
        pos = utoa_generic_base16_preallocated<true>(str, pos, value);
    else
        pos = utoa_generic_base16_preallocated<false>(str, pos, value);

    pos -= zeroes;
    std::memset(str + pos, '0', zeroes);
    if(prefix) {
        str[--pos] = cs.uppercase? 'X' : 'x';
        str[--pos] = '0';
    }
    if(sign)
        str[--pos] = sign;
    if(!cs.left_justify) {
        pos -= padding;
        std::memset(str + pos, ' ', padding);
    }

    assert(pos == 0);
    pbuffer->commit(size);
}
    
template <typename Integer>
typename std::enable_if<std::is_signed<Integer>::value, void>::type
itoa_generic_base16(output_buffer* pbuffer, Integer value, conversion_specification const& cs)
{
    bool negative = value < 0;
    typename std::make_unsigned<Integer>::type uv;
    if(negative)
        uv = unsigned_cast(-value);
    else
        uv = unsigned_cast(value);
    itoa_generic_base16(pbuffer, negative, uv, cs);
}

template <typename Integer>
typename std::enable_if<std::is_unsigned<Integer>::value, void>::type
itoa_generic_base16(output_buffer* pbuffer, Integer value, conversion_specification const& cs)
{
    itoa_generic_base16(pbuffer, false, value, cs);
}

template <typename Float>
Float fxtract(Float v, Float* exp)
{
    Float significand;
    asm("fxtract" : "=t"(significand), "=u"(*exp) : "0"(v));
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

decimal18 binary64_to_decimal18(double v)
{
    if(v == 0)
        return {std::signbit(v), 0, 0};
    
    long double e2;
    long double m2 = fxtract(static_cast<long double>(v), &e2);

    // We have
    //   v = m2 * 2^e2  [1 <= m2 < 2] (1)
    // and want a new representation
    //   v = m10 * 10^e10  [1 <= m10 < 10].
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

    m10 = m2*powl(10, e10f + 16);
    std::uint64_t mantissa = static_cast<std::uint64_t>(std::llrint(std::abs(m10)));
    if(mantissa < 100000000000000000u)
        mantissa *= 10;
    else
        e10i += 1.0L;
    // TODO rounding of m10 might cause it to overflow, right?. We need to
    // adjust again in that case. But try to reproduce this scenario first so
    // we know it's needed.

    return {std::signbit(v), mantissa, static_cast<int>(e10i)};
}

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

unsigned count_trailing_zeroes(decimal18 const& dv, std::uint64_t truncated_digits)
{
    std::uint64_t mantissa = rounded_rshift(dv.sign, dv.mantissa, truncated_digits);
    unsigned low = 0;
    unsigned high = 18u-truncated_digits;
    // Find the lowest x such that mantissa % 10^x != 0.
    // Then the number of trailing zeroes is x-1.
    while(low < high) {
        unsigned pos = (low + high)/2;
        if(mantissa % power_lut[pos] == 0)
            low = pos + 1;
        else
            high = pos;
    }

    return low-1;
}

unsigned count_trailing_zeroes_after_dot(decimal18 const& dv)
{
    // We need to get rid of the least significant digit because a double can't
    // always represent a zero digit at that position.
    // (e.g. 123.456 becomes 123.456000000000003).
    std::uint64_t mantissa = rounded_divide(dv.sign, dv.mantissa, 10);
    unsigned digits_before_dot = unsigned_cast(std::max(0, dv.exponent+1));
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

void ftoa_base10_f_normal(output_buffer* pbuffer, decimal18 dv, unsigned precision, conversion_specification const& cs)
{
    // [sign] [digits_before_dot] [zeroes_before_dot] [dot] [zeroes_after_dot] [digits_after_dot] [suffix_zeroes]
    char sign = dv.sign? '-' : cs.plus_sign;
    unsigned digits_before_dot;
    unsigned zeroes_before_dot;
    unsigned zeroes_after_dot;
    unsigned digits_after_dot;
    
    if(dv.exponent>=17) {
        // All digits from the mantissa are on the left-hand side of the dot.
        digits_before_dot = 18;
        zeroes_before_dot = unsigned_cast(dv.exponent)+1 - 18;
        zeroes_after_dot = precision;
        digits_after_dot = 0;
    } else if(dv.exponent < 0) {
        // All digits from the mantissa are on the right-hand side of the dot.
        // We set zeroes_before_dot=1 to generate a zero on the left-hand side,
        // since this is what people expect and it's what stdio does.
        digits_before_dot = 0;
        zeroes_before_dot = 1;
        zeroes_after_dot = std::min(unsigned_cast(-dv.exponent)-1, precision);
        digits_after_dot = std::min(18u, precision - zeroes_after_dot);
    } else {
        // There are digits from the mantissa both on the left- and right-hand
        // sides of the dot.
        if(dv.mantissa == 0) {
            // Special case for +-0: utoa_generic_base10_preallocated does not
            // output any digits for a zero value, so we need to fake it in by
            // using zeroes_before_dot instead.
            digits_before_dot = 0;
            zeroes_before_dot = 1;
            zeroes_after_dot = 0;
            digits_after_dot = 0;
        } else {
            digits_before_dot = unsigned_cast(dv.exponent+1);
            zeroes_before_dot = 0;
            zeroes_after_dot = 0;
            digits_after_dot = std::min(18 - digits_before_dot, precision);
        }
    }
    unsigned suffix_zeroes = precision - (zeroes_after_dot + digits_after_dot);
    
    bool dot = cs.alternative_form
        || (zeroes_after_dot + digits_after_dot + suffix_zeroes != 0);

    // Reduce the mantissa part to the requested number of digits.
    auto mantissa_digits = digits_before_dot + digits_after_dot;
    std::uint64_t mantissa = rounded_rshift(dv.sign, dv.mantissa, 18-mantissa_digits);
    auto order = power_lut[mantissa_digits];
    bool carry = mantissa >= order;
    if(carry) {
        // When reducing the mantissa, rounding caused it to overflow. For
        // example, when formatting 0.095 with precision=2, we have
        // zeroes_before_dot=1, zeroes_after_dot=1, mantissa_digits=1 and
        // suffix_zeroes=0. When trying to reduce the mantissa to 1 digit we
        // end up not with 9, but with 10 due to rounding. In this case we need
        // to make room for an extra digit from the mantissa, which we'll take
        // from zeroes_after_dot or by increasing digits_before_dot.
        // 
        // Logically this can only happen if there are actual digits from the
        // mantissa on the right-hand side of the dot. If all the digits are on
        // the left-hand side then no reduction will take place, because only
        // the number of fractional digits are user-controllable. So, if
        // zeroes_before_dot is nonzero then that's only to generate a filler
        // zero (see if clause above where dv.exponent < 0). If the overflow
        // now produces a digit on the left-hand side of the dot then we must
        // cancel out that zero by setting zeroes_before_dot=0.
        if(zeroes_after_dot) {
            --zeroes_after_dot;
            ++digits_after_dot;
        } else {
            ++digits_before_dot;
            zeroes_before_dot = 0;
        }
        ++mantissa_digits;
    }

    unsigned content_size = !!sign + digits_before_dot + zeroes_before_dot
        + dot + zeroes_after_dot + digits_after_dot + suffix_zeroes;
    unsigned size = std::max(cs.minimum_field_width, content_size);
    unsigned padding = size - content_size;
    unsigned pad_zeroes = 0;
    if(cs.pad_with_zeroes && !cs.left_justify) {
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
        pos -= suffix_zeroes;
        std::memset(str+pos, '0', suffix_zeroes);
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
        str[--pos] = sign;

    if(!cs.left_justify) {
        pos -= padding;
        std::memset(str+pos, ' ', padding);
    }
    pbuffer->commit(size);
    assert(pos == 0);
}

void ftoa_base10_e_normal(output_buffer* pbuffer, decimal18 dv,
        unsigned precision, conversion_specification const& cs)
{
    // We have either
    // [sign] [digit] [dot] [digits_after_dot] [e] [exponent sign] [exponent_digits]
    // or
    // [padding] [sign] [zero_padding] [digit] [dot] [digits_after_dot] [e] [exponent sign] [exponent_digits]
    // depending on if it's left-justified or not.

    char exponent_sign;
    unsigned exponent;
    if(dv.exponent < 0) {
        exponent_sign = '-';
        exponent = unsigned_cast(-dv.exponent);
    } else {
        exponent_sign = '+';
        exponent = unsigned_cast(dv.exponent);
    }
    unsigned digits_after_dot = precision;
    int dot = digits_after_dot!=0 || cs.alternative_form;
    
    int exponent_digits;
    // Apparently stdio never prints less than two digits for the exponent,
    // so we'll do the same to stay consistent. Maximum value of the exponent
    // in a double is 308 so we won't get more than 3 digits.
    if(exponent < 100)
        exponent_digits = 2;
    else
        exponent_digits = 3;

    char sign = dv.sign? '-' : cs.plus_sign;
    unsigned content_size = !!sign + 1 + dot + digits_after_dot + 1 + 1 + exponent_digits;
    unsigned size = std::max(cs.minimum_field_width, content_size);
    unsigned padding = size - content_size;
    unsigned pad_zeroes = 0;
    if(cs.pad_with_zeroes && !cs.left_justify) {
        pad_zeroes = padding;
        padding = 0;
    }
    
    // Get rid of digits we don't want in the mantissa.
    std::uint64_t mantissa = rounded_rshift(dv.sign, dv.mantissa,
            18-(digits_after_dot+1));
    
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
        str[0] = sign;
    
    if(!cs.left_justify) {
        pos -= padding;
        std::memset(str+pos, ' ', padding);
    }
    pbuffer->commit(size);
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

void itoa_base16(output_buffer* pbuffer, int value, conversion_specification const& cs)
{
    itoa_generic_base16(pbuffer, value, cs);
}

void itoa_base16(output_buffer* pbuffer, unsigned int value, conversion_specification const& cs)
{
    itoa_generic_base16(pbuffer, value, cs);
}

void itoa_base16(output_buffer* pbuffer, long value, conversion_specification const& cs)
{
    itoa_generic_base16(pbuffer, value, cs);
}

void itoa_base16(output_buffer* pbuffer, unsigned long value, conversion_specification const& cs)
{
    itoa_generic_base16(pbuffer, value, cs);
}

void itoa_base16(output_buffer* pbuffer, long long value, conversion_specification const& cs)
{
    itoa_generic_base16(pbuffer, value, cs);
}
void itoa_base16(output_buffer* pbuffer, unsigned long long value, conversion_specification const& cs)
{
    itoa_generic_base16(pbuffer, value, cs);
}

void ftoa_base10_f(output_buffer* pbuffer, double value, conversion_specification const& cs)
{
    // TODO measure if these prefetches help
    //prefetch_digits();
    //prefetch_power_lut();
    auto category = std::fpclassify(value);
    if(category == FP_NAN) {
        return write_nan(pbuffer, value, cs);
    } else if(category == FP_INFINITE) {
        return write_inf(pbuffer, value, cs);
    } else {
        decimal18 dv = binary64_to_decimal18(value);
        int p = cs.precision == UNSPECIFIED_PRECISION? 6 : cs.precision;
        return ftoa_base10_f_normal(pbuffer, dv, p, cs);
    }
}

void ftoa_base10_g(output_buffer* pbuffer, double value, conversion_specification const& cs)
{
    // The idea of %g is that the precision says how many significant digits we
    // want in the string representation. If the number of significant digits is
    // not enough to represent all the digits up to the dot (i.e. it is higher than
    // the number's exponent), then %e notation is used instead.
    // 
    // A special feature of %g which is not present in %e and %f is that if there
    // are trailing zeroes in the fraction then those will be removed, unless
    // alternative mode is requested. Alternative mode also means that the period
    // stays, no matter if there are any fractional decimals remaining or
    // not.
    //prefetch_digits();
    //prefetch_power_lut();
    auto category = std::fpclassify(value);
    if(category == FP_NAN) {
        return write_nan(pbuffer, value, cs);
    } else if(category == FP_INFINITE) {
        return write_inf(pbuffer, value, cs);
    } else {
        int const minimum_exponent = -4;
        decimal18 dv = binary64_to_decimal18(value);

        int p;
        if(cs.precision == UNSPECIFIED_PRECISION)
            p = 6;
        else if(cs.precision == 0)
            p = 1;
        else {
            p = cs.precision;
            if(!cs.alternative_form) {
                // We are supposed to remove trailing zeroes, so we can't ever
                // get more than 18 digits because the rest will be zero
                // anyway, as this is the number of digits in the mantissa.
                p = std::min(p, 18);
            }
        }
        unsigned truncated_digits = 18 - p;
        
        if(p > dv.exponent && dv.exponent >= minimum_exponent) {
            unsigned trailing_zeroes = 0;
            if(!cs.alternative_form) {
                trailing_zeroes = count_trailing_zeroes_after_dot(dv);
                if(trailing_zeroes > truncated_digits)
                    trailing_zeroes -= truncated_digits;
                else
                    trailing_zeroes = 0;
            }
            p = p - 1 - dv.exponent - trailing_zeroes;
            p = std::max(0, p);
            return ftoa_base10_f_normal(pbuffer, dv, p, cs);
        } else {
            unsigned trailing_zeroes = 0;
            if(!cs.alternative_form)
                trailing_zeroes = count_trailing_zeroes(dv, truncated_digits);
            p = p - 1 - trailing_zeroes;
            return ftoa_base10_e_normal(pbuffer, dv, p, cs);
        }
    }
}

}   // namespace reckless

#ifdef UNIT_TEST
#include "unit_test.hpp"

#include <string>
#include <sstream>  // istringstream, ostringstream
#include <iomanip>  // iomanip
#include <random>
#include <algorithm>    // mismatch

namespace reckless {
namespace detail {
class whitebox_output_buffer : public output_buffer {
public:
    using output_buffer::output_buffer;
    using output_buffer::flush;
};

class log10_suite {
public:
    void uint32()
    {
        test<std::uint32_t>(10);
    }

    void uint64()
    {
        test<std::uint64_t>(20);
    }

private:
    template <class T>
    void test(std::size_t digits)
    {
        T v = 0;
        TEST(log10(v) == 0);
        v = 1;
        TEST(log10(v) == 0);
        v = 10;
        for(unsigned i=1; i!=digits; ++i)
        {
            //std::cerr << v-1 << " -> " << i-1 << '?'  << std::endl;
            TEST(log10(v-1) == i-1);
            //std::cerr << v << " -> " << i << '?'  << std::endl;
            TEST(log10(v) == i);
            v *= 10;
        }
    }
};

unit_test::suite<log10_suite> log10_tests = {
    TESTCASE(log10_suite::uint32),
    TESTCASE(log10_suite::uint64),
};

class string_writer : public writer {
public:
    std::size_t write(void const* pbuffer, std::size_t count, std::error_code& ec) noexcept override
    {
        auto pc = static_cast<char const*>(pbuffer);
        buffer_.insert(buffer_.end(), pc, pc + count);
        ec.clear();
        return count;
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
        TEST(convert(std::numeric_limits<long long>::max()) == std::to_string(std::numeric_limits<long long>::max()));
    }

    void negative()
    {
        TEST(convert(-1) == "-1");
        TEST(convert(-2) == "-2");
        TEST(convert(-10) == "-10");
        TEST(convert(-100) == "-100");
        TEST(convert(std::numeric_limits<long long>::min()) == std::to_string(std::numeric_limits<long long>::min()));
    }

    void field_width()
    {
        conversion_specification cs;
        cs.minimum_field_width = 11;
        cs.precision = UNSPECIFIED_PRECISION;
        cs.left_justify = false;
        cs.alternative_form = false;
        cs.pad_with_zeroes = false;
        cs.plus_sign = 0;
        cs.uppercase = false;
        
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
        cs.minimum_field_width = 11;
        cs.precision = UNSPECIFIED_PRECISION;
        cs.left_justify = true;
        cs.alternative_form = false;
        cs.pad_with_zeroes = false;
        cs.plus_sign = 0;
        cs.uppercase = false;
        
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
        cs.minimum_field_width = 0;
        cs.precision = 0;
        cs.left_justify = false;
        cs.alternative_form = false;
        cs.pad_with_zeroes = false;
        cs.plus_sign = 0;
        cs.uppercase = 0;

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
        cs.minimum_field_width = 0;
        cs.precision = 0;
        cs.left_justify = false;
        cs.alternative_form = false;
        cs.pad_with_zeroes = false;
        cs.plus_sign = '+';
        cs.uppercase = false;
        
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
        cs.minimum_field_width = 5;
        cs.precision = 3;
        cs.left_justify = false;
        cs.alternative_form = false;
        cs.pad_with_zeroes = true;
        cs.plus_sign = 0;
        cs.uppercase = false;

        // "for d, i, o, u, x, and X conversions, if a precision is specified,
        // the 0 flag is ignored."
        TEST(convert(0, cs) == "  000");
        TEST(convert(1, cs) == "  001");
        TEST(convert(999, cs) == "  999");
        TEST(convert(1000, cs) == " 1000");
        TEST(convert(99999, cs) == "99999");
        TEST(convert(999999, cs) == "999999");

        cs.precision = UNSPECIFIED_PRECISION;
        TEST(convert(0, cs) == "00000");
        TEST(convert(1, cs) == "00001");
        TEST(convert(1000, cs) == "01000");
    }
    
private:
    template <class T>
    std::string convert(T v)
    {
        conversion_specification cs;
        cs.minimum_field_width = 0;
        cs.precision = UNSPECIFIED_PRECISION;
        cs.left_justify = false;
        cs.alternative_form = false;
        cs.pad_with_zeroes = false;
        cs.plus_sign = 0;
        cs.uppercase = false;
        
        return convert(v, cs);
    }
    template <class T>
    std::string convert(T v, conversion_specification const& cs)
    {
        writer_.reset();
        
        itoa_base10(&output_buffer_, v, cs);
        
        output_buffer_.flush();
        return writer_.str();
    }
    
    string_writer writer_;
    whitebox_output_buffer output_buffer_;
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

class itoa_base16_suite
{
public:
    itoa_base16_suite() :
        output_buffer_(&writer_, 1024)
    {
    }

    void positive()
    {
        TEST(convert(0) == "0");
        TEST(convert(1) == "1");
        TEST(convert(2) == "2");
        TEST(convert(0x10) == "10");
        TEST(convert(0x100) == "100");
        std::ostringstream ostr;
        ostr << std::hex << std::numeric_limits<long long>::max();
        TEST(convert(std::numeric_limits<long long>::max()) == ostr.str());
    }

    void negative()
    {
        TEST(convert(-1) == "-1");
        TEST(convert(-2) == "-2");
        TEST(convert(-0x10) == "-10");
        TEST(convert(-0x100) == "-100");

        // stdlib doesn't support signed integers for hexadecimal output so
        // we'll have to do some patchwork.
        long long v = std::numeric_limits<long long>::max();
        std::ostringstream ostr;
        ostr << std::hex << v;
        std::string expected = std::string("-") + ostr.str();
        TEST(convert(-std::numeric_limits<long long>::max()) == expected);
    }

    void field_width()
    {
        conversion_specification cs;
        cs.minimum_field_width = 11;
        cs.precision = UNSPECIFIED_PRECISION;
        cs.left_justify = false;
        cs.alternative_form = false;
        cs.pad_with_zeroes = false;
        cs.plus_sign = 0;
        cs.uppercase = false;
        
        TEST(convert(0x0, cs) == "          0");
        TEST(convert(0x1, cs) == "          1");
        TEST(convert(0x2, cs) == "          2");
        TEST(convert(0x10, cs) == "         10");
        TEST(convert(0x100, cs) == "        100");
        
        TEST(convert(-0x1, cs) == "         -1");
        TEST(convert(-0x2, cs) == "         -2");
        TEST(convert(-0x10, cs) == "        -10");
        TEST(convert(-0x100, cs) == "       -100");
    }

    void left_justify()
    {
        conversion_specification cs;
        cs.minimum_field_width = 11;
        cs.precision = UNSPECIFIED_PRECISION;
        cs.left_justify = true;
        cs.alternative_form = false;
        cs.pad_with_zeroes = false;
        cs.plus_sign = 0;
        cs.uppercase = false;
        
        TEST(convert(0x0, cs) == "0          ");
        TEST(convert(0x1, cs) == "1          ");
        TEST(convert(0x2, cs) == "2          ");
        TEST(convert(0x10, cs) == "10         ");
        TEST(convert(0x100, cs) == "100        ");
        
        TEST(convert(-0x1, cs) == "-1         ");
        TEST(convert(-0x2, cs) == "-2         ");
        TEST(convert(-0x10, cs) == "-10        ");
        TEST(convert(-0x100, cs) == "-100       ");
    }

    void precision()
    {
        conversion_specification cs;
        cs.minimum_field_width = 0;
        cs.precision = 0;
        cs.left_justify = false;
        cs.alternative_form = false;
        cs.pad_with_zeroes = false;
        cs.plus_sign = 0;
        cs.uppercase = 0;

        TEST(convert(0, cs) == "");
        TEST(convert(1, cs) == "1");
        TEST(convert(0x10, cs) == "10");
        
        cs.precision = 1;
        
        TEST(convert(0x0, cs) == "0");
        TEST(convert(0x1, cs) == "1");
        TEST(convert(0x10, cs) == "10");
        
        cs.precision = 2;
        
        TEST(convert(0x0, cs) == "00");
        TEST(convert(0x1, cs) == "01");
        TEST(convert(0x10, cs) == "10");
        
        cs.precision = 3;
        
        TEST(convert(0x0, cs) == "000");
        TEST(convert(0x1, cs) == "001");
        TEST(convert(0x10, cs) == "010");
    }

    void sign()
    {
        conversion_specification cs;
        cs.minimum_field_width = 0;
        cs.precision = 0;
        cs.left_justify = false;
        cs.alternative_form = false;
        cs.pad_with_zeroes = false;
        cs.plus_sign = '+';
        cs.uppercase = false;
        
        TEST(convert(0x0, cs) == "+");
        TEST(convert(0x1, cs) == "+1");
        TEST(convert(0x10, cs) == "+10");
        TEST(convert(-0x10, cs) == "-10");
        
        cs.plus_sign = ' ';
        TEST(convert(0x01, cs) == " 1");
        TEST(convert(-0x1, cs) == "-1");
    }

    void precision_and_padding()
    {
        conversion_specification cs;
        cs.minimum_field_width = 5;
        cs.precision = 3;
        cs.left_justify = false;
        cs.alternative_form = false;
        cs.pad_with_zeroes = true;
        cs.plus_sign = 0;
        cs.uppercase = false;

        TEST(convert(0x0, cs) == "  000");
        TEST(convert(0x1, cs) == "  001");
        TEST(convert(0x999, cs) == "  999");
        TEST(convert(0x1000, cs) == " 1000");
        TEST(convert(0x99999, cs) == "99999");
        TEST(convert(0x999999, cs) == "999999");
        
        cs.precision = UNSPECIFIED_PRECISION;
        TEST(convert(0x0, cs) == "00000");
        TEST(convert(0x1, cs) == "00001");
        TEST(convert(0x1000, cs) == "01000");
    }

    void xcase()
    {
        conversion_specification cs;
        cs.minimum_field_width = 0;
        cs.precision = UNSPECIFIED_PRECISION;
        cs.left_justify = false;
        cs.alternative_form = true;
        cs.pad_with_zeroes = false;
        cs.plus_sign = 0;
        cs.uppercase = false;
        
        TEST(convert(0xabcdef, cs) == "0xabcdef");
        TEST(convert(-0xabcdef, cs) == "-0xabcdef");
        cs.uppercase = true;
        TEST(convert(0xabcdef, cs) == "0XABCDEF");
        TEST(convert(-0xabcdef, cs) == "-0XABCDEF");
    }
    
private:

    template <class T>
    std::string convert(T v)
    {
        conversion_specification cs;
        cs.minimum_field_width = 0;
        cs.precision = UNSPECIFIED_PRECISION;
        cs.left_justify = false;
        cs.alternative_form = false;
        cs.pad_with_zeroes = false;
        cs.plus_sign = 0;
        cs.uppercase = false;
        
        return convert(v, cs);
    }
    template <class T>
    std::string convert(T v, conversion_specification const& cs)
    {
        writer_.reset();
        
        itoa_base16(&output_buffer_, v, cs);
        
        output_buffer_.flush();
        return writer_.str();
    }
    
    string_writer writer_;
    whitebox_output_buffer output_buffer_;
};

unit_test::suite<itoa_base16_suite> itoa_base16_tests = {
    TESTCASE(itoa_base16_suite::positive),
    TESTCASE(itoa_base16_suite::negative),
    TESTCASE(itoa_base16_suite::field_width),
    TESTCASE(itoa_base16_suite::left_justify),
    TESTCASE(itoa_base16_suite::precision),
    TESTCASE(itoa_base16_suite::sign),
    TESTCASE(itoa_base16_suite::precision_and_padding),
    TESTCASE(itoa_base16_suite::xcase)
};

class ftoa_base10_f
{
public:
    ftoa_base10_f() :
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
        cs.minimum_field_width = 7;
        cs.precision = 3;
        cs.left_justify = false;
        cs.alternative_form = false;
        cs.pad_with_zeroes = true;
        cs.plus_sign = 0;
        cs.uppercase = false;
        
        TEST(convert(0.5, cs) == "000.500");
        TEST(convert(-0.5, cs) == "-00.500");
        TEST(convert(0.0, cs) == "000.000");
        TEST(convert(-0.0, cs) == "-00.000");
        
        cs.left_justify = true;
        
        TEST(convert(0.5, cs) == "0.500  ");
        TEST(convert(-0.5, cs) == "-0.500 ");
        TEST(convert(0.0, cs) == "0.000  ");
        TEST(convert(-0.0, cs) == "-0.000 ");
    }

private:
    std::string convert(double number, conversion_specification const& cs)
    {
        writer_.reset();
        reckless::ftoa_base10_f(&output_buffer_, number, cs);
        output_buffer_.flush();
        //std::cout << '[' << writer_.str() << ']' << std::endl;
        return writer_.str();
    }

    std::string convert(double number, unsigned precision)
    {
        conversion_specification cs;
        cs.minimum_field_width = 0;
        cs.precision = precision;
        cs.left_justify = false;
        cs.alternative_form = false;
        cs.pad_with_zeroes = false;
        cs.plus_sign = 0;
        cs.uppercase = false;
        return convert(number, cs);
    }

    string_writer writer_;
    whitebox_output_buffer output_buffer_;
};

unit_test::suite<ftoa_base10_f> ftoa_base10_precision_tests = {
    TESTCASE(ftoa_base10_f::normal),
    TESTCASE(ftoa_base10_f::padding),
};

#define TEST_FTOA(number) test_conversion_quality(number, __FILE__, __LINE__)

class ftoa_base10_g
{
public:
    static int const PERFECT_QUALITY = std::numeric_limits<int>::max();
    ftoa_base10_g() :
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
        TEST_FTOA(std::nextafter(1.7976931348623157e308, 2.0));
    }

    void fractional()
    {
        // TODO test zero with precision>0 etc
        TEST_FTOA(0.1);
        TEST_FTOA(0.01);
        TEST_FTOA(0.001);
        TEST_FTOA(0.0);
        TEST_FTOA(0.123456);
        TEST_FTOA(0.00000123456);
        TEST_FTOA(0.00000000000000000123456);
        // TODO below test works but doesn't give perfect output. Can we fix?
        //TEST_FTOA(0.0000000000000000012345678901234567890);
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
        cs.minimum_field_width = 6;
        cs.precision = 0;
        cs.left_justify = false;
        cs.alternative_form = false;
        cs.pad_with_zeroes = false;
        cs.plus_sign = 0;
        cs.uppercase = false;
        
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
        TEST(convert(1.2345e-8, 6) == "1.2345e-08");
        TEST(convert(1.2345e-10, 6) == "1.2345e-10");
        TEST(convert(1.2345e+10, 6) == "1.2345e+10");
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
        cs.minimum_field_width = 5;
        cs.precision = 3;
        cs.left_justify = false;
        cs.alternative_form = false;
        cs.pad_with_zeroes = true;
        cs.plus_sign = 0;
        cs.uppercase = false;

        TEST(convert(0, cs) == "00000");
        TEST(convert(1, cs) == "00001");
        TEST(convert(999, cs) == "00999");
        TEST(convert(1000, cs) == "1e+03");
        TEST(convert(0.01, cs) == "00.01");
    }

    void random()
    {
        std::mt19937_64 rng;
        int const total = 100000000;
        int perfect = 0;
        int correct_superfluous = 0;
        int quality_counts[18] = {0};
        std::vector<int> superfluous_counts;
        int i = 0;
        while(i != total) {
            std::uint64_t bits = rng();
            bits &= (std::uint64_t(1)<<63)-1;
            unsigned exponent = static_cast<unsigned>(bits >> 52);
            if(exponent == 0 || exponent == 2047)
                continue;
            double const v = *reinterpret_cast<double const*>(&bits);
            //double const v = 1.116216926772162e-277;

            auto quality = get_conversion_quality(v);
            if(quality == PERFECT_QUALITY) {
                perfect++;
            } else {
                assert(quality < 18);
                if(quality>=0)
                    quality_counts[quality]++;
                else {
                    quality = -quality;
                    if(static_cast<std::size_t>(quality) >= superfluous_counts.size())
                        superfluous_counts.resize(quality+1);
                    superfluous_counts[quality]++;
                    correct_superfluous++;
                }
            }
            ++i;
        }
        std::cout << "  perfect conversions: " << perfect;
        std::cout << " (" << 100*static_cast<double>(perfect)/total << "%)\n";
        std::cout << "  correct conversions: " << (perfect+correct_superfluous);
        std::cout << " (" << 100*static_cast<double>(perfect+correct_superfluous)/total << "%)\n";
        std::cout << "  Count of N correct significant digits:\n";
        for(std::size_t i=0; i!=18; ++i)
            std::cout << "    " << i << ": " << quality_counts[i]  << 
                " (" << 100*static_cast<double>(quality_counts[i])/total << "%)\n";
        std::cout << "  Count of N superfluous/unneeded digits:\n";
        for(std::size_t i=0; i!=superfluous_counts.size(); ++i)
            std::cout << "    " << i << ": " << superfluous_counts[i] <<
                " (" << 100*static_cast<double>(superfluous_counts[i])/total << "%)\n";
    }

private:
    std::pair<std::string, std::string> normalize_for_comparison(std::string const& number)
    {
        auto exponent_pos = number.find('e');
        std::string mantissa(number, 0, exponent_pos);
        auto decimal_point = mantissa.find('.');
        if(decimal_point != std::string::npos)
            mantissa.erase(decimal_point, 1);
        auto nonzero_pos = mantissa.find_first_not_of('0');
        if(nonzero_pos != std::string::npos)
            mantissa.erase(0, nonzero_pos);
        
        std::string exponent;
        if(exponent_pos != std::string::npos)
            exponent.assign(number, exponent_pos+1, std::string::npos);

        return {mantissa, exponent};
    }

    // PERFECT_QUALITY return: Character-for-character correct.
    // Positive return: number of correct significant digits
    // Negative return: number of superfluous significant digits.
    int get_conversion_quality(double number)
    {
        std::string const& str = convert(number);
        std::istringstream istr(str);
        double number2;
        istr >> number2;
        //{
            char buf[32];
            std::sprintf(buf, "%.18g", number);
            if(str == buf)
                return PERFECT_QUALITY;
            std::string mantissa1, exponent1;
            tie(mantissa1, exponent1) = normalize_for_comparison(str);

            std::string mantissa2, exponent2;
            tie(mantissa2, exponent2) = normalize_for_comparison(buf);

            assert(exponent1 == exponent2);
            //assert(mantissa1.size() == mantissa2.size());
            auto size = std::min(mantissa1.size(), mantissa2.size());
            std::size_t correct_digits = mismatch(
                begin(mantissa1), begin(mantissa1)+size,
                begin(mantissa2)).first - begin(mantissa1);
            auto expected = mantissa2.size();
            if(correct_digits < expected) {
                // Only some of the characters we produced match the characters
                // produced by stdio. Return how many digits were correct.
                // However: sometimes we actually perform better than the
                // runtime library does. An example of this is 123.456 where we
                // get "123.456" and stdio prints "123.456000000000003" which
                // converts back to the exact same floating-point number. Hence
                // "123.456" would have been sufficient. By counting "correct
                // digits" it looks as if we got only 6 matching digits and got
                // the other 12 wrong. To avoid reporting this as poor quality,
                // we check whether our string converts back to the same value
                // and if it does, we consider it to be perfect quality.
                //std::istringstream istr(str);
                //double number2;
                //istr >> number2;
                if(number != number2)
                    return static_cast<int>(correct_digits);
                else
                    return PERFECT_QUALITY;
            } else {
                assert(mantissa1.size() > expected);
                return -static_cast<int>(mantissa1.size() - expected);
            }
    }

    std::string convert(double number, conversion_specification const& cs)
    {
        writer_.reset();
        reckless::ftoa_base10_g(&output_buffer_, number, cs);
        output_buffer_.flush();
        return writer_.str();
    }
    
    std::string convert(double number, int significant_digits=18)
    {
        conversion_specification cs;
        cs.minimum_field_width = 0;
        cs.precision = significant_digits;
        cs.left_justify = false;
        cs.alternative_form = false;
        cs.pad_with_zeroes = false;
        cs.plus_sign = 0;
        cs.uppercase = false;
        
        auto str = convert(number, cs);
        //char buf[128];
        //std::sprintf(buf, "%.*g", significant_digits, number);
        //std::cout << str << '\t' << buf << std::endl;
        return str;
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
    whitebox_output_buffer output_buffer_;
};

unit_test::suite<ftoa_base10_g> ftoa_base10_tests = {
    TESTCASE(ftoa_base10_g::greater_than_one),
    TESTCASE(ftoa_base10_g::fractional),
    TESTCASE(ftoa_base10_g::negative),
    TESTCASE(ftoa_base10_g::subnormals),
    TESTCASE(ftoa_base10_g::special),
    TESTCASE(ftoa_base10_g::scientific),
    TESTCASE(ftoa_base10_g::padding),
    TESTCASE(ftoa_base10_g::random)
};

}   // namespace detail
}   // namespace reckless

UNIT_TEST_MAIN();
#endif
