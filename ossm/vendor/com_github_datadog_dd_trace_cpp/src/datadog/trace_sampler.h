#pragma once

// This component provides a class, `TraceSampler`, that determines which
// traces that originate in this process will be sent to Datadog.
//
// `TraceSampler` is not instantiated directly, but is instead configured via
// `TracerConfig::trace_sampler`.
//
// When a span is extracted from an outside context (i.e.
// `Tracer::extract_span`), then the trace sampling decision is included in the
// extracted information.  In order to ensure that all parts of a trace are
// sampled consistently, such sampling decisions are honored.
//
// However, when this process is the first service in a distributed trace (i.e.
// `Tracer::create_span`), it makes the trace sampling decision.  The
// `TraceSampler` determines how the decision is made.
//
// There are three levels of configuration, in order of increasing specificity,
// accepted by the `TraceSampler`.
//
// 1. Agent Priority Sampling
// --------------------------
// The default sampling behavior is to consult the Datadog Agent for per-service
// sample rates.
//
// The Datadog Agent has a configured target number of traces per second to send
// to Datadog.  It chases this target by adjusting the sample rates of services
// that send it traces.  The target traces per second can be configured in the
// Datadog Agent via the environment variable `DD_APM_MAX_TPS` or the
// corresponding YAML configuration option `max_traces_per_second`.
//
// The Agent adjusts service-specific sample rates dynamically as trace volume
// fluctuates.
//
// 2. Global Sample Rate
// ---------------------
// If `TraceSamplerConfig::sample_rate` is given a value, or if the
// `DD_TRACE_SAMPLE_RATE` environment variable has a value, then the rate at
// which traces are kept is overridden to be the configured value.  The Datadog
// Agent provided rate is no longer used.
//
// For example, if `TracerSamplerConfig::sample_rate` is `0.1`, then 10% of
// traces that originate with this tracer will be sent to Datadog.  The
// remaining 90% will be sent to the Datadog Agent, but will not be sent to
// Datadog's backend and will not be visible in the Datadog UI.
//
// The volume of traces kept on account of the global sample rate is limited by
// the same setting as for trace sampling rules.  See the description of
// `TraceSamplerConfig::max_per_second` and `DD_TRACE_RATE_LIMIT` at the end of
// the following section.
//
// 3. Trace Sampling Rules
// -----------------------
// For finer-grained control over the sample rates of different kinds of traces,
// trace sampling rules can be defined.
//
// Trace sampling rules are configured via `TraceSamplerConfig::rules` or the
// `DD_TRACE_SAMPLING_RULES` environment variable.
//
// A trace sampling rule associates a span pattern with a sample rate.  If the
// root span of a new trace created by the tracer matches the span pattern,
// then the associated sample rate is applied.
//
// A span pattern can match a span in any combination of the following ways:
//
// - service name glob pattern
// - span name (operation name) glob pattern
// - resource name glob pattern
// - tag value glob patterns
//
// For more information on span matching and glob patterns, see
// `span_matcher.h`.
//
// If a root span matches multiple rules, then the sample rate of the first
// matching rule is used.
//
// The global rate (section 2, above) is implemented as a sampling rule that
// matches any span and is appended to any configured sampling rules.  Thus,
// sampling rules override the global sample rate for matching root spans.
//
// The volume of traces kept by sampling rules (including the global sample
// rate) is limited by a configurable number of traces-per-second.  The limit is
// configured via `TraceSamplerConfig::max_per_second` or the
// `DD_TRACE_RATE_LIMIT` environment variable.

#include <mutex>
#include <string>
#include <unordered_map>

#include "clock.h"
#include "json_fwd.hpp"
#include "limiter.h"
#include "optional.h"
#include "rate.h"
#include "trace_sampler_config.h"

namespace datadog {
namespace tracing {

struct CollectorResponse;
struct SamplingDecision;
struct SpanData;

class TraceSampler {
 private:
  std::mutex mutex_;

  Optional<Rate> collector_default_sample_rate_;
  std::unordered_map<std::string, Rate> collector_sample_rates_;
  std::vector<TraceSamplerRule> rules_;
  Limiter limiter_;
  double limiter_max_per_second_;

 public:
  TraceSampler(const FinalizedTraceSamplerConfig& config, const Clock& clock);

  void set_rules(std::vector<TraceSamplerRule> rules);

  // Return a sampling decision for the specified root span.
  SamplingDecision decide(const SpanData&);

  // Update this sampler's Agent-provided sample rates using the specified
  // collector response.
  void handle_collector_response(const CollectorResponse&);

  nlohmann::json config_json() const;
};

}  // namespace tracing
}  // namespace datadog
