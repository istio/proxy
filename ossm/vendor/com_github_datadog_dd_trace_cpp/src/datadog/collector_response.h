#pragma once

// This component provides a `struct`, `CollectorResponse`, that contains
// information that a `Collector` such as the Datadog Agent might deliver in
// response to traces sent.
//
// It is really only filled out by the Datadog Agent, via `DatadogAgent` in
// `datadog_agent.h`. It contains a mapping from keys to sample rates, where a
// key identifies a service in a particular environment, and the sample rate is
// the rate at which the trace sampler should keep traces produced by that
// service in that environment.
//
// See `TraceSampler::handle_collector_response` in `trace_sampler.h` for more
// information.

#include <string>
#include <unordered_map>

#include "rate.h"
#include "string_view.h"

namespace datadog {
namespace tracing {

struct CollectorResponse {
  static std::string key(StringView service, StringView environment);
  static const std::string key_of_default_rate;
  std::unordered_map<std::string, Rate> sample_rate_by_key;
};

}  // namespace tracing
}  // namespace datadog
