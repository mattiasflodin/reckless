#ifndef RECKLESS_DETAIL_ITOA_HPP
#define RECKLESS_DETAIL_ITOA_HPP
#include "reckless/output_buffer.hpp"

#include <cstdint>
#include <limits>

namespace reckless {
    
struct conversion_specification {
    unsigned minimum_field_width;
    unsigned precision;
    char plus_sign;
    bool left_justify;
    bool alternative_form;
    bool pad_with_zeroes;
    bool uppercase;
};
unsigned const UNSPECIFIED_PRECISION = std::numeric_limits<unsigned>::max();

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
