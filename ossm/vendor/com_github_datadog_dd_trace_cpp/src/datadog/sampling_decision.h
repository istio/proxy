#pragma once

// This component provides a `struct`, `SamplingDecision`, that describes a
// keep/drop sampling decision (for either trace sampling or span sampling) and
// contains supporting information about the reason for the decision.

#include "optional.h"
#include "rate.h"
#include "sampling_mechanism.h"

namespace datadog {
namespace tracing {

struct SamplingDecision {
  enum class Origin {
    // There was already a sampling decision associated with this trace when we
    // extracted the local root span from somewhere else (i.e.
    // `Tracer::extract_span`).
    EXTRACTED,
    // We made the sampling decision for this trace/span based on one of
    // `TraceSampler`, `SpanSampler`, or
    // `TraceSegment::override_sampling_priority`).
    LOCAL,
    // We made a provisional sampling decision earlier, and later requested that
    // another service that we call make the sampling decision instead. That
    // service then responded with its own sampling decision, which is this one.
    DELEGATED
  };

  // See `sampling_priority.h`. Positive values mean "keep," while others mean
  // "drop."
  int priority;
  // See `sampling_mechanism.h`.
  Optional<int> mechanism;
  // The sample rate associated with this decision, if any.
  Optional<Rate> configured_rate;
  // The effective rate of the limiter consulted in this decision, if any. A
  // limiter's effective rate is `num_allowed / num_asked`.
  Optional<Rate> limiter_effective_rate;
  // The per-second maximum allowed number of "keeps" configured for the limiter
  // consulted in this decision, if any.
  Optional<double> limiter_max_per_second;
  // The provenance of this decision.
  Origin origin;
};

}  // namespace tracing
}  // namespace datadog
