#pragma once

// This component provides facilities for generating sequences of IDs used as
// span IDs and trace IDs.
//
// `IDGenerator` is an interface for generating trace IDs and span IDs.
//
// `default_id_generator` is an `IDGenerator` that produces a thread-local
// pseudo-random sequence of uniformly distributed 63-bit unsigned integers. The
// sequence is randomly seeded once per thread and anytime the process forks.

#include <cstdint>
#include <memory>

#include "clock.h"
#include "trace_id.h"

namespace datadog {
namespace tracing {

class IDGenerator {
 public:
  virtual ~IDGenerator() = default;

  // Generate a span ID.
  virtual std::uint64_t span_id() const = 0;
  // Generate a trace ID for a trace having the specified `start` time.
  virtual TraceID trace_id(const TimePoint& start) const = 0;
};

std::shared_ptr<const IDGenerator> default_id_generator(bool trace_id_128_bit);

}  // namespace tracing
}  // namespace datadog
