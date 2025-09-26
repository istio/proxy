#pragma once

// This component provides symbols for all span tag names that have special
// meaning.

#include <string>

#include "string_util.h"
#include "string_view.h"

namespace datadog {
namespace tracing {
namespace tags {

extern const std::string environment;
extern const std::string service_name;
extern const std::string span_type;
extern const std::string operation_name;
extern const std::string resource_name;
extern const std::string version;

namespace internal {
extern const std::string propagation_error;
extern const std::string decision_maker;
extern const std::string origin;
extern const std::string hostname;
extern const std::string sampling_priority;
extern const std::string rule_sample_rate;
extern const std::string rule_limiter_sample_rate;
extern const std::string agent_sample_rate;
extern const std::string span_sampling_mechanism;
extern const std::string span_sampling_rule_rate;
extern const std::string span_sampling_limit;
extern const std::string w3c_extraction_error;
extern const std::string trace_id_high;
extern const std::string process_id;
extern const std::string language;
extern const std::string runtime_id;
extern const std::string sampling_decider;
extern const std::string w3c_parent_id;
}  // namespace internal

// Return whether the specified `tag_name` is reserved for use internal to this
// library.
inline bool is_internal(StringView tag_name) {
  return starts_with(tag_name, "_dd.");
}

}  // namespace tags
}  // namespace tracing
}  // namespace datadog
