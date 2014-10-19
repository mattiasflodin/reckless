#ifndef ASYNCLOG_DETAIL_ITOA_HPP
#define ASYNCLOG_DETAIL_ITOA_HPP
#include "asynclog/output_buffer.hpp"

#include <cstdint>

namespace asynclog {
namespace detail {

void itoa_base10(output_buffer* pbuffer, int value);
void itoa_base10(output_buffer* pbuffer, unsigned int value);
void itoa_base10(output_buffer* pbuffer, long value);
void itoa_base10(output_buffer* pbuffer, unsigned long value);
void itoa_base10(output_buffer* pbuffer, long long value);
void itoa_base10(output_buffer* pbuffer, unsigned long long value);

void itoa_base16(output_buffer* pbuffer, long value, bool uppercase, char const* prefix);
void itoa_base16(output_buffer* pbuffer, unsigned long value, bool uppercase, char const* prefix);

void ftoa_base10(output_buffer* pbuffer, double value, unsigned significant_digits, int minimum_exponent = -4, int maximum_exponent = 5);
void ftoa_base10_precision(output_buffer* pbuffer, double value, unsigned precision = 6);

}   // namespace detail
}   // namespace asynclog

#endif  // ASYNCLOG_DETAIL_ITOA_HPP
