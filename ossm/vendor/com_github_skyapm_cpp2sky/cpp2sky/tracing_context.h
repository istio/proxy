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

#pragma once

#include <list>
#include <memory>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "cpp2sky/config.pb.h"
#include "cpp2sky/propagation.h"
#include "cpp2sky/time.h"
#include "language-agent/Tracing.pb.h"

namespace cpp2sky {

class TracingSpan {
 public:
  virtual ~TracingSpan() = default;

  /**
   * Generate Apache SkyWalking native span object from current segment span.
   */
  virtual skywalking::v3::SpanObject createSpanObject() = 0;

  /**
   * Get span ID.
   */
  virtual int32_t spanId() const = 0;

  /**
   * Get parent span ID.
   */
  virtual int32_t parentSpanId() const = 0;

  /**
   * Get start time.
   */
  virtual int64_t startTime() const = 0;

  /**
   * Get end time.
   */
  virtual int64_t endTime() const = 0;

  /**
   * Get peer address.
   */
  virtual absl::string_view peer() const = 0;

  /**
   * Get span type.
   */
  virtual skywalking::v3::SpanType spanType() const = 0;

  /**
   * Get span layer.
   */
  virtual skywalking::v3::SpanLayer spanLayer() const = 0;

  /**
   * Get error occurred or not.
   */
  virtual bool errorStatus() const = 0;

  /**
   * Enable to skip analysis or not.
   */
  virtual bool skipAnalysis() const = 0;

  /**
   * Get component ID.
   */
  virtual int32_t componentId() const = 0;

  /**
   * Get operation name.
   */
  virtual absl::string_view operationName() const = 0;

  /**
   * Set parent span ID of this span.
   */
  virtual void setParentSpanId(int32_t span_id) = 0;

  /**
   * Set start time to calculate execution time.
   */
  virtual void startSpan(absl::string_view operation_name) = 0;
  virtual void startSpan(absl::string_view operation_name,
                         TimePoint<SystemTime> current_time) = 0;
  virtual void startSpan(absl::string_view operation_name,
                         TimePoint<SteadyTime> current_time) = 0;

  /**
   * Set end time to calculate execution time.
   */
  virtual void endSpan() = 0;
  virtual void endSpan(TimePoint<SystemTime> current_time) = 0;
  virtual void endSpan(TimePoint<SteadyTime> current_time) = 0;

  /**
   * Set peer address for this span (lvalue)
   */
  virtual void setPeer(absl::string_view remote_address) = 0;

  /**
   * Set span type. Entry or Exit. Entry span means origin span which doesn't
   * have parent span, like root node of span tree. Exit span has opposite
   * meaning, like leaf node of span tree.
   */
  virtual void setSpanType(skywalking::v3::SpanType type) = 0;

  /**
   * Set span layer. It supports only HTTP request tracing currently.
   */
  virtual void setSpanLayer(skywalking::v3::SpanLayer layer) = 0;

  /**
   * If error had caused on this span, This should be called.
   */
  virtual void setErrorStatus() = 0;

  /**
   * Determine whether to skip the analysis of this span. If we'd like to skip
   * analysis, this should be called.
   */
  virtual void setSkipAnalysis() = 0;

  /**
   * Set tag to current span.
   */
  virtual void addTag(absl::string_view key, absl::string_view value) = 0;

  /**
   * Add log related with current span.
   */
  virtual void addLog(absl::string_view key, absl::string_view value) = 0;
  virtual void addLog(absl::string_view key, absl::string_view value,
                      TimePoint<SystemTime> current_time) = 0;
  virtual void addLog(absl::string_view key, absl::string_view value,
                      TimePoint<SteadyTime> current_time) = 0;

  /**
   * Set component ID.
   */
  virtual void setComponentId(int32_t component_id) = 0;

  /**
   * Set operation name.
   */
  virtual void setOperationName(absl::string_view operation_name) = 0;

  /**
   * Add parent segment reference to current span.
   */
  virtual void addSegmentRef(const SpanContext& span_context) = 0;

  /**
   * This span had finished or not.
   */
  virtual bool finished() const = 0;
};

using TracingSpanSharedPtr = std::shared_ptr<TracingSpan>;

class TracingContext {
 public:
  virtual ~TracingContext() = default;

  /**
   * Get trace ID. This value must be unique globally.
   */
  virtual const std::string& traceId() const = 0;

  /**
   * Get trace segment ID. This value must be unique globally.
   */
  virtual const std::string& traceSegmentId() const = 0;

  /**
   * Get service name.
   */
  virtual const std::string& service() const = 0;

  /**
   * Get service instance name.
   */
  virtual const std::string& serviceInstance() const = 0;

  /**
   * Get spans generated by this segment context.
   */
  virtual const std::list<TracingSpanSharedPtr>& spans() const = 0;

  /**
   * Get span context which generated this segment context as parent.
   */
  virtual SpanContextSharedPtr parentSpanContext() const = 0;

  /**
   * Get span context extension which generated this segment context.
   */
  virtual SpanContextExtensionSharedPtr parentSpanContextExtension() const = 0;

  /**
   * Generate a segment span related with this segment context.
   * @param parent_span Parent span which is extracted from caller.
   */
  virtual TracingSpanSharedPtr createExitSpan(
      TracingSpanSharedPtr parent_span) = 0;

  /**
   * Generate root segment span, called once per workload.
   */
  virtual TracingSpanSharedPtr createEntrySpan() = 0;

  /**
   * Generate sw8 value to send SegmentRef.
   * @param target_address Target address to send request. For more detail:
   * https://github.com/apache/skywalking-data-collect-protocol/blob/master/language-agent/Tracing.proto#L97-L101
   */
  virtual absl::optional<std::string> createSW8HeaderValue(
      const absl::string_view target_address) = 0;

  /**
   * Generate Apache SkyWalking native segment object. This method **SHOULD**
   * only be called once.
   */
  virtual skywalking::v3::SegmentObject createSegmentObject() = 0;

  /**
   * If called, all spans belongs to this segment will be skipped analysis.
   */
  virtual void setSkipAnalysis() = 0;

  /**
   * Whether belonging span can be skipped analysis or not.
   */
  virtual bool skipAnalysis() = 0;

  /**
   * Determine whether to send this segment or not.
   */
  virtual bool readyToSend() = 0;

  /**
   * Get log message. Output value of this function is based on default cpp2sky
   * logging format following with any format extracted with
   * cpp2sky::logFormat().
   */
  virtual std::string logMessage(absl::string_view message) const = 0;
};

using TracingContextSharedPtr = std::shared_ptr<TracingContext>;

/**
 * RAII based span creation. It acquired then create new span with required
 * properties. The span wiil be closed and set end time when called destructor.
 */
class StartEntrySpan {
 public:
  StartEntrySpan(TracingContextSharedPtr tracing_context,
                 absl::string_view operation_name)
      : span_(tracing_context->createEntrySpan()) {
    span_->startSpan(operation_name.data());
  }

  ~StartEntrySpan() {
    // Span won't be released because the entity is holded by TracingContext.
    span_->endSpan();
  }

  TracingSpanSharedPtr get() { return span_; }

 private:
  TracingSpanSharedPtr span_;
};

class StartExitSpan {
 public:
  StartExitSpan(TracingContextSharedPtr tracing_context,
                TracingSpanSharedPtr parent_span,
                absl::string_view operation_name)
      : span_(tracing_context->createExitSpan(parent_span)) {
    span_->startSpan(operation_name.data());
  }

  ~StartExitSpan() {
    // Span won't be released because the entity is holded by TracingContext.
    span_->endSpan();
  }

  TracingSpanSharedPtr get() { return span_; }

 private:
  TracingSpanSharedPtr span_;
};

}  // namespace cpp2sky
