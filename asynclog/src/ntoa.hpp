#ifndef ASYNCLOG_DETAIL_ITOA_HPP
#define ASYNCLOG_DETAIL_ITOA_HPP
#include "asynclog/output_buffer.hpp"

#include <cstdint>
#include <limits>

namespace asynclog {
    
struct conversion_specification {
    bool left_justify;
    bool alternative_form;
    bool pad_with_zeroes;
    char plus_sign;
    unsigned minimum_field_width;
    unsigned precision;
};
unsigned const UNSPECIFIED_PRECISION = std::numeric_limits<unsigned>::max();

void itoa_base10(output_buffer* pbuffer, int value, conversion_specification const& cs);
void itoa_base10(output_buffer* pbuffer, unsigned int value, conversion_specification const& cs);
void itoa_base10(output_buffer* pbuffer, long value, conversion_specification const& cs);
void itoa_base10(output_buffer* pbuffer, unsigned long value, conversion_specification const& cs);
void itoa_base10(output_buffer* pbuffer, long long value, conversion_specification const& cs);
void itoa_base10(output_buffer* pbuffer, unsigned long long value, conversion_specification const& cs);

void itoa_base16(output_buffer* pbuffer, long value, bool uppercase, char const* prefix);
void itoa_base16(output_buffer* pbuffer, unsigned long value, bool uppercase, char const* prefix);

void ftoa_base10(output_buffer* pbuffer, double value, unsigned significant_digits, int minimum_exponent = -4, int maximum_exponent = 5);
void ftoa_base10_precision(output_buffer* pbuffer, double value, unsigned precision = 6);

}   // namespace asynclog

#endif  // ASYNCLOG_DETAIL_ITOA_HPP
