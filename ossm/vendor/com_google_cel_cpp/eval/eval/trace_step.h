// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_TRACE_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_TRACE_STEP_H_

#include <memory>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/optional.h"
#include "common/native_type.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "internal/status_macros.h"
namespace google::api::expr::runtime {

// A decorator that implements tracing for recursively evaluated CEL
// expressions.
//
// Allows inspection for extensions to extract the wrapped expression.
class TraceStep : public DirectExpressionStep {
 public:
  explicit TraceStep(std::unique_ptr<DirectExpressionStep> expression)
      : DirectExpressionStep(-1), expression_(std::move(expression)) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, cel::Value& result,
                        AttributeTrail& trail) const override {
    CEL_RETURN_IF_ERROR(expression_->Evaluate(frame, result, trail));
    if (!frame.callback()) {
      return absl::OkStatus();
    }
    return frame.callback()(expression_->expr_id(), result,
                            frame.descriptor_pool(), frame.message_factory(),
                            frame.arena());
  }

  cel::NativeTypeId GetNativeTypeId() const override {
    return cel::NativeTypeId::For<TraceStep>();
  }

  absl::optional<std::vector<const DirectExpressionStep*>> GetDependencies()
      const override {
    return {{expression_.get()}};
  }

  absl::optional<std::vector<std::unique_ptr<DirectExpressionStep>>>
  ExtractDependencies() override {
    std::vector<std::unique_ptr<DirectExpressionStep>> dependencies;
    dependencies.push_back(std::move(expression_));
    return dependencies;
  };

 private:
  std::unique_ptr<DirectExpressionStep> expression_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_TRACE_STEP_H_
