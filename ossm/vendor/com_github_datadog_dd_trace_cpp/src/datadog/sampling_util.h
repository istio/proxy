#pragma once

// This component provides sampling-related miscellanea.  It's used by both
// `TraceSampler` and `SpanSampler`.

#include <cstdint>
#include <limits>

#include "rate.h"

namespace datadog {
namespace tracing {

// Return a hash value for the specified `value`.  `value` is one of the
// following:
//
// - a 64-bit span ID
// - a 64-bit trace ID
// - the lower 64 bits of a 128-bit trace ID
//
// The resulting hash value is compared with an upper bound provided by
// `max_id_from_rate` (below) to determine whether the span/trace associated
// with `value` is eligible for keeping on statistical grounds.
inline std::uint64_t knuth_hash(std::uint64_t value) {
  return value * UINT64_C(1111111111111111111);
}

inline std::uint64_t max_id_from_rate(Rate rate) {
  // `double(std::numeric_limits<uint64_t>::max())` is slightly larger than the
  // largest `uint64_t`, but consider it a fun fact that the largest `double`
  // less than 1.0 (i.e. the "previous value" to 1.0), when multiplied by the
  // max `uint64_t`, results in a number not greater than the max `uint64_t`.
  // So, the only special case to consider is 1.0.
  if (rate == 1.0) {
    return std::numeric_limits<uint64_t>::max();
  }

  return static_cast<std::uint64_t>(
      rate * static_cast<double>(std::numeric_limits<std::uint64_t>::max()));
}

}  // namespace tracing
}  // namespace datadog
