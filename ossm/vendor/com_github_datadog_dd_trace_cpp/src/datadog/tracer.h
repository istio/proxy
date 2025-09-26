#pragma once

// This component provides a class, `Tracer`, that instantiates the mechanisms
// necessary for tracing, and provides member functions for creating spans.
// Each span created by `Tracer` is either the root of a new trace (see
// `create_span`) or part of an existing trace whose information is extracted
// from a provided key/value source (see `extract_span`).
//
// `Tracer` is instantiated with a `FinalizedTracerConfig`, which can be
// obtained from a `TracerConfig` via the `finalize_config` function.  See
// `tracer_config.h`.

#include <cstddef>
#include <memory>

#include "clock.h"
#include "config_manager.h"
#include "error.h"
#include "expected.h"
#include "id_generator.h"
#include "json_fwd.hpp"
#include "optional.h"
#include "span.h"
#include "tracer_config.h"
#include "tracer_signature.h"
#include "tracer_telemetry.h"

namespace datadog {
namespace tracing {

class DictReader;
struct SpanConfig;
class TraceSampler;
class SpanSampler;

class Tracer {
  std::shared_ptr<Logger> logger_;
  std::shared_ptr<ConfigManager> config_manager_;
  std::shared_ptr<Collector> collector_;
  RuntimeID runtime_id_;
  TracerSignature signature_;
  std::shared_ptr<TracerTelemetry> tracer_telemetry_;
  std::shared_ptr<SpanSampler> span_sampler_;
  std::shared_ptr<const IDGenerator> generator_;
  Clock clock_;
  std::vector<PropagationStyle> injection_styles_;
  std::vector<PropagationStyle> extraction_styles_;
  Optional<std::string> hostname_;
  std::size_t tags_header_max_size_;
  bool sampling_delegation_enabled_;

 public:
  // Create a tracer configured using the specified `config`, and optionally:
  // - using the specified `generator` to create trace IDs and span IDs
  // - using the specified `clock` to get the current time.
  explicit Tracer(const FinalizedTracerConfig& config);
  Tracer(const FinalizedTracerConfig& config,
         const std::shared_ptr<const IDGenerator>& generator);

  // Create a new trace and return the root span of the trace.  Optionally
  // specify a `config` indicating the attributes of the root span.
  Span create_span();
  Span create_span(const SpanConfig& config);

  // Return a span whose parent and other context is parsed from the specified
  // `reader`, and whose attributes are determined by the optionally specified
  // `config`.  If there is no tracing information in `reader`, then return an
  // error with code `Error::NO_SPAN_TO_EXTRACT`.  If a failure occurs, then
  // return an error with some other code.
  Expected<Span> extract_span(const DictReader& reader);
  Expected<Span> extract_span(const DictReader& reader,
                              const SpanConfig& config);

  // Return a span extracted from the specified `reader` (see `extract_span`).
  // If there is no span to extract, then return a span that is the root of a
  // new trace (see `create_span`).  Optionally specify a `config` indicating
  // the attributes of the span.  If a failure occurs, then return an error.
  // Note that the absence of a span to extract is not considered an error.
  Expected<Span> extract_or_create_span(const DictReader& reader);
  Expected<Span> extract_or_create_span(const DictReader& reader,
                                        const SpanConfig& config);

  // Return a JSON object describing this Tracer's configuration. It is the same
  // JSON object that was logged when this Tracer was created.
  nlohmann::json config_json() const;
};

}  // namespace tracing
}  // namespace datadog
