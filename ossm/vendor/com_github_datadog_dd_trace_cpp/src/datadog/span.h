#pragma once

// This component defines a class, `Span`, that represents an extent of time in
// which some operation of interest occurs, such as an RPC request, database
// query, calculation, etc.
//
// `Span` objects are created by calling member functions on `Tracer` or on
// another `Span` object.  They are not instantiated directly.
//
// A `Span` has a start time, an end time, and a name (sometimes called its
// "operation name").  A span is associated with a service, a resource (such as
// the URL endpoint in an HTTP request), and arbitrary key/value string pairs
// known as tags.
//
// A `Span` can have at most one parent and can have zero or more children. The
// operation that a `Span` represents is a subtask of the operation that its
// parent represents, and the children of a `Span` represent subtasks of its
// operation.
//
// For example, an HTTP server might create a `Span` for each request processed.
// The `Span` begins when the server begins reading the request, and ends when
// the server has finished writing the response or reporting an error.  The
// first child of the request span might represent the reading and parsing of
// the HTTP request's headers.  The second child of the request span might
// represent the dispatch of the request handling to an endpoint-specific
// handler.  That child might itself have children, such as a database query or
// a request to an authentication service.
//
// The complete set of spans that are related to each other via the parent/child
// relationship is called a trace.
//
// A trace can extend across processes and networks via trace context
// propagation.  A `Span` can be _extracted_ from its external parent via
// `Tracer::extract_span`, and a `Span` can be _injected_ via `Span::inject`
// into an outside context from which its external children might be extracted.
//
// If an error occurs during the operation that a span represents, the error can
// be noted in the span via the `set_error` family of member functions.
//
// A `Span` is finished when it is destroyed.  The end time can be overridden
// via the `set_end_time` member function prior to the span's destruction.

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>

#include "clock.h"
#include "error.h"
#include "optional.h"
#include "string_view.h"
#include "trace_id.h"

namespace datadog {
namespace tracing {

struct InjectionOptions;
class DictReader;
class DictWriter;
struct SpanConfig;
struct SpanData;
class TraceSegment;

class Span {
  std::shared_ptr<TraceSegment> trace_segment_;
  SpanData* data_;
  std::function<std::uint64_t()> generate_span_id_;
  Clock clock_;
  Optional<std::chrono::steady_clock::time_point> end_time_;
  mutable bool expecting_delegated_sampling_decision_;

 public:
  // Create a span whose properties are stored in the specified `data`, that is
  // associated with the specified `trace_segment`, that uses the specified
  // `generate_span_id` to generate IDs of child spans, and that uses the
  // specified `clock` to determine start and end times.
  Span(SpanData* data, const std::shared_ptr<TraceSegment>& trace_segment,
       const std::function<std::uint64_t()>& generate_span_id,
       const Clock& clock);
  Span(const Span&) = delete;
  Span(Span&&) = default;
  Span& operator=(Span&&) = delete;
  Span& operator=(const Span&) = delete;

  // Finish this span and submit it to the associated trace segment.  If
  // `set_end_time` has not been called on this span, then set this span's end
  // time to the current time.
  // If this span was moved-from, then the destructor has no effect aside from
  // destroying data members.
  ~Span();

  // Return a span that is a child of this span.  Use the optionally specified
  // `config` to determine the properties of the child span.  If `config` is not
  // specified, then the child span's properties are determined by the
  // `SpanDefaults` that were used to configure the `Tracer` to which this span
  // is related.  The child span's start time is the current time unless
  // overridden in `config`.
  Span create_child(const SpanConfig& config) const;
  Span create_child() const;

  // Return this span's ID (span ID).
  std::uint64_t id() const;
  // Return the ID of the trace of which this span is a part.
  TraceID trace_id() const;
  // Return the ID of this span's parent span, or return null if this span has
  // no parent.
  Optional<std::uint64_t> parent_id() const;
  // Return the start time of this span.
  TimePoint start_time() const;
  // Return whether this span has been marked as an error having occurred during
  // its extent.
  bool error() const;
  // Return the name of the service associated with this span, e.g.
  // "ingress-nginx-useast1".
  const std::string& service_name() const;
  // Return the type of the service associated with this span, e.g. "web".
  const std::string& service_type() const;
  // Return the name of the operation associated with the operation that this
  // span represents, e.g. "handle.request", "execute.query", or "healthcheck".
  const std::string& name() const;
  // Return the name of the resource associated with the operation that this
  // span represents, e.g. "/api/v1/info" or "select count(*) from users".
  const std::string& resource_name() const;

  // Return the value of the tag having the specified `name`, or return null if
  // there is no such tag.
  Optional<StringView> lookup_tag(StringView name) const;
  // Return the value of the metric having the specified `name`, or return null
  // if there is no such metric.
  Optional<double> lookup_metric(StringView name) const;
  // Overwrite the tag having the specified `name` so that it has the specified
  // `value`, or create a new tag.
  void set_tag(StringView name, StringView value);
  // Overwrite the metric having the specified `name` so that it has the
  // specified `value`, or create a new metric.
  void set_metric(StringView name, double value);
  // Delete the tag having the specified `name` if it exists.
  void remove_tag(StringView name);
  // Delete the metric having the specified `name` if it exists.
  void remove_metric(StringView name);
  // Set the name of the service associated with this span, e.g.
  // "ingress-nginx-useast1".
  void set_service_name(StringView);
  // Set the type of the service associated with this span, e.g. "web".
  void set_service_type(StringView);
  // Set the name of the operation that this span represents, e.g.
  // "handle.request", "execute.query", or "healthcheck".
  void set_name(StringView);
  // Set the name of the resource associated with the operation that this span
  // represents, e.g. "/api/v1/info" or "select count(*) from users".
  void set_resource_name(StringView);
  // Set whether an error occurred during the extent of this span.  If `false`,
  // then error-related tags will be removed from this span as well.
  void set_error(bool);
  // Associate a message with the error that occurred during the extent of this
  // span.  This also has the effect of calling `set_error(true)`.
  void set_error_message(StringView);
  // Associate an error type with the error that occurred during the extent of
  // this span.  This also has the effect of calling `set_error(true)`.
  void set_error_type(StringView);
  // Associate a call stack with the error that occurred during the extent of
  // this span.  This also has the effect of calling `set_error(true)`.
  void set_error_stack(StringView);
  // Set end time of this span.  Doing so will override the default behavior of
  // using the current time in the destructor.
  void set_end_time(std::chrono::steady_clock::time_point);

  // Write information about this span and its trace into the specified `writer`
  // using all of the configured injection propagation styles.
  void inject(DictWriter& writer) const;
  void inject(DictWriter& writer, const InjectionOptions& options) const;

  // If this span is expecting a sampling decision that it previously delegated,
  // then extract a sampling decision from the specified `reader`. Return an
  // error if a sampling decision is present in `reader` but is invalid. Return
  // success otherwise. The trace segment associated with this span might adopt
  // the sampling decision from `reader`.
  Expected<void> read_sampling_delegation_response(const DictReader& reader);

  // Return a reference to this span's trace segment.  The trace segment has
  // member functions that affect the trace as a whole, such as
  // `TraceSegment::override_sampling_priority`.
  TraceSegment& trace_segment();
  const TraceSegment& trace_segment() const;
};

}  // namespace tracing
}  // namespace datadog
