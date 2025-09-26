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

#include "absl/strings/string_view.h"
#include "cpp2sky/config.pb.h"
#include "cpp2sky/propagation.h"
#include "cpp2sky/tracing_context.h"
#include "source/utils/random_generator.h"

namespace cpp2sky {

class TracingContextImpl;

class TracingSpanImpl : public TracingSpan {
 public:
  TracingSpanImpl(int32_t span_id, TracingContextImpl& parent_tracing_context);

  skywalking::v3::SpanObject createSpanObject() override {
    // Create an copy of current span object. This is only be used for test for
    // now.
    return span_store_;
  }
  int32_t spanId() const override { return span_store_.spanid(); }
  int32_t parentSpanId() const override { return span_store_.parentspanid(); }
  int64_t startTime() const override { return span_store_.starttime(); }
  int64_t endTime() const override { return span_store_.endtime(); }
  absl::string_view peer() const override { return span_store_.peer(); }
  skywalking::v3::SpanType spanType() const override {
    return span_store_.spantype();
  }
  skywalking::v3::SpanLayer spanLayer() const override {
    return span_store_.spanlayer();
  }
  bool errorStatus() const override { return span_store_.iserror(); }
  bool skipAnalysis() const override { return span_store_.skipanalysis(); }
  int32_t componentId() const override { return span_store_.componentid(); }
  absl::string_view operationName() const override {
    return span_store_.operationname();
  }

  void setParentSpanId(int32_t span_id) override {
    assert(!finished_);
    span_store_.set_parentspanid(span_id);
  }
  void startSpan(absl::string_view operation_name) override;
  void startSpan(absl::string_view operation_name,
                 TimePoint<SystemTime> current_time) override;
  void startSpan(absl::string_view operation_name,
                 TimePoint<SteadyTime> current_time) override;
  void endSpan() override;
  void endSpan(TimePoint<SystemTime> current_time) override;
  void endSpan(TimePoint<SteadyTime> current_time) override;
  void setPeer(absl::string_view remote_address) override {
    assert(!finished_);
    span_store_.set_peer(std::string(remote_address));
  }
  void setSpanType(skywalking::v3::SpanType type) override {
    span_store_.set_spantype(type);
  }
  void setSpanLayer(skywalking::v3::SpanLayer layer) override {
    span_store_.set_spanlayer(layer);
  }
  void setErrorStatus() override { span_store_.set_iserror(true); }
  void setSkipAnalysis() override { span_store_.set_skipanalysis(true); }
  void addTag(absl::string_view key, absl::string_view value) override;
  void addLog(absl::string_view key, absl::string_view value) override;
  void addLog(absl::string_view key, absl::string_view value,
              TimePoint<SystemTime> current_time) override;
  void addLog(absl::string_view key, absl::string_view value,
              TimePoint<SteadyTime> current_time) override;
  void setComponentId(int32_t component_id) override;
  void setOperationName(absl::string_view name) override;
  void addSegmentRef(const SpanContext& span_context) override;
  bool finished() const override { return finished_; }

  void addLogImpl(absl::string_view key, absl::string_view value,
                  int64_t timestamp);
  void startSpanImpl(absl::string_view operation_name, int64_t timestamp);
  void endSpanImpl(int64_t timestamp);

 private:
  bool finished_ = false;

  // Parent segment owns all span objects and we only keep a ref in the tracing
  // span.
  skywalking::v3::SpanObject& span_store_;
};

class TracingContextImpl : public TracingContext {
 public:
  // This constructor is called when there is no parent SpanContext.
  TracingContextImpl(const std::string& service_name,
                     const std::string& instance_name, RandomGenerator& random);
  TracingContextImpl(const std::string& service_name,
                     const std::string& instance_name,
                     SpanContextSharedPtr parent_span_context,
                     RandomGenerator& random);
  TracingContextImpl(const std::string& service_name,
                     const std::string& instance_name,
                     SpanContextSharedPtr parent_span_context,
                     SpanContextExtensionSharedPtr parent_ext_span_context,
                     RandomGenerator& random);

  const std::string& traceId() const override {
    return segment_store_.traceid();
  }
  const std::string& traceSegmentId() const override {
    return segment_store_.tracesegmentid();
  }
  const std::string& service() const override {
    return segment_store_.service();
  }
  const std::string& serviceInstance() const override {
    return segment_store_.serviceinstance();
  }
  const std::list<TracingSpanSharedPtr>& spans() const override {
    return spans_;
  }
  SpanContextSharedPtr parentSpanContext() const override {
    return parent_span_context_;
  }
  SpanContextExtensionSharedPtr parentSpanContextExtension() const override {
    return parent_ext_span_context_;
  }

  TracingSpanSharedPtr createExitSpan(
      TracingSpanSharedPtr parent_span) override;

  TracingSpanSharedPtr createEntrySpan() override;
  absl::optional<std::string> createSW8HeaderValue(
      const absl::string_view target_address) override;
  skywalking::v3::SegmentObject createSegmentObject() override;
  void setSkipAnalysis() override { should_skip_analysis_ = true; }
  bool skipAnalysis() override { return should_skip_analysis_; }
  bool readyToSend() override;
  std::string logMessage(absl::string_view message) const override;

 private:
  friend class TracingSpanImpl;

  std::string encodeSpan(TracingSpanSharedPtr parent_span,
                         const absl::string_view target_address);
  TracingSpanSharedPtr createSpan();

  SpanContextSharedPtr parent_span_context_;
  SpanContextExtensionSharedPtr parent_ext_span_context_;

  std::list<TracingSpanSharedPtr> spans_;

  skywalking::v3::SegmentObject segment_store_;

  bool should_skip_analysis_ = false;
};

class TracingContextFactory {
 public:
  TracingContextFactory(const TracerConfig& config);

  TracingContextSharedPtr create();
  TracingContextSharedPtr create(SpanContextSharedPtr span_context);
  TracingContextSharedPtr create(
      SpanContextSharedPtr span_context,
      SpanContextExtensionSharedPtr ext_span_context);

 private:
  std::string service_name_;
  std::string instance_name_;
  RandomGeneratorImpl random_generator_;
};

}  // namespace cpp2sky
