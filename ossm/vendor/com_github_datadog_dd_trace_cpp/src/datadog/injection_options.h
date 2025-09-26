#pragma once

// This component provides a `struct InjectionOptions` containing optional
// parameters to `Span::inject` that alter the behavior of trace context
// propagation.

#include "optional.h"

namespace datadog {
namespace tracing {

struct InjectionOptions {
  // If this tracer is using the "Datadog" propagation injection style, then
  // include a request header that indicates that whoever extracts this trace
  // context "on the other side" may make their own trace sampling decision
  // and convey it back to us in a response header. If
  // `delegate_sampling_decision` is null, then its value depends on the tracer
  // configuration (see `TracerConfig::delegate_trace_sampling`).
  Optional<bool> delegate_sampling_decision;
};

}  // namespace tracing
}  // namespace datadog
