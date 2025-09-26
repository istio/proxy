#pragma once

// This component provides a class, `TraceSegment`, that represents a portion of
// a trace that is passing through this process.
//
// `TraceSegment` is not instantiated directly.  It is an implementation detail
// of this library.
//
// A trace might begin in this process, or it might have been propagated in from
// outside (see `Tracer::extract_span`).  A trace might remain in this process,
// or it might be propagated outward (see `Span::inject`) one or more times.
//
// A trace might pass through this process twice or more.  Consider an RPC
// server that receives a request, in handling that request makes a request to a
// different service, and in the course of the other service handling its
// request, the original service is called again.  Both "passes" through this
// process are part of the same trace, but each pass is a different _trace
// segment_.
//
// `TraceSegment` stores context and configuration shared among all spans within
// the trace segment, and additionally owns the spans' data.  When `Tracer`
// creates or extracts a span, it also creates a new `TraceSegment`.  When a
// child `Span` is created from a `Span`, the child and the parent share the
// same `TraceSegment`.
//
// When all of the `Span`s associated with `TraceSegment` have been destroyed,
// the `TraceSegment` submits them in a payload to a `Collector`.

#include <cstddef>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "config_manager.h"
#include "expected.h"
#include "metrics.h"
#include "optional.h"
#include "propagation_style.h"
#include "sampling_decision.h"
#include "tracer_telemetry.h"

namespace datadog {
namespace tracing {

class Collector;
class DictReader;
class DictWriter;
struct InjectionOptions;
class Logger;
struct SpanData;
struct SpanDefaults;
class SpanSampler;
class TraceSampler;

class TraceSegment {
  mutable std::mutex mutex_;

  std::shared_ptr<Logger> logger_;
  std::shared_ptr<Collector> collector_;
  std::shared_ptr<TracerTelemetry> tracer_telemetry_;
  std::shared_ptr<TraceSampler> trace_sampler_;
  std::shared_ptr<SpanSampler> span_sampler_;

  std::shared_ptr<const SpanDefaults> defaults_;
  RuntimeID runtime_id_;
  const std::vector<PropagationStyle> injection_styles_;
  const Optional<std::string> hostname_;
  const Optional<std::string> origin_;
  const std::size_t tags_header_max_size_;
  std::vector<std::pair<std::string, std::string>> trace_tags_;

  std::vector<std::unique_ptr<SpanData>> spans_;
  std::size_t num_finished_spans_;
  Optional<SamplingDecision> sampling_decision_;
  Optional<std::string> additional_w3c_tracestate_;
  Optional<std::string> additional_datadog_w3c_tracestate_;

  std::shared_ptr<ConfigManager> config_manager_;

  // See `doc/sampling-delegation.md` for more information about
  // `struct SamplingDelegation`.
  struct SamplingDelegation {
    // This segment is configured to delegate its sampling decision.
    bool enabled;
    // The trace context from which the local root span was extracted delegated
    // the sampling decision to this segment.
    bool decision_was_delegated_to_me;
    // This segment included a request for sampling delegation in outbound
    // injected trace context (see `inject`).
    bool sent_request_header;
    // This segment received a (presumably delegated) sampling decision. See
    // `read_sampling_delegation_response`.
    bool received_matching_response_header;
    // This segment conveyed a sampling decision back to a parent service that
    // had previously requested a delegated sampling decision.
    bool sent_response_header;
  } sampling_delegation_ = {};

 public:
  TraceSegment(const std::shared_ptr<Logger>& logger,
               const std::shared_ptr<Collector>& collector,
               const std::shared_ptr<TracerTelemetry>& tracer_telemetry,
               const std::shared_ptr<TraceSampler>& trace_sampler,
               const std::shared_ptr<SpanSampler>& span_sampler,
               const std::shared_ptr<const SpanDefaults>& defaults,
               const std::shared_ptr<ConfigManager>& config_manager,
               const RuntimeID& runtime_id, bool sampling_delegation_enabled,
               bool sampling_decision_was_delegated_to_me,
               const std::vector<PropagationStyle>& injection_styles,
               const Optional<std::string>& hostname,
               Optional<std::string> origin, std::size_t tags_header_max_size,
               std::vector<std::pair<std::string, std::string>> trace_tags,
               Optional<SamplingDecision> sampling_decision,
               Optional<std::string> additional_w3c_tracestate,
               Optional<std::string> additional_datadog_w3c_tracestate,
               std::unique_ptr<SpanData> local_root);

  const SpanDefaults& defaults() const;
  const Optional<std::string>& hostname() const;
  const Optional<std::string>& origin() const;
  Optional<SamplingDecision> sampling_decision() const;

  Logger& logger() const;

  // Inject trace context for the specified `span` into the specified `writer`.
  // Return whether the trace sampling decision was delegated.
  // This function is the implementation of `Span::inject`.
  bool inject(DictWriter& writer, const SpanData& span);
  bool inject(DictWriter& writer, const SpanData& span,
              const InjectionOptions& options);

  // Inject this segment's trace sampling decision into the specified `writer`,
  // if appropriate.
  void write_sampling_delegation_response(DictWriter& writer);

  // Extract a trace sampling decision from the specified `reader` if it has
  // one, and use the resulting decision, if appropriate.
  Expected<void> read_sampling_delegation_response(const DictReader& reader);

  // Take ownership of the specified `span`.
  void register_span(std::unique_ptr<SpanData> span);
  // Increment the number of finished spans.  If that number is equal to the
  // number of registered spans, send all of the spans to the `Collector`.
  void span_finished();

  // Set the sampling decision to be a local, manual decision with the specified
  // sampling `priority`.  Overwrite any previous sampling decision.
  void override_sampling_priority(int priority);

 private:
  // If `sampling_decision_` is null, use `trace_sampler_` to make a
  // sampling decision and assign it to `sampling_decision_`.
  void make_sampling_decision_if_null();
  // Set or remove the `tags::internal::decision_maker` trace tag in
  // `trace_tags_` according to either information extracted from trace context
  // or from a local sampling decision.
  void update_decision_maker_trace_tag();
};

}  // namespace tracing
}  // namespace datadog
