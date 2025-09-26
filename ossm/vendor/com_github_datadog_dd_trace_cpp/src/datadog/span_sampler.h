#pragma once

// This component provides a class, `SpanSampler`, that determines which spans
// should be sent to Datadog (if any) when the enclosing trace is not going to
// be sent to Datadog.
//
// The primary way to control which data gets sent to Datadog is trace sampling.
// See `trace_sampler.h` for a description of trace sampling.
//
// `SpanSampler` allows individual spans to be sent to Datadog even when a trace
// is dropped.
//
// As with the `TraceSampler`, spans are matched by rules that indicate the
// sample rate at which the spans will be sent to Datadog.  Each rule is
// additionally associated with an optional limiter that prevents the sent
// volume of spans from exceeding a specified number of spans per second.
//
// See `span_matcher.h` for a description of how spans are matched by span
// sampling rules.

#include <memory>
#include <mutex>

#include "clock.h"
#include "json_fwd.hpp"
#include "limiter.h"
#include "sampling_decision.h"
#include "span_sampler_config.h"

namespace datadog {
namespace tracing {

class SpanSampler {
 public:
  struct SynchronizedLimiter {
    std::mutex mutex;
    Limiter limiter;

    SynchronizedLimiter(const Clock&, double max_per_second);
  };

  class Rule : public FinalizedSpanSamplerConfig::Rule {
    std::unique_ptr<SynchronizedLimiter> limiter_;

   public:
    explicit Rule(const FinalizedSpanSamplerConfig::Rule&, const Clock&);

    // Return a sampling decision for the specified span.
    SamplingDecision decide(const SpanData&);
  };

 private:
  std::vector<Rule> rules_;

 public:
  explicit SpanSampler(const FinalizedSpanSamplerConfig& config,
                       const Clock& clock);

  // Return a pointer to the first `Rule` that the specified span matches, or
  // return null if there is no match.
  Rule* match(const SpanData&);

  nlohmann::json config_json() const;
};

}  // namespace tracing
}  // namespace datadog
