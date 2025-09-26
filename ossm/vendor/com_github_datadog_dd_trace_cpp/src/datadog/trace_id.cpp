#include "trace_id.h"

#include "hex.h"
#include "parse_util.h"
#include "string_util.h"

namespace datadog {
namespace tracing {

TraceID::TraceID() : low(0), high(0) {}

TraceID::TraceID(std::uint64_t low) : low(low), high(0) {}

TraceID::TraceID(std::uint64_t low, std::uint64_t high)
    : low(low), high(high) {}

std::string TraceID::hex_padded() const {
  std::string result;
  if (high) {
    result += ::datadog::tracing::hex_padded(high);
  } else {
    result.append(16, '0');
  }
  result += ::datadog::tracing::hex_padded(low);
  return result;
}

Expected<TraceID> TraceID::parse_hex(StringView input) {
  const auto parse_hex_piece =
      [input](StringView piece) -> Expected<std::uint64_t> {
    auto result = parse_uint64(piece, 16);
    if (auto *error = result.if_error()) {
      std::string prefix = "Unable to parse trace ID from \"";
      append(prefix, input);
      prefix += "\": ";
      return error->with_prefix(prefix);
    }
    return result;
  };

  // A 64-bit integer is at most 16 hex characters.  If the input is no
  // longer than that, then it will all fit in `TraceID::low`.
  if (input.size() <= 16) {
    auto result = parse_hex_piece(input);
    if (auto *error = result.if_error()) {
      return std::move(*error);
    }
    return TraceID(*result);
  }

  // Parse the lower part and the higher part separately.
  const auto divider = input.size() - 16;
  const auto high_hex = input.substr(0, divider);
  const auto low_hex = input.substr(divider);

  TraceID trace_id;

  auto result = parse_hex_piece(low_hex);
  if (auto *error = result.if_error()) {
    return std::move(*error);
  }
  trace_id.low = *result;

  result = parse_hex_piece(high_hex);
  if (auto *error = result.if_error()) {
    return std::move(*error);
  }
  trace_id.high = *result;

  return trace_id;
}

bool operator==(TraceID left, TraceID right) {
  return left.low == right.low && left.high == right.high;
}

bool operator!=(TraceID left, TraceID right) {
  return left.low != right.low || left.high != right.high;
}

bool operator==(TraceID left, std::uint64_t right) {
  return left == TraceID{right};
}

bool operator!=(TraceID left, std::uint64_t right) {
  return left != TraceID{right};
}

bool operator==(std::uint64_t left, TraceID right) { return right == left; }

bool operator!=(std::uint64_t left, TraceID right) { return right != left; }

}  // namespace tracing
}  // namespace datadog
