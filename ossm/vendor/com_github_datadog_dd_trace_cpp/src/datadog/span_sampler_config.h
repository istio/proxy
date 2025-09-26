#pragma once

// This component provides a `struct`, `SpanSamplerConfig`, used to configure
// `SpanSampler`. `SpanSampler` accepts a `FinalizedSpanSamplerConfig`, which
// must be obtained from a call to `finalize_config`.
//
// `SpanSamplerConfig` is specified as the `span_sampler` property of
// `TracerConfig`.

#include <unordered_map>
#include <vector>

#include "config.h"
#include "expected.h"
#include "json_fwd.hpp"
#include "logger.h"
#include "optional.h"
#include "rate.h"
#include "span_matcher.h"

namespace datadog {
namespace tracing {

struct SpanSamplerConfig {
  struct Rule : public SpanMatcher {
    double sample_rate = 1.0;
    Optional<double> max_per_second;

    Rule(const SpanMatcher&);
    Rule() = default;
  };

  // Can be overriden by the `DD_TRACE_SAMPLING_RULES` environment variable.
  // Also, the `DD_TRACE_SAMPLE_RATE` environment variable, if present, causes a
  // corresponding `Rule` to be appended to `rules`.
  std::vector<Rule> rules;
};

class FinalizedSpanSamplerConfig {
  friend Expected<FinalizedSpanSamplerConfig> finalize_config(
      const SpanSamplerConfig&, Logger&);
  friend class FinalizedTracerConfig;

  FinalizedSpanSamplerConfig() = default;

 public:
  struct Rule : public SpanMatcher {
    Rate sample_rate;
    Optional<double> max_per_second;
  };

  std::vector<Rule> rules;
  std::unordered_map<ConfigName, ConfigMetadata> metadata;
};

Expected<FinalizedSpanSamplerConfig> finalize_config(const SpanSamplerConfig&,
                                                     Logger&);

nlohmann::json to_json(const FinalizedSpanSamplerConfig::Rule&);

}  // namespace tracing
}  // namespace datadog
