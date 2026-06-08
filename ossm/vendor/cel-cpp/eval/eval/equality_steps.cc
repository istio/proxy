// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_EQUALITY_STEPS_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_EQUALITY_STEPS_H_

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/builtins.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "internal/number.h"
#include "internal/status_macros.h"
#include "runtime/internal/errors.h"
#include "runtime/standard/equality_functions.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::BoolValue;
using ::cel::IntValue;
using ::cel::MapValue;
using ::cel::UintValue;
using ::cel::Value;

using ::cel::ValueKind;
using ::cel::internal::Number;
using ::cel::runtime_internal::ValueEqualImpl;

absl::StatusOr<Value> EvaluateEquality(
    ExecutionFrameBase& frame, const Value& lhs, const AttributeTrail& lhs_attr,
    const Value& rhs, const AttributeTrail& rhs_attr, bool negation) {
  if (lhs.IsError()) {
    return lhs;
  }

  if (rhs.IsError()) {
    return rhs;
  }

  if (frame.unknown_processing_enabled()) {
    auto accu = frame.attribute_utility().CreateAccumulator();
    accu.MaybeAdd(lhs, lhs_attr);
    accu.MaybeAdd(rhs, rhs_attr);
    if (!accu.IsEmpty()) {
      return std::move(accu).Build();
    }
  }

  CEL_ASSIGN_OR_RETURN(auto is_equal,
                       ValueEqualImpl(lhs, rhs, frame.descriptor_pool(),
                                      frame.message_factory(), frame.arena()));
  if (!is_equal.has_value()) {
    return cel::ErrorValue(cel::runtime_internal::CreateNoMatchingOverloadError(
        negation ? cel::builtin::kInequal : cel::builtin::kEqual));
  }
  return negation ? BoolValue(!*is_equal) : BoolValue(*is_equal);
}

class DirectEqualityStep : public DirectExpressionStep {
 public:
  explicit DirectEqualityStep(std::unique_ptr<DirectExpressionStep> lhs,
                              std::unique_ptr<DirectExpressionStep> rhs,
                              bool negation, int64_t expr_id)
      : DirectExpressionStep(expr_id),
        lhs_(std::move(lhs)),
        rhs_(std::move(rhs)),
        negation_(negation) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, Value& result,
                        AttributeTrail& attribute_trail) const override {
    AttributeTrail lhs_attr;
    CEL_RETURN_IF_ERROR(lhs_->Evaluate(frame, result, lhs_attr));

    Value rhs_result;
    AttributeTrail rhs_attr;
    CEL_RETURN_IF_ERROR(rhs_->Evaluate(frame, rhs_result, rhs_attr));
    CEL_ASSIGN_OR_RETURN(
        result, EvaluateEquality(frame, result, lhs_attr, rhs_result, rhs_attr,
                                 negation_));
    return absl::OkStatus();
  }

 private:
  std::unique_ptr<DirectExpressionStep> lhs_;
  std::unique_ptr<DirectExpressionStep> rhs_;
  bool negation_;
};

class IterativeEqualityStep : public ExpressionStepBase {
 public:
  explicit IterativeEqualityStep(bool negation, int64_t expr_id)
      : ExpressionStepBase(expr_id), negation_(negation) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    if (!frame->value_stack().HasEnough(2)) {
      return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
    }
    auto args = frame->value_stack().GetSpan(2);
    auto attrs = frame->value_stack().GetAttributeSpan(2);

    CEL_ASSIGN_OR_RETURN(Value result,
                         EvaluateEquality(*frame, args[0], attrs[0], args[1],
                                          attrs[1], negation_));

    frame->value_stack().PopAndPush(2, std::move(result));
    return absl::OkStatus();
  }

 private:
  bool negation_;
};

absl::StatusOr<Value> EvaluateInMap(ExecutionFrameBase& frame,
                                    const Value& item,
                                    const MapValue& container) {
  absl::StatusOr<Value> result = {BoolValue(false)};
  switch (item.kind()) {
    case ValueKind::kBool:
    case ValueKind::kString:
    case ValueKind::kInt:
    case ValueKind::kUint:
      result = container.Has(item, frame.descriptor_pool(),
                             frame.message_factory(), frame.arena());
      break;
    case ValueKind::kDouble:
      break;
    default:
      return cel::ErrorValue(
          cel::runtime_internal::CreateNoMatchingOverloadError(
              cel::builtin::kIn));
  }

  if (result.ok() && result.value().IsBool() &&
      result.value().GetBool().NativeValue()) {
    return result;
  }

  if (item.IsDouble() || item.IsUint()) {
    Number number = item.IsDouble()
                        ? Number::FromDouble(item.GetDouble().NativeValue())
                        : Number::FromUint64(item.GetUint().NativeValue());
    if (number.LosslessConvertibleToInt()) {
      result = container.Has(IntValue(number.AsInt()), frame.descriptor_pool(),
                             frame.message_factory(), frame.arena());
      if (result.ok() && result.value().IsBool() &&
          result.value().GetBool().NativeValue()) {
        return result;
      }
    }
  }

  if (item.IsDouble() || item.IsInt()) {
    Number number = item.IsDouble()
                        ? Number::FromDouble(item.GetDouble().NativeValue())
                        : Number::FromInt64(item.GetInt().NativeValue());
    if (number.LosslessConvertibleToUint()) {
      result =
          container.Has(UintValue(number.AsUint()), frame.descriptor_pool(),
                        frame.message_factory(), frame.arena());
      if (result.ok() && result.value().IsBool() &&
          result.value().GetBool().NativeValue()) {
        return result;
      }
    }
  }

  if (!result.ok()) {
    return BoolValue(false);
  }

  return result;
}

absl::StatusOr<Value> EvaluateIn(ExecutionFrameBase& frame, const Value& item,
                                 const AttributeTrail& item_attr,
                                 const Value& container,
                                 const AttributeTrail& container_attr) {
  if (item.IsError()) {
    return item;
  }
  if (container.IsError()) {
    return container;
  }

  if (frame.unknown_processing_enabled()) {
    auto accu = frame.attribute_utility().CreateAccumulator();
    accu.MaybeAdd(item, item_attr);
    accu.MaybeAdd(container, container_attr);
    if (!accu.IsEmpty()) {
      return std::move(accu).Build();
    }
  }
  if (container.IsList()) {
    return container.GetList().Contains(item, frame.descriptor_pool(),
                                        frame.message_factory(), frame.arena());
  }
  if (container.IsMap()) {
    return EvaluateInMap(frame, item, container.GetMap());
  }
  return cel::ErrorValue(
      cel::runtime_internal::CreateNoMatchingOverloadError(cel::builtin::kIn));
}

class DirectInStep : public DirectExpressionStep {
 public:
  explicit DirectInStep(std::unique_ptr<DirectExpressionStep> item,
                        std::unique_ptr<DirectExpressionStep> container,
                        int64_t expr_id)
      : DirectExpressionStep(expr_id),
        item_(std::move(item)),
        container_(std::move(container)) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, Value& result,
                        AttributeTrail& attribute_trail) const override {
    AttributeTrail item_attr;
    CEL_RETURN_IF_ERROR(item_->Evaluate(frame, result, item_attr));

    Value container_result;
    AttributeTrail container_attr;
    CEL_RETURN_IF_ERROR(
        container_->Evaluate(frame, container_result, container_attr));
    CEL_ASSIGN_OR_RETURN(result, EvaluateIn(frame, result, item_attr,
                                            container_result, container_attr));
    return absl::OkStatus();
  }

 private:
  std::unique_ptr<DirectExpressionStep> item_;
  std::unique_ptr<DirectExpressionStep> container_;
};

class IterativeInStep : public ExpressionStepBase {
 public:
  explicit IterativeInStep(int64_t expr_id) : ExpressionStepBase(expr_id) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    if (!frame->value_stack().HasEnough(2)) {
      return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
    }

    auto args = frame->value_stack().GetSpan(2);
    auto attrs = frame->value_stack().GetAttributeSpan(2);

    CEL_ASSIGN_OR_RETURN(
        Value result, EvaluateIn(*frame, args[0], attrs[0], args[1], attrs[1]));
    frame->value_stack().PopAndPush(2, std::move(result));
    return absl::OkStatus();
  }
};

}  // namespace

// Factory method for recursive _==_ and _!=_ Execution step
std::unique_ptr<DirectExpressionStep> CreateDirectEqualityStep(
    std::unique_ptr<DirectExpressionStep> lhs,
    std::unique_ptr<DirectExpressionStep> rhs, bool negation, int64_t expr_id) {
  return std::make_unique<DirectEqualityStep>(std::move(lhs), std::move(rhs),
                                              negation, expr_id);
}

// Factory method for iterative _==_ and _!=_ Execution step
std::unique_ptr<ExpressionStep> CreateEqualityStep(bool negation,
                                                   int64_t expr_id) {
  return std::make_unique<IterativeEqualityStep>(negation, expr_id);
}

// Factory method for recursive @in Execution step
std::unique_ptr<DirectExpressionStep> CreateDirectInStep(
    std::unique_ptr<DirectExpressionStep> item,
    std::unique_ptr<DirectExpressionStep> container, int64_t expr_id) {
  return std::make_unique<DirectInStep>(std::move(item), std::move(container),
                                        expr_id);
}

// Factory method for iterative @in Execution step
std::unique_ptr<ExpressionStep> CreateInStep(int64_t expr_id) {
  return std::make_unique<IterativeInStep>(expr_id);
}

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_EQUALITY_STEPS_H_
