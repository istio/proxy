#pragma once

// This component provides a `struct`, `TraceSamplerConfig`, used to configure
// `TraceSampler`. `TraceSampler` accepts a `FinalizedTraceSamplerConfig`, which
// must be obtained from a call to `finalize_config`.
//
// `TraceSamplerConfig` is specified as the `trace_sampler` property of
// `TracerConfig`.

#include <unordered_map>
#include <vector>

#include "config.h"
#include "expected.h"
#include "json_fwd.hpp"
#include "optional.h"
#include "rate.h"
#include "sampling_mechanism.h"
#include "span_matcher.h"

namespace datadog {
namespace tracing {

struct TraceSamplerRule final {
  Rate rate;
  SpanMatcher matcher;
  SamplingMechanism mechanism;

  nlohmann::json to_json() const;
};

struct TraceSamplerConfig {
  struct Rule : public SpanMatcher {
    double sample_rate = 1.0;

    Rule(const SpanMatcher&);
    Rule() = default;
  };

  Optional<double> sample_rate;
  std::vector<Rule> rules;
  Optional<double> max_per_second;
};

class FinalizedTraceSamplerConfig {
  friend Expected<FinalizedTraceSamplerConfig> finalize_config(
      const TraceSamplerConfig& config);
  friend class FinalizedTracerConfig;

  FinalizedTraceSamplerConfig() = default;

 public:
  double max_per_second;
  std::vector<TraceSamplerRule> rules;
  std::unordered_map<ConfigName, ConfigMetadata> metadata;
};

Expected<FinalizedTraceSamplerConfig> finalize_config(
    const TraceSamplerConfig& config);

}  // namespace tracing
}  // namespace datadog
