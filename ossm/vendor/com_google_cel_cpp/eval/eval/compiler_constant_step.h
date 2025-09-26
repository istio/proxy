// Copyright 2023 Google LLC
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
#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_COMPILER_CONSTANT_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_COMPILER_CONSTANT_STEP_H_

#include <cstdint>
#include <utility>

#include "absl/status/status.h"
#include "common/native_type.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"

namespace google::api::expr::runtime {

// DirectExpressionStep implementation that simply assigns a constant value.
//
// Overrides NativeTypeId() allow the FlatExprBuilder and extensions to
// inspect the underlying value.
class DirectCompilerConstantStep : public DirectExpressionStep {
 public:
  DirectCompilerConstantStep(cel::Value value, int64_t expr_id)
      : DirectExpressionStep(expr_id), value_(std::move(value)) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, cel::Value& result,
                        AttributeTrail& attribute) const override;

  cel::NativeTypeId GetNativeTypeId() const override {
    return cel::NativeTypeId::For<DirectCompilerConstantStep>();
  }

  const cel::Value& value() const { return value_; }

 private:
  cel::Value value_;
};

// ExpressionStep implementation that simply pushes a constant value on the
// stack.
//
// Overrides NativeTypeId ()o allow the FlatExprBuilder and extensions to
// inspect the underlying value.
class CompilerConstantStep : public ExpressionStepBase {
 public:
  CompilerConstantStep(cel::Value value, int64_t expr_id, bool comes_from_ast)
      : ExpressionStepBase(expr_id, comes_from_ast), value_(std::move(value)) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

  cel::NativeTypeId GetNativeTypeId() const override {
    return cel::NativeTypeId::For<CompilerConstantStep>();
  }

  const cel::Value& value() const { return value_; }

 private:
  cel::Value value_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_COMPILER_CONSTANT_STEP_H_
