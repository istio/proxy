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
#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_DIRECT_EXPRESSION_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_DIRECT_EXPRESSION_STEP_H_

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/optional.h"
#include "common/native_type.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/evaluator_core.h"

namespace google::api::expr::runtime {

// Represents a directly evaluated CEL expression.
//
// Subexpressions assign to values on the C++ program stack and call their
// dependencies directly.
//
// This reduces the setup overhead for evaluation and minimizes value churn
// to / from a heap based value stack managed by the CEL runtime, but can't be
// used for arbitrarily nested expressions.
class DirectExpressionStep {
 public:
  explicit DirectExpressionStep(int64_t expr_id) : expr_id_(expr_id) {}
  DirectExpressionStep() : expr_id_(-1) {}

  virtual ~DirectExpressionStep() = default;

  int64_t expr_id() const { return expr_id_; }
  bool comes_from_ast() const { return expr_id_ >= 0; }

  virtual absl::Status Evaluate(ExecutionFrameBase& frame, cel::Value& result,
                                AttributeTrail& attribute) const = 0;

  // Return a type id for this node.
  //
  // Users must not make any assumptions about the type if the default value is
  // returned.
  virtual cel::NativeTypeId GetNativeTypeId() const {
    return cel::NativeTypeId();
  }

  // Implementations optionally support inspecting the program tree.
  virtual absl::optional<std::vector<const DirectExpressionStep*>>
  GetDependencies() const {
    return absl::nullopt;
  }

  // Implementations optionally support extracting the program tree.
  //
  // Extract prevents the callee from functioning, and is only intended for use
  // when replacing a given expression step.
  virtual absl::optional<std::vector<std::unique_ptr<DirectExpressionStep>>>
  ExtractDependencies() {
    return absl::nullopt;
  };

 protected:
  int64_t expr_id_;
};

// Wrapper for direct steps to work with the stack machine impl.
class WrappedDirectStep : public ExpressionStep {
 public:
  WrappedDirectStep(std::unique_ptr<DirectExpressionStep> impl, int64_t expr_id)
      : ExpressionStep(expr_id, false), impl_(std::move(impl)) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

  cel::NativeTypeId GetNativeTypeId() const override {
    return cel::NativeTypeId::For<WrappedDirectStep>();
  }

  const DirectExpressionStep* wrapped() const { return impl_.get(); }

 private:
  std::unique_ptr<DirectExpressionStep> impl_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_DIRECT_EXPRESSION_STEP_H_
