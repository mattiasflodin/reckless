#ifndef RECKLESS_DETAIL_ITOA_HPP
#define RECKLESS_DETAIL_ITOA_HPP
#include <reckless/output_buffer.hpp>

#include <limits>

namespace reckless {
    
unsigned const UNSPECIFIED_PRECISION = std::numeric_limits<unsigned>::max();
// TODO It probably doesn't hurt to document these members even though they're
// in the printf docs. Also, would be nice to reduce footprint e.g. with a
// bitset.
struct conversion_specification {
    conversion_specification() :
        minimum_field_width(0),
        precision(UNSPECIFIED_PRECISION),
        plus_sign(0),
        left_justify(false),
        alternative_form(false),
        pad_with_zeroes(false),
        uppercase(false)
    {
    }
    unsigned minimum_field_width;
    unsigned precision;
    char plus_sign;
    bool left_justify;
    bool alternative_form;
    bool pad_with_zeroes;
    bool uppercase;
};

void itoa_base10(output_buffer* pbuffer, int value, conversion_specification const& cs);
void itoa_base10(output_buffer* pbuffer, unsigned int value, conversion_specification const& cs);
void itoa_base10(output_buffer* pbuffer, long value, conversion_specification const& cs);
void itoa_base10(output_buffer* pbuffer, unsigned long value, conversion_specification const& cs);
void itoa_base10(output_buffer* pbuffer, long long value, conversion_specification const& cs);
void itoa_base10(output_buffer* pbuffer, unsigned long long value, conversion_specification const& cs);

void itoa_base16(output_buffer* pbuffer, int value, conversion_specification const& cs);
void itoa_base16(output_buffer* pbuffer, unsigned int value, conversion_specification const& cs);
void itoa_base16(output_buffer* pbuffer, long value, conversion_specification const& cs);
void itoa_base16(output_buffer* pbuffer, unsigned long value, conversion_specification const& cs);
void itoa_base16(output_buffer* pbuffer, long long value, conversion_specification const& cs);
void itoa_base16(output_buffer* pbuffer, unsigned long long value, conversion_specification const& cs);

void ftoa_base10_f(output_buffer* pbuffer, double value, conversion_specification const& cs);
void ftoa_base10_g(output_buffer* pbuffer, double value, conversion_specification const& cs);

}   // namespace reckless

#endif  // RECKLESS_DETAIL_ITOA_HPP
