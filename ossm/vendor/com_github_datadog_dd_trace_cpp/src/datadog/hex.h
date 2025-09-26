#pragma once

// This component provides functions for formatting an unsigned integral value
// in hexadecimal.

#include <cassert>
#include <charconv>
#include <limits>
#include <system_error>
#include <utility>

namespace datadog {
namespace tracing {

// Return the specified unsigned `value` formatted as a lower-case hexadecimal
// string without any leading zeroes.
template <typename UnsignedInteger>
std::string hex(UnsignedInteger value) {
  static_assert(!std::numeric_limits<UnsignedInteger>::is_signed);

  // 4 bits per hex digit char
  char buffer[std::numeric_limits<UnsignedInteger>::digits / 4];

  const int base = 16;
  auto result =
      std::to_chars(std::begin(buffer), std::end(buffer), value, base);
  assert(result.ec == std::errc());

  return std::string{std::begin(buffer), result.ptr};
}

// Return the specified unsigned `value` formatted as a lower-case hexadecimal
// string with leading zeroes.
template <typename UnsignedInteger>
std::string hex_padded(UnsignedInteger value) {
  static_assert(!std::numeric_limits<UnsignedInteger>::is_signed);

  // 4 bits per hex digit char.
  char buffer[std::numeric_limits<UnsignedInteger>::digits / 4];

  const int base = 16;
  auto result =
      std::to_chars(std::begin(buffer), std::end(buffer), value, base);
  assert(result.ec == std::errc());

  const auto num_zeroes = sizeof(buffer) - (result.ptr - std::begin(buffer));
  std::string padded(num_zeroes, '0');
  padded.append(std::begin(buffer), result.ptr);
  return padded;
}

}  // namespace tracing
}  // namespace datadog