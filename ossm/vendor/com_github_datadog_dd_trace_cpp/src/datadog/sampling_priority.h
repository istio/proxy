#pragma once

// This component defines an enumeration of "sampling priority" values.
//
// Sampling priority is a hybrid between a sampling decision ("keep" versus
// "drop") and a sampling reason ("user-specified rule").  Values less than or
// equal to zero indicate a decision to "drop," while positive values indicate
// a decision to "keep."
//
// The "priority" in the term "sampling priority" is a misnomer, since the
// value does not denote any relationship among the different kinds of sampling
// decisions.

#include "sampling_mechanism.h"

namespace datadog {
namespace tracing {

enum class SamplingPriority {
  // Drop on account of user configuration, such as an explicit sampling
  // rate, sampling rule, or a manual override.
  USER_DROP = -1,
  // Drop on account of a rate conveyed by the Datadog Agent.
  AUTO_DROP = 0,
  // Keep on account of a rate conveyed by the Datadog Agent.
  AUTO_KEEP = 1,
  // Keep on account of user configuration, such as an explicit sampling
  // rate, sampling rule, or a manual override.
  USER_KEEP = 2
};

}  // namespace tracing
}  // namespace datadog
