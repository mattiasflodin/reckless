#ifndef ASYNCLOG_DETAIL_ITOA_HPP
#define ASYNCLOG_DETAIL_ITOA_HPP
#include "asynclog/output_buffer.hpp"

#include <cstdint>

namespace asynclog {
namespace detail {

void utoa_base10(output_buffer* pbuffer, unsigned int value);
void itoa_base10(output_buffer* pbuffer, int value);

void ftoa_base10_natural(output_buffer* pbuffer, double value, unsigned significant_digits);

}   // namespace detail
}   // namespace asynclog

#endif  // ASYNCLOG_DETAIL_ITOA_HPP
