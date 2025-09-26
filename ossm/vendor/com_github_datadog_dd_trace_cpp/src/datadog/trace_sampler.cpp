#include "trace_sampler.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>

#include "collector_response.h"
#include "json.hpp"
#include "sampling_decision.h"
#include "sampling_priority.h"
#include "sampling_util.h"
#include "span_data.h"

namespace datadog {
namespace tracing {

TraceSampler::TraceSampler(const FinalizedTraceSamplerConfig& config,
                           const Clock& clock)
    : rules_(config.rules),
      limiter_(clock, config.max_per_second),
      limiter_max_per_second_(config.max_per_second) {}

void TraceSampler::set_rules(std::vector<TraceSamplerRule> rules) {
  std::lock_guard lock(mutex_);
  rules_ = std::move(rules);
}

SamplingDecision TraceSampler::decide(const SpanData& span) {
  SamplingDecision decision;
  decision.origin = SamplingDecision::Origin::LOCAL;

  // First check sampling rules.
  const auto found_rule =
      std::find_if(rules_.cbegin(), rules_.cend(),
                   [&](const auto& it) { return it.matcher.match(span); });

  // `mutex_` protects `limiter_`, `collector_sample_rates_`, and
  // `collector_default_sample_rate_`, so let's lock it here.
  std::lock_guard lock(mutex_);

  if (found_rule != rules_.end()) {
    const auto& rule = *found_rule;
    decision.mechanism = int(rule.mechanism);
    decision.limiter_max_per_second = limiter_max_per_second_;
    decision.configured_rate = rule.rate;
    const std::uint64_t threshold = max_id_from_rate(rule.rate);
    if (knuth_hash(span.trace_id.low) < threshold) {
      const auto result = limiter_.allow();
      if (result.allowed) {
        decision.priority = int(SamplingPriority::USER_KEEP);
      } else {
        decision.priority = int(SamplingPriority::USER_DROP);
      }
      decision.limiter_effective_rate = result.effective_rate;
    } else {
      decision.priority = int(SamplingPriority::USER_DROP);
    }

    return decision;
  }

  // No sampling rule matched.  Find the appropriate collector-controlled
  // sample rate.
  auto found_rate = collector_sample_rates_.find(
      CollectorResponse::key(span.service, span.environment().value_or("")));
  if (found_rate != collector_sample_rates_.end()) {
    decision.configured_rate = found_rate->second;
    decision.mechanism = int(SamplingMechanism::AGENT_RATE);
  } else {
    if (collector_default_sample_rate_) {
      decision.configured_rate = *collector_default_sample_rate_;
      decision.mechanism = int(SamplingMechanism::AGENT_RATE);
    } else {
      // We have yet to receive a default rate from the collector.  This
      // corresponds to the `DEFAULT` sampling mechanism.
      decision.configured_rate = Rate::one();
      decision.mechanism = int(SamplingMechanism::DEFAULT);
    }
  }

  const std::uint64_t threshold = max_id_from_rate(*decision.configured_rate);
  if (knuth_hash(span.trace_id.low) < threshold) {
    decision.priority = int(SamplingPriority::AUTO_KEEP);
  } else {
    decision.priority = int(SamplingPriority::AUTO_DROP);
  }

  return decision;
}

void TraceSampler::handle_collector_response(
    const CollectorResponse& response) {
  const auto found =
      response.sample_rate_by_key.find(response.key_of_default_rate);
  std::lock_guard<std::mutex> lock(mutex_);

  if (found != response.sample_rate_by_key.end()) {
    collector_default_sample_rate_ = found->second;
  }

  collector_sample_rates_ = response.sample_rate_by_key;
}

nlohmann::json TraceSampler::config_json() const {
  std::vector<nlohmann::json> rules;
  for (const auto& rule : rules_) {
    rules.push_back(rule.to_json());
  }

  return nlohmann::json::object({
      {"rules", rules},
      {"max_per_second", limiter_max_per_second_},
  });
}

}  // namespace tracing
}  // namespace datadog
