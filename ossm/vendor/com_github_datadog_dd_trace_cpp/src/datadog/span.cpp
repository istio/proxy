#include "span.h"

#include <cassert>
#include <string>

#include "dict_writer.h"
#include "optional.h"
#include "span_config.h"
#include "span_data.h"
#include "string_view.h"
#include "tags.h"
#include "trace_segment.h"

namespace datadog {
namespace tracing {

Span::Span(SpanData* data, const std::shared_ptr<TraceSegment>& trace_segment,
           const std::function<std::uint64_t()>& generate_span_id,
           const Clock& clock)
    : trace_segment_(trace_segment),
      data_(data),
      generate_span_id_(generate_span_id),
      clock_(clock),
      expecting_delegated_sampling_decision_(false) {
  assert(trace_segment_);
  assert(data_);
  assert(generate_span_id_);
  assert(clock_);
}

Span::~Span() {
  if (!trace_segment_) {
    // We were moved from.
    return;
  }

  if (end_time_) {
    data_->duration = *end_time_ - data_->start.tick;
  } else {
    const auto now = clock_();
    data_->duration = now - data_->start;
  }

  trace_segment_->span_finished();
}

Span Span::create_child(const SpanConfig& config) const {
  auto span_data = std::make_unique<SpanData>();
  span_data->apply_config(trace_segment_->defaults(), config, clock_);
  span_data->trace_id = data_->trace_id;
  span_data->parent_id = data_->span_id;
  span_data->span_id = generate_span_id_();

  const auto span_data_ptr = span_data.get();
  trace_segment_->register_span(std::move(span_data));
  return Span(span_data_ptr, trace_segment_, generate_span_id_, clock_);
}

Span Span::create_child() const { return create_child(SpanConfig{}); }

void Span::inject(DictWriter& writer) const {
  expecting_delegated_sampling_decision_ =
      trace_segment_->inject(writer, *data_);
}

void Span::inject(DictWriter& writer, const InjectionOptions& options) const {
  expecting_delegated_sampling_decision_ =
      trace_segment_->inject(writer, *data_, options);
}

Expected<void> Span::read_sampling_delegation_response(
    const DictReader& reader) {
  if (!expecting_delegated_sampling_decision_) return {};

  expecting_delegated_sampling_decision_ = false;
  return trace_segment_->read_sampling_delegation_response(reader);
}

std::uint64_t Span::id() const { return data_->span_id; }

TraceID Span::trace_id() const { return data_->trace_id; }

Optional<std::uint64_t> Span::parent_id() const {
  if (data_->parent_id == 0) {
    return nullopt;
  }
  return data_->parent_id;
}

TimePoint Span::start_time() const { return data_->start; }

bool Span::error() const { return data_->error; }

const std::string& Span::service_name() const { return data_->service; }

const std::string& Span::service_type() const { return data_->service_type; }

const std::string& Span::name() const { return data_->name; }

const std::string& Span::resource_name() const { return data_->resource; }

Optional<StringView> Span::lookup_tag(StringView name) const {
  const auto found = data_->tags.find(std::string(name));
  if (found == data_->tags.end()) {
    return nullopt;
  }
  return found->second;
}

Optional<double> Span::lookup_metric(StringView name) const {
  const auto found = data_->numeric_tags.find(std::string(name));
  if (found == data_->numeric_tags.end()) {
    return nullopt;
  }
  return found->second;
}

void Span::set_tag(StringView name, StringView value) {
  data_->tags.insert_or_assign(std::string(name), std::string(value));
}

void Span::set_metric(StringView name, double value) {
  data_->numeric_tags.insert_or_assign(std::string(name), value);
}

void Span::remove_tag(StringView name) { data_->tags.erase(std::string(name)); }

void Span::remove_metric(StringView name) {
  data_->numeric_tags.erase(std::string(name));
}

void Span::set_service_name(StringView service) {
  assign(data_->service, service);
}

void Span::set_service_type(StringView type) {
  assign(data_->service_type, type);
}

void Span::set_resource_name(StringView resource) {
  assign(data_->resource, resource);
}

void Span::set_error(bool is_error) {
  data_->error = is_error;
  if (!is_error) {
    data_->tags.erase("error.message");
    data_->tags.erase("error.type");
  }
}

void Span::set_error_message(StringView message) {
  data_->error = true;
  data_->tags.insert_or_assign("error.message", std::string(message));
}

void Span::set_error_type(StringView type) {
  data_->error = true;
  data_->tags.insert_or_assign("error.type", std::string(type));
}

void Span::set_error_stack(StringView type) {
  data_->error = true;
  data_->tags.insert_or_assign("error.stack", std::string(type));
}

void Span::set_name(StringView value) { assign(data_->name, value); }

void Span::set_end_time(std::chrono::steady_clock::time_point end_time) {
  end_time_ = end_time;
}

TraceSegment& Span::trace_segment() { return *trace_segment_; }

const TraceSegment& Span::trace_segment() const { return *trace_segment_; }

}  // namespace tracing
}  // namespace datadog
