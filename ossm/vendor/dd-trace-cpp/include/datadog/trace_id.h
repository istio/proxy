#pragma once

// This component provides a `struct`, `TraceID`, that represents an opaque,
// unique identifier for a trace.
// `TraceID` is 128 bits wide, though in some contexts only the lower 64 bits
// are used.

#include <cstdint>
#include <string>

#include "expected.h"
#include "string_view.h"

namespace datadog {
namespace tracing {

struct TraceID {
  std::uint64_t low;
  std::uint64_t high;

  // Create a zero trace ID.
  TraceID();

  // Create a trace ID whose lower 64 bits are the specified `low` and whose
  // higher 64 bits are zero.
  explicit TraceID(std::uint64_t low);

  // Create a trace ID whose lower 64 bits are the specified `low` and whose
  // higher 64 bits are the specified `high`.
  TraceID(std::uint64_t low, std::uint64_t high);

  // Return a 32 character lower-case hexadecimal representation of this trace
  // ID, padded with zeroes on the left.
  std::string hex_padded() const;

  // Return a `TraceID` parsed from the specified hexadecimal string, or return
  // an `Error`. It is an error of the input contains any non-hexadecimal
  // characters.
  static Expected<TraceID> parse_hex(StringView);
};

bool operator==(TraceID, TraceID);
bool operator!=(TraceID, TraceID);
bool operator==(TraceID, std::uint64_t);
bool operator!=(TraceID, std::uint64_t);
bool operator==(std::uint64_t, TraceID);
bool operator!=(std::uint64_t, TraceID);

}  // namespace tracing
}  // namespace datadog
