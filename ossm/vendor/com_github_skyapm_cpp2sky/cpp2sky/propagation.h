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

#include <memory>

#include "absl/strings/string_view.h"

namespace cpp2sky {

class SpanContext {
 public:
  virtual ~SpanContext() = default;

  /**
   * Get the status of sample on SKyWalking.
   * It indicates whether current context should send or not.
   */
  virtual bool sample() const = 0;

  /**
   * Get parent's trace ID. This value must be unique globally.
   */
  virtual const std::string& traceId() const = 0;

  /**
   * Get trace parent's segment ID. This value must be unique globally.
   */
  virtual const std::string& traceSegmentId() const = 0;

  /**
   * Get parent's span ID. This span id points to the parent span in parent
   * trace segment.
   */
  virtual int32_t spanId() const = 0;

  /**
   * Get parent's service name.
   */
  virtual const std::string& service() const = 0;

  /**
   * Get parent's service instance name.
   */
  virtual const std::string& serviceInstance() const = 0;

  /**
   * Get endpoint. Operation Name of the first entry span in the parent
   * segment.
   */
  virtual const std::string& endpoint() const = 0;

  /**
   * Get target address. The network address used at client side to access
   * this target service.
   */
  virtual const std::string& targetAddress() const = 0;
};

using SpanContextSharedPtr = std::shared_ptr<SpanContext>;

enum class TracingMode {
  Default,
  // It represents all spans generated in this context should skip
  // analysis.
  Skip
};

class SpanContextExtension {
 public:
  virtual ~SpanContextExtension() = default;

  virtual TracingMode tracingMode() const = 0;
};

using SpanContextExtensionSharedPtr = std::shared_ptr<SpanContextExtension>;

SpanContextSharedPtr createSpanContext(absl::string_view ctx);

SpanContextExtensionSharedPtr createSpanContextExtension(absl::string_view ctx);

}  // namespace cpp2sky
