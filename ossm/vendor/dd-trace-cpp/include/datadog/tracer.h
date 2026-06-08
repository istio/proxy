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

#include "baggage.h"
#include "clock.h"
#include "expected.h"
#include "id_generator.h"
#include "optional.h"
#include "span.h"
#include "span_config.h"
#include "tracer_config.h"
#include "tracer_signature.h"

namespace datadog {
namespace tracing {

class ConfigManager;
class DictReader;
struct SpanConfig;
class TraceSampler;
class SpanSampler;
class IDGenerator;
class InMemoryFile;

class Tracer {
  std::shared_ptr<Logger> logger_;
  RuntimeID runtime_id_;
  TracerSignature signature_;
  std::shared_ptr<ConfigManager> config_manager_;
  std::shared_ptr<Collector> collector_;
  std::shared_ptr<SpanSampler> span_sampler_;
  std::shared_ptr<const IDGenerator> generator_;
  Clock clock_;
  std::vector<PropagationStyle> injection_styles_;
  std::vector<PropagationStyle> extraction_styles_;
  Optional<std::string> hostname_;
  std::size_t tags_header_max_size_;
  // Store the tracer configuration in an in-memory file, allowing it to be
  // read to determine if the process is instrumented with a tracer and to
  // retrieve relevant tracing information.
  std::shared_ptr<InMemoryFile> metadata_file_;
  Baggage::Options baggage_opts_;
  bool baggage_injection_enabled_;
  bool baggage_extraction_enabled_;
  bool tracing_enabled_;

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
  // If there is no span to extract, or if an error occurs during extraction,
  // then return a span that is the root of a new trace (see `create_span`).
  // Optionally specify a `config` indicating the attributes of the span.
  Span extract_or_create_span(const DictReader& reader);
  Span extract_or_create_span(const DictReader& reader,
                              const SpanConfig& config);

  // Create a baggage.
  Baggage create_baggage();

  // Return the extracted baggage from the specified `reader`.
  // An error is returned if an error occurs during extraction.
  Expected<Baggage, Baggage::Error> extract_baggage(const DictReader& reader);

  // Return the extracted baggage from the specified `reader`, or an empty
  // baggage is there is no baggage to extract if an error occurs during
  // extraction.
  Baggage extract_or_create_baggage(const DictReader& reader);

  // Inject baggage into the specified `reader`.
  Expected<void> inject(const Baggage& baggage, DictWriter& writer);

  // Return a JSON object describing this Tracer's configuration. It is the
  // same JSON object that was logged when this Tracer was created.
  std::string config() const;

 private:
  void store_config();
};

}  // namespace tracing
}  // namespace datadog
