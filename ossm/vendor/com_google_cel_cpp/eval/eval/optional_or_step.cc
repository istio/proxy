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

#include "eval/eval/optional_or_step.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "common/casting.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/eval/jump_step.h"
#include "internal/status_macros.h"
#include "runtime/internal/errors.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::As;
using ::cel::ErrorValue;
using ::cel::InstanceOf;
using ::cel::OptionalValue;
using ::cel::UnknownValue;
using ::cel::Value;
using ::cel::runtime_internal::CreateNoMatchingOverloadError;

enum class OptionalOrKind { kOrOptional, kOrValue };

ErrorValue MakeNoOverloadError(OptionalOrKind kind) {
  switch (kind) {
    case OptionalOrKind::kOrOptional:
      return ErrorValue(CreateNoMatchingOverloadError("or"));
    case OptionalOrKind::kOrValue:
      return ErrorValue(CreateNoMatchingOverloadError("orValue"));
  }

  ABSL_UNREACHABLE();
}

// Implements short-circuiting for optional.or.
// Expected layout if short-circuiting enabled:
//
// +--------+-----------------------+-------------------------------+
// |   idx  |         Step          |   Stack After                 |
// +--------+-----------------------+-------------------------------+
// |    1   |<optional target expr> | OptionalValue                 |
// +--------+-----------------------+-------------------------------+
// |    2   | Jump to 5 if present  | OptionalValue                 |
// +--------+-----------------------+-------------------------------+
// |    3   | <alternative expr>    | OptionalValue, OptionalValue  |
// +--------+-----------------------+-------------------------------+
// |    4   | optional.or           | OptionalValue                 |
// +--------+-----------------------+-------------------------------+
// |    5   | <rest>                | ...                           |
// +--------------------------------+-------------------------------+
//
// If implementing the orValue variant, the jump step handles unwrapping (
// getting the result of optional.value())
class OptionalHasValueJumpStep final : public JumpStepBase {
 public:
  OptionalHasValueJumpStep(int64_t expr_id, OptionalOrKind kind)
      : JumpStepBase({}, expr_id), kind_(kind) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    if (!frame->value_stack().HasEnough(1)) {
      return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
    }
    const auto& value = frame->value_stack().Peek();
    auto optional_value = As<OptionalValue>(value);
    // We jump if the receiver is `optional_type` which has a value or the
    // receiver is an error/unknown. Unlike `_||_` we are not commutative. If
    // we run into an error/unknown, we skip the `else` branch.
    const bool should_jump =
        (optional_value.has_value() && optional_value->HasValue()) ||
        (!optional_value.has_value() && (cel::InstanceOf<ErrorValue>(value) ||
                                         cel::InstanceOf<UnknownValue>(value)));
    if (should_jump) {
      if (kind_ == OptionalOrKind::kOrValue && optional_value.has_value()) {
        frame->value_stack().PopAndPush(optional_value->Value());
      }
      return Jump(frame);
    }
    return absl::OkStatus();
  }

 private:
  const OptionalOrKind kind_;
};

class OptionalOrStep : public ExpressionStepBase {
 public:
  explicit OptionalOrStep(int64_t expr_id, OptionalOrKind kind)
      : ExpressionStepBase(expr_id), kind_(kind) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  const OptionalOrKind kind_;
};

// Shared implementation for optional or.
//
// If return value is Ok, the result is assigned to the result reference
// argument.
absl::Status EvalOptionalOr(OptionalOrKind kind, const Value& lhs,
                            const Value& rhs, const AttributeTrail& lhs_attr,
                            const AttributeTrail& rhs_attr, Value& result,
                            AttributeTrail& result_attr) {
  if (InstanceOf<ErrorValue>(lhs) || InstanceOf<UnknownValue>(lhs)) {
    result = lhs;
    result_attr = lhs_attr;
    return absl::OkStatus();
  }

  auto lhs_optional_value = As<OptionalValue>(lhs);
  if (!lhs_optional_value.has_value()) {
    result = MakeNoOverloadError(kind);
    result_attr = AttributeTrail();
    return absl::OkStatus();
  }

  if (lhs_optional_value->HasValue()) {
    if (kind == OptionalOrKind::kOrValue) {
      result = lhs_optional_value->Value();
    } else {
      result = lhs;
    }
    result_attr = lhs_attr;
    return absl::OkStatus();
  }

  if (kind == OptionalOrKind::kOrOptional && !InstanceOf<ErrorValue>(rhs) &&
      !InstanceOf<UnknownValue>(rhs) && !InstanceOf<OptionalValue>(rhs)) {
    result = MakeNoOverloadError(kind);
    result_attr = AttributeTrail();
    return absl::OkStatus();
  }

  result = rhs;
  result_attr = rhs_attr;
  return absl::OkStatus();
}

absl::Status OptionalOrStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(2)) {
    return absl::InternalError("Value stack underflow");
  }

  absl::Span<const Value> args = frame->value_stack().GetSpan(2);
  absl::Span<const AttributeTrail> args_attr =
      frame->value_stack().GetAttributeSpan(2);

  Value result;
  AttributeTrail result_attr;
  CEL_RETURN_IF_ERROR(EvalOptionalOr(kind_, args[0], args[1], args_attr[0],
                                     args_attr[1], result, result_attr));

  frame->value_stack().PopAndPush(2, std::move(result), std::move(result_attr));
  return absl::OkStatus();
}

class ExhaustiveDirectOptionalOrStep : public DirectExpressionStep {
 public:
  ExhaustiveDirectOptionalOrStep(
      int64_t expr_id, std::unique_ptr<DirectExpressionStep> optional,
      std::unique_ptr<DirectExpressionStep> alternative, OptionalOrKind kind)

      : DirectExpressionStep(expr_id),
        kind_(kind),
        optional_(std::move(optional)),
        alternative_(std::move(alternative)) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, Value& result,
                        AttributeTrail& attribute) const override;

 private:
  OptionalOrKind kind_;
  std::unique_ptr<DirectExpressionStep> optional_;
  std::unique_ptr<DirectExpressionStep> alternative_;
};

absl::Status ExhaustiveDirectOptionalOrStep::Evaluate(
    ExecutionFrameBase& frame, Value& result, AttributeTrail& attribute) const {
  CEL_RETURN_IF_ERROR(optional_->Evaluate(frame, result, attribute));
  Value rhs;
  AttributeTrail rhs_attr;
  CEL_RETURN_IF_ERROR(alternative_->Evaluate(frame, rhs, rhs_attr));
  CEL_RETURN_IF_ERROR(EvalOptionalOr(kind_, result, rhs, attribute, rhs_attr,
                                     result, attribute));
  return absl::OkStatus();
}

class DirectOptionalOrStep : public DirectExpressionStep {
 public:
  DirectOptionalOrStep(int64_t expr_id,
                       std::unique_ptr<DirectExpressionStep> optional,
                       std::unique_ptr<DirectExpressionStep> alternative,
                       OptionalOrKind kind)

      : DirectExpressionStep(expr_id),
        kind_(kind),
        optional_(std::move(optional)),
        alternative_(std::move(alternative)) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, Value& result,
                        AttributeTrail& attribute) const override;

 private:
  OptionalOrKind kind_;
  std::unique_ptr<DirectExpressionStep> optional_;
  std::unique_ptr<DirectExpressionStep> alternative_;
};

absl::Status DirectOptionalOrStep::Evaluate(ExecutionFrameBase& frame,
                                            Value& result,
                                            AttributeTrail& attribute) const {
  CEL_RETURN_IF_ERROR(optional_->Evaluate(frame, result, attribute));

  if (InstanceOf<UnknownValue>(result) || InstanceOf<ErrorValue>(result)) {
    // Forward the lhs error instead of attempting to evaluate the alternative
    // (unlike CEL's commutative logic operators).
    return absl::OkStatus();
  }

  auto optional_value = As<OptionalValue>(static_cast<const Value&>(result));
  if (!optional_value.has_value()) {
    result = MakeNoOverloadError(kind_);
    return absl::OkStatus();
  }

  if (optional_value->HasValue()) {
    if (kind_ == OptionalOrKind::kOrValue) {
      result = optional_value->Value();
    }
    return absl::OkStatus();
  }

  CEL_RETURN_IF_ERROR(alternative_->Evaluate(frame, result, attribute));

  // If optional.or check that rhs is an optional.
  //
  // Otherwise, we don't know what type to expect so can't check anything.
  if (kind_ == OptionalOrKind::kOrOptional) {
    if (!InstanceOf<OptionalValue>(result) && !InstanceOf<ErrorValue>(result) &&
        !InstanceOf<UnknownValue>(result)) {
      result = MakeNoOverloadError(kind_);
    }
  }

  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<JumpStepBase>> CreateOptionalHasValueJumpStep(
    bool or_value, int64_t expr_id) {
  return std::make_unique<OptionalHasValueJumpStep>(
      expr_id,
      or_value ? OptionalOrKind::kOrValue : OptionalOrKind::kOrOptional);
}

std::unique_ptr<ExpressionStep> CreateOptionalOrStep(bool is_or_value,
                                                     int64_t expr_id) {
  return std::make_unique<OptionalOrStep>(
      expr_id,
      is_or_value ? OptionalOrKind::kOrValue : OptionalOrKind::kOrOptional);
}

std::unique_ptr<DirectExpressionStep> CreateDirectOptionalOrStep(
    int64_t expr_id, std::unique_ptr<DirectExpressionStep> optional,
    std::unique_ptr<DirectExpressionStep> alternative, bool is_or_value,
    bool short_circuiting) {
  auto kind =
      is_or_value ? OptionalOrKind::kOrValue : OptionalOrKind::kOrOptional;
  if (short_circuiting) {
    return std::make_unique<DirectOptionalOrStep>(expr_id, std::move(optional),
                                                  std::move(alternative), kind);
  } else {
    return std::make_unique<ExhaustiveDirectOptionalOrStep>(
        expr_id, std::move(optional), std::move(alternative), kind);
  }
}

}  // namespace google::api::expr::runtime
