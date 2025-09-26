#pragma once

// This component provides a `struct`, `SpanData`, that contains all data fields
// relevant to `Span`. `SpanData` is what is consumed by `Collector`.

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "clock.h"
#include "expected.h"
#include "optional.h"
#include "string_view.h"
#include "trace_id.h"

namespace datadog {
namespace tracing {

struct SpanConfig;
struct SpanDefaults;

struct SpanData {
  std::string service;
  std::string service_type;
  std::string name;
  std::string resource;
  TraceID trace_id;
  std::uint64_t span_id = 0;
  std::uint64_t parent_id = 0;
  TimePoint start;
  Duration duration = Duration::zero();
  bool error = false;
  std::unordered_map<std::string, std::string> tags;
  std::unordered_map<std::string, double> numeric_tags;

  Optional<StringView> environment() const;
  Optional<StringView> version() const;

  // Modify the properties of this object to honor the specified `config` and
  // `defaults`.  The properties of `config`, if set, override the properties of
  // `defaults`. Use the specified `clock` to provide a start none of none is
  // specified in `config`.
  void apply_config(const SpanDefaults& defaults, const SpanConfig& config,
                    const Clock& clock);
};

// Append to the specified `destination` the MessagePack representation of the
// specified `span`.
Expected<void> msgpack_encode(std::string& destination, const SpanData& span);

// Append to the specified `destination` the MessagePack representation of an
// array containing each of the specified `spans`.  The behavior is undefined
// if any span is `nullptr`.
Expected<void> msgpack_encode(
    std::string& destination,
    const std::vector<std::unique_ptr<SpanData>>& spans);

}  // namespace tracing
}  // namespace datadog
