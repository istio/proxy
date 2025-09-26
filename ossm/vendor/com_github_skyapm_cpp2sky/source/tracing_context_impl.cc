// Copyright 2020 SkyAPM

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "source/tracing_context_impl.h"

#include <string>

#include "absl/strings/string_view.h"
#include "cpp2sky/exception.h"
#include "cpp2sky/time.h"
#include "language-agent/Tracing.pb.h"
#include "source/utils/base64.h"
#include "source/utils/random_generator.h"

namespace cpp2sky {

TracingSpanImpl::TracingSpanImpl(int32_t span_id,
                                 TracingContextImpl& parent_tracing_context)
    : span_store_(
          *parent_tracing_context.segment_store_.mutable_spans()->Add()) {
  span_store_.set_spanid(span_id);
  // Default component id for historical reason.
  span_store_.set_componentid(9000);
}

void TracingSpanImpl::addLogImpl(absl::string_view key, absl::string_view value,
                                 int64_t timestamp) {
  assert(!finished_);
  auto* l = span_store_.add_logs();
  l->set_time(timestamp);
  auto* e = l->add_data();
  e->set_key(std::string(key));
  e->set_value(std::string(value));
}

void TracingSpanImpl::addLog(absl::string_view key, absl::string_view value) {
  addLogImpl(key, value, TimePoint<SystemTime>().fetch());
}

void TracingSpanImpl::addLog(absl::string_view key, absl::string_view value,
                             TimePoint<SystemTime> current_time) {
  addLogImpl(key, value, current_time.fetch());
}

void TracingSpanImpl::addLog(absl::string_view key, absl::string_view value,
                             TimePoint<SteadyTime> current_time) {
  addLogImpl(key, value, current_time.fetch());
}

void TracingSpanImpl::addTag(absl::string_view key, absl::string_view value) {
  assert(!finished_);
  auto* kv = span_store_.add_tags();
  kv->set_key(std::string(key));
  kv->set_value(std::string(value));
}

void TracingSpanImpl::startSpanImpl(absl::string_view operation_name,
                                    int64_t timestamp) {
  span_store_.set_operationname(std::string(operation_name));
  span_store_.set_starttime(timestamp);
}

void TracingSpanImpl::startSpan(absl::string_view operation_name) {
  startSpanImpl(operation_name, TimePoint<SystemTime>().fetch());
}

void TracingSpanImpl::startSpan(absl::string_view operation_name,
                                TimePoint<SystemTime> current_time) {
  startSpanImpl(operation_name, current_time.fetch());
}

void TracingSpanImpl::startSpan(absl::string_view operation_name,
                                TimePoint<SteadyTime> current_time) {
  startSpanImpl(operation_name, current_time.fetch());
}

void TracingSpanImpl::endSpanImpl(int64_t timestamp) {
  assert(!finished_);
  span_store_.set_endtime(timestamp);
  finished_ = true;
}

void TracingSpanImpl::endSpan() {
  endSpanImpl(TimePoint<SystemTime>().fetch());
}

void TracingSpanImpl::endSpan(TimePoint<SystemTime> current_time) {
  endSpanImpl(current_time.fetch());
}

void TracingSpanImpl::endSpan(TimePoint<SteadyTime> current_time) {
  endSpanImpl(current_time.fetch());
}

void TracingSpanImpl::setComponentId(int32_t component_id) {
  assert(!finished_);

  // Component ID is reserved on Skywalking spec.
  // For more details here:
  // https://github.com/apache/skywalking/blob/master/docs/en/guides/Component-library-settings.md
  span_store_.set_componentid(component_id);
}

void TracingSpanImpl::setOperationName(absl::string_view name) {
  assert(!finished_);
  span_store_.set_operationname(std::string(name));
}

void TracingSpanImpl::addSegmentRef(const SpanContext& span_context) {
  // TODO(shikugawa): cpp2sky only supports cross process propagation right now.
  // So It is correct to specify this.
  auto* entry = span_store_.add_refs();

  entry->set_reftype(skywalking::v3::RefType::CrossProcess);
  entry->set_traceid(span_context.traceId());
  entry->set_parenttracesegmentid(span_context.traceSegmentId());
  entry->set_parentservice(span_context.service());
  entry->set_parentserviceinstance(span_context.serviceInstance());
  entry->set_parentspanid(span_context.spanId());
  entry->set_parentendpoint(span_context.endpoint());
  entry->set_networkaddressusedatpeer(span_context.targetAddress());
}

TracingContextImpl::TracingContextImpl(
    const std::string& service_name, const std::string& instance_name,
    SpanContextSharedPtr parent_span_context,
    SpanContextExtensionSharedPtr parent_ext_span_context,
    RandomGenerator& random)
    : parent_span_context_(std::move(parent_span_context)),
      parent_ext_span_context_(std::move(parent_ext_span_context)) {
  segment_store_.set_traceid(
      parent_span_context_ ? parent_span_context_->traceId() : random.uuid());
  segment_store_.set_tracesegmentid(random.uuid());
  segment_store_.set_service(service_name);
  segment_store_.set_serviceinstance(instance_name);
}

TracingContextImpl::TracingContextImpl(const std::string& service_name,
                                       const std::string& instance_name,
                                       RandomGenerator& random)
    : TracingContextImpl(service_name, instance_name, nullptr, nullptr,
                         random) {}

TracingContextImpl::TracingContextImpl(const std::string& service_name,
                                       const std::string& instance_name,
                                       SpanContextSharedPtr parent_span_context,
                                       RandomGenerator& random)
    : TracingContextImpl(service_name, instance_name,
                         std::move(parent_span_context), nullptr, random) {}

TracingSpanSharedPtr TracingContextImpl::createExitSpan(
    TracingSpanSharedPtr parent_span) {
  auto current_span = createSpan();
  current_span->setParentSpanId(parent_span->spanId());
  current_span->setSpanType(skywalking::v3::SpanType::Exit);
  return current_span;
}

TracingSpanSharedPtr TracingContextImpl::createEntrySpan() {
  if (!spans_.empty()) {
    return nullptr;
  }

  auto current_span = createSpan();
  current_span->setParentSpanId(-1);
  current_span->setSpanType(skywalking::v3::SpanType::Entry);

  if (parent_span_context_ != nullptr) {
    current_span->addSegmentRef(*parent_span_context_);
  }

  return current_span;
}

absl::optional<std::string> TracingContextImpl::createSW8HeaderValue(
    const absl::string_view target_address) {
  auto target_span = spans_.back();
  if (target_span->spanType() != skywalking::v3::SpanType::Exit) {
    return absl::nullopt;
  }
  return encodeSpan(target_span, target_address);
}

std::string TracingContextImpl::encodeSpan(
    TracingSpanSharedPtr parent_span, const absl::string_view target_address) {
  assert(parent_span);
  std::string header_value;

  auto parent_spanid = std::to_string(parent_span->spanId());
  auto endpoint = spans_.front()->operationName();

  // always send to OAP
  header_value += "1-";
  header_value += Base64::encode(segment_store_.traceid()) + "-";
  header_value += Base64::encode(segment_store_.tracesegmentid()) + "-";
  header_value += parent_spanid + "-";
  header_value += Base64::encode(segment_store_.service()) + "-";
  header_value += Base64::encode(segment_store_.serviceinstance()) + "-";
  header_value += Base64::encode(endpoint.data(), endpoint.size()) + "-";
  header_value +=
      Base64::encode(target_address.data(), target_address.length());

  return header_value;
}

TracingSpanSharedPtr TracingContextImpl::createSpan() {
  auto current_span = std::make_shared<TracingSpanImpl>(spans_.size(), *this);

  // It supports only HTTP request tracing.
  current_span->setSpanLayer(skywalking::v3::SpanLayer::Http);
  if (should_skip_analysis_) {
    current_span->setSkipAnalysis();
  }

  spans_.push_back(current_span);
  return current_span;
}

skywalking::v3::SegmentObject TracingContextImpl::createSegmentObject() {
  spans_.clear();
  return std::move(segment_store_);
}

bool TracingContextImpl::readyToSend() {
  for (const auto& span : spans_) {
    if (!span->finished()) {
      return false;
    }
  }
  return true;
}

std::string TracingContextImpl::logMessage(absl::string_view message) const {
  std::string output = message.data();
  output += "\", \"SW_CTX\": [";
  output += "\"" + segment_store_.service() + "\",";
  output += "\"" + segment_store_.serviceinstance() + "\",";
  output += "\"" + segment_store_.traceid() + "\",";
  output += "\"" + segment_store_.tracesegmentid() + "\",";

  if (!spans_.empty()) {
    output += "\"" + std::to_string(spans_.back()->spanId()) + "\"]}";
  } else {
    output += "\"-1\"]}";
  }

  return output;
}

TracingContextFactory::TracingContextFactory(const TracerConfig& config)
    : service_name_(config.service_name()),
      instance_name_(config.instance_name()) {}

TracingContextSharedPtr TracingContextFactory::create() {
  return std::make_shared<TracingContextImpl>(service_name_, instance_name_,
                                              random_generator_);
}

TracingContextSharedPtr TracingContextFactory::create(
    SpanContextSharedPtr span_context) {
  return std::make_shared<TracingContextImpl>(service_name_, instance_name_,
                                              span_context, random_generator_);
}

TracingContextSharedPtr TracingContextFactory::create(
    SpanContextSharedPtr span_context,
    SpanContextExtensionSharedPtr ext_span_context) {
  auto context = std::make_shared<TracingContextImpl>(
      service_name_, instance_name_, span_context, ext_span_context,
      random_generator_);
  if (ext_span_context->tracingMode() == TracingMode::Skip) {
    context->setSkipAnalysis();
  }
  return context;
}

}  // namespace cpp2sky
