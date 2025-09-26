#include "eval/eval/logic_step.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "base/builtins.h"
#include "common/casting.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/internal/errors.h"
#include "internal/status_macros.h"
#include "runtime/internal/errors.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::BoolValue;
using ::cel::Cast;
using ::cel::ErrorValue;
using ::cel::InstanceOf;
using ::cel::UnknownValue;
using ::cel::Value;
using ::cel::ValueKind;
using ::cel::runtime_internal::CreateNoMatchingOverloadError;

enum class OpType { kAnd, kOr };

// Shared logic for the fall through case (we didn't see the shortcircuit
// value).
absl::Status ReturnLogicResult(ExecutionFrameBase& frame, OpType op_type,
                               Value& lhs_result, Value& rhs_result,
                               AttributeTrail& attribute_trail,
                               AttributeTrail& rhs_attr) {
  ValueKind lhs_kind = lhs_result.kind();
  ValueKind rhs_kind = rhs_result.kind();

  if (frame.unknown_processing_enabled()) {
    if (lhs_kind == ValueKind::kUnknown && rhs_kind == ValueKind::kUnknown) {
      lhs_result = frame.attribute_utility().MergeUnknownValues(
          Cast<UnknownValue>(lhs_result), Cast<UnknownValue>(rhs_result));
      // Clear attribute trail so this doesn't get re-identified as a new
      // unknown and reset the accumulated attributes.
      attribute_trail = AttributeTrail();
      return absl::OkStatus();
    } else if (lhs_kind == ValueKind::kUnknown) {
      return absl::OkStatus();
    } else if (rhs_kind == ValueKind::kUnknown) {
      lhs_result = std::move(rhs_result);
      attribute_trail = std::move(rhs_attr);
      return absl::OkStatus();
    }
  }

  if (lhs_kind == ValueKind::kError) {
    return absl::OkStatus();
  } else if (rhs_kind == ValueKind::kError) {
    lhs_result = std::move(rhs_result);
    attribute_trail = std::move(rhs_attr);
    return absl::OkStatus();
  }

  if (lhs_kind == ValueKind::kBool && rhs_kind == ValueKind::kBool) {
    return absl::OkStatus();
  }

  // Otherwise, add a no overload error.
  attribute_trail = AttributeTrail();
  lhs_result = cel::ErrorValue(CreateNoMatchingOverloadError(
      op_type == OpType::kOr ? cel::builtin::kOr : cel::builtin::kAnd));
  return absl::OkStatus();
}

class ExhaustiveDirectLogicStep : public DirectExpressionStep {
 public:
  explicit ExhaustiveDirectLogicStep(std::unique_ptr<DirectExpressionStep> lhs,
                                     std::unique_ptr<DirectExpressionStep> rhs,
                                     OpType op_type, int64_t expr_id)
      : DirectExpressionStep(expr_id),
        lhs_(std::move(lhs)),
        rhs_(std::move(rhs)),
        op_type_(op_type) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, cel::Value& result,
                        AttributeTrail& attribute_trail) const override;

 private:
  std::unique_ptr<DirectExpressionStep> lhs_;
  std::unique_ptr<DirectExpressionStep> rhs_;
  OpType op_type_;
};

absl::Status ExhaustiveDirectLogicStep::Evaluate(
    ExecutionFrameBase& frame, cel::Value& result,
    AttributeTrail& attribute_trail) const {
  CEL_RETURN_IF_ERROR(lhs_->Evaluate(frame, result, attribute_trail));
  ValueKind lhs_kind = result.kind();

  Value rhs_result;
  AttributeTrail rhs_attr;
  CEL_RETURN_IF_ERROR(rhs_->Evaluate(frame, rhs_result, attribute_trail));

  ValueKind rhs_kind = rhs_result.kind();
  if (lhs_kind == ValueKind::kBool) {
    bool lhs_bool = Cast<BoolValue>(result).NativeValue();
    if ((op_type_ == OpType::kOr && lhs_bool) ||
        (op_type_ == OpType::kAnd && !lhs_bool)) {
      return absl::OkStatus();
    }
  }

  if (rhs_kind == ValueKind::kBool) {
    bool rhs_bool = Cast<BoolValue>(rhs_result).NativeValue();
    if ((op_type_ == OpType::kOr && rhs_bool) ||
        (op_type_ == OpType::kAnd && !rhs_bool)) {
      result = std::move(rhs_result);
      attribute_trail = std::move(rhs_attr);
      return absl::OkStatus();
    }
  }

  return ReturnLogicResult(frame, op_type_, result, rhs_result, attribute_trail,
                           rhs_attr);
}

class DirectLogicStep : public DirectExpressionStep {
 public:
  explicit DirectLogicStep(std::unique_ptr<DirectExpressionStep> lhs,
                           std::unique_ptr<DirectExpressionStep> rhs,
                           OpType op_type, int64_t expr_id)
      : DirectExpressionStep(expr_id),
        lhs_(std::move(lhs)),
        rhs_(std::move(rhs)),
        op_type_(op_type) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, cel::Value& result,
                        AttributeTrail& attribute_trail) const override;

 private:
  std::unique_ptr<DirectExpressionStep> lhs_;
  std::unique_ptr<DirectExpressionStep> rhs_;
  OpType op_type_;
};

absl::Status DirectLogicStep::Evaluate(ExecutionFrameBase& frame, Value& result,
                                       AttributeTrail& attribute_trail) const {
  CEL_RETURN_IF_ERROR(lhs_->Evaluate(frame, result, attribute_trail));
  ValueKind lhs_kind = result.kind();
  if (lhs_kind == ValueKind::kBool) {
    bool lhs_bool = Cast<BoolValue>(result).NativeValue();
    if ((op_type_ == OpType::kOr && lhs_bool) ||
        (op_type_ == OpType::kAnd && !lhs_bool)) {
      return absl::OkStatus();
    }
  }

  Value rhs_result;
  AttributeTrail rhs_attr;

  CEL_RETURN_IF_ERROR(rhs_->Evaluate(frame, rhs_result, attribute_trail));

  ValueKind rhs_kind = rhs_result.kind();

  if (rhs_kind == ValueKind::kBool) {
    bool rhs_bool = Cast<BoolValue>(rhs_result).NativeValue();
    if ((op_type_ == OpType::kOr && rhs_bool) ||
        (op_type_ == OpType::kAnd && !rhs_bool)) {
      result = std::move(rhs_result);
      attribute_trail = std::move(rhs_attr);
      return absl::OkStatus();
    }
  }

  return ReturnLogicResult(frame, op_type_, result, rhs_result, attribute_trail,
                           rhs_attr);
}

class LogicalOpStep : public ExpressionStepBase {
 public:
  // Constructs FunctionStep that uses overloads specified.
  LogicalOpStep(OpType op_type, int64_t expr_id)
      : ExpressionStepBase(expr_id), op_type_(op_type) {
    shortcircuit_ = (op_type_ == OpType::kOr);
  }

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  void Calculate(ExecutionFrame* frame, absl::Span<const Value> args,
                 Value& result) const {
    bool bool_args[2];
    bool has_bool_args[2];

    for (size_t i = 0; i < args.size(); i++) {
      has_bool_args[i] = args[i]->Is<BoolValue>();
      if (has_bool_args[i]) {
        bool_args[i] = args[i].GetBool().NativeValue();
        if (bool_args[i] == shortcircuit_) {
          result = BoolValue{bool_args[i]};
          return;
        }
      }
    }

    if (has_bool_args[0] && has_bool_args[1]) {
      switch (op_type_) {
        case OpType::kAnd:
          result = BoolValue{bool_args[0] && bool_args[1]};
          return;
        case OpType::kOr:
          result = BoolValue{bool_args[0] || bool_args[1]};
          return;
      }
    }

    // As opposed to regular function, logical operation treat Unknowns with
    // higher precedence than error. This is due to the fact that after Unknown
    // is resolved to actual value, it may short-circuit and thus hide the
    // error.
    if (frame->enable_unknowns()) {
      // Check if unknown?
      absl::optional<cel::UnknownValue> unknown_set =
          frame->attribute_utility().MergeUnknowns(args);
      if (unknown_set.has_value()) {
        result = std::move(*unknown_set);
        return;
      }
    }

    if (args[0]->Is<cel::ErrorValue>()) {
      result = args[0];
      return;
    } else if (args[1]->Is<cel::ErrorValue>()) {
      result = args[1];
      return;
    }

    // Fallback.
    result = cel::ErrorValue(CreateNoMatchingOverloadError(
        (op_type_ == OpType::kOr) ? cel::builtin::kOr : cel::builtin::kAnd));
  }

  const OpType op_type_;
  bool shortcircuit_;
};

absl::Status LogicalOpStep::Evaluate(ExecutionFrame* frame) const {
  // Must have 2 or more values on the stack.
  if (!frame->value_stack().HasEnough(2)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }

  // Create Span object that contains input arguments to the function.
  auto args = frame->value_stack().GetSpan(2);
  Value result;
  Calculate(frame, args, result);
  frame->value_stack().PopAndPush(args.size(), std::move(result));

  return absl::OkStatus();
}

std::unique_ptr<DirectExpressionStep> CreateDirectLogicStep(
    std::unique_ptr<DirectExpressionStep> lhs,
    std::unique_ptr<DirectExpressionStep> rhs, int64_t expr_id, OpType op_type,
    bool shortcircuiting) {
  if (shortcircuiting) {
    return std::make_unique<DirectLogicStep>(std::move(lhs), std::move(rhs),
                                             op_type, expr_id);
  } else {
    return std::make_unique<ExhaustiveDirectLogicStep>(
        std::move(lhs), std::move(rhs), op_type, expr_id);
  }
}

class DirectNotStep : public DirectExpressionStep {
 public:
  explicit DirectNotStep(std::unique_ptr<DirectExpressionStep> operand,
                         int64_t expr_id)
      : DirectExpressionStep(expr_id), operand_(std::move(operand)) {}
  absl::Status Evaluate(ExecutionFrameBase& frame, Value& result,
                        AttributeTrail& attribute_trail) const override;

 private:
  std::unique_ptr<DirectExpressionStep> operand_;
};

absl::Status DirectNotStep::Evaluate(ExecutionFrameBase& frame, Value& result,
                                     AttributeTrail& attribute_trail) const {
  CEL_RETURN_IF_ERROR(operand_->Evaluate(frame, result, attribute_trail));

  if (frame.unknown_processing_enabled()) {
    if (frame.attribute_utility().CheckForUnknownPartial(attribute_trail)) {
      result = frame.attribute_utility().CreateUnknownSet(
          attribute_trail.attribute());
      return absl::OkStatus();
    }
  }

  switch (result.kind()) {
    case ValueKind::kBool:
      result = BoolValue{!result.GetBool().NativeValue()};
      break;
    case ValueKind::kUnknown:
    case ValueKind::kError:
      // just forward.
      break;
    default:
      result =
          cel::ErrorValue(CreateNoMatchingOverloadError(cel::builtin::kNot));
      break;
  }

  return absl::OkStatus();
}

class IterativeNotStep : public ExpressionStepBase {
 public:
  explicit IterativeNotStep(int64_t expr_id) : ExpressionStepBase(expr_id) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;
};

absl::Status IterativeNotStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(1)) {
    return absl::InternalError("Value stack underflow");
  }
  const Value& operand = frame->value_stack().Peek();

  if (frame->unknown_processing_enabled()) {
    const AttributeTrail& attribute_trail =
        frame->value_stack().PeekAttribute();
    if (frame->attribute_utility().CheckForUnknownPartial(attribute_trail)) {
      frame->value_stack().PopAndPush(
          frame->attribute_utility().CreateUnknownSet(
              attribute_trail.attribute()));
      return absl::OkStatus();
    }
  }

  switch (operand.kind()) {
    case ValueKind::kBool:
      frame->value_stack().PopAndPush(
          BoolValue{!operand.GetBool().NativeValue()});
      break;
    case ValueKind::kUnknown:
    case ValueKind::kError:
      // just forward.
      break;
    default:
      frame->value_stack().PopAndPush(
          cel::ErrorValue(CreateNoMatchingOverloadError(cel::builtin::kNot)));
      break;
  }

  return absl::OkStatus();
}

class DirectNotStrictlyFalseStep : public DirectExpressionStep {
 public:
  explicit DirectNotStrictlyFalseStep(
      std::unique_ptr<DirectExpressionStep> operand, int64_t expr_id)
      : DirectExpressionStep(expr_id), operand_(std::move(operand)) {}
  absl::Status Evaluate(ExecutionFrameBase& frame, Value& result,
                        AttributeTrail& attribute_trail) const override;

 private:
  std::unique_ptr<DirectExpressionStep> operand_;
};

absl::Status DirectNotStrictlyFalseStep::Evaluate(
    ExecutionFrameBase& frame, Value& result,
    AttributeTrail& attribute_trail) const {
  CEL_RETURN_IF_ERROR(operand_->Evaluate(frame, result, attribute_trail));

  switch (result.kind()) {
    case ValueKind::kBool:
      // just forward.
      break;
    case ValueKind::kUnknown:
    case ValueKind::kError:
      result = BoolValue(true);
      break;
    default:
      result =
          cel::ErrorValue(CreateNoMatchingOverloadError(cel::builtin::kNot));
      break;
  }

  return absl::OkStatus();
}

class IterativeNotStrictlyFalseStep : public ExpressionStepBase {
 public:
  explicit IterativeNotStrictlyFalseStep(int64_t expr_id)
      : ExpressionStepBase(expr_id) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;
};

absl::Status IterativeNotStrictlyFalseStep::Evaluate(
    ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(1)) {
    return absl::InternalError("Value stack underflow");
  }
  const Value& operand = frame->value_stack().Peek();

  switch (operand.kind()) {
    case ValueKind::kBool:
      // just forward.
      break;
    case ValueKind::kUnknown:
    case ValueKind::kError:
      frame->value_stack().PopAndPush(BoolValue(true));
      break;
    default:
      frame->value_stack().PopAndPush(
          cel::ErrorValue(CreateNoMatchingOverloadError(cel::builtin::kNot)));
      break;
  }

  return absl::OkStatus();
}

}  // namespace

// Factory method for "And" Execution step
std::unique_ptr<DirectExpressionStep> CreateDirectAndStep(
    std::unique_ptr<DirectExpressionStep> lhs,
    std::unique_ptr<DirectExpressionStep> rhs, int64_t expr_id,
    bool shortcircuiting) {
  return CreateDirectLogicStep(std::move(lhs), std::move(rhs), expr_id,
                               OpType::kAnd, shortcircuiting);
}

// Factory method for "Or" Execution step
std::unique_ptr<DirectExpressionStep> CreateDirectOrStep(
    std::unique_ptr<DirectExpressionStep> lhs,
    std::unique_ptr<DirectExpressionStep> rhs, int64_t expr_id,
    bool shortcircuiting) {
  return CreateDirectLogicStep(std::move(lhs), std::move(rhs), expr_id,
                               OpType::kOr, shortcircuiting);
}

// Factory method for "And" Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateAndStep(int64_t expr_id) {
  return std::make_unique<LogicalOpStep>(OpType::kAnd, expr_id);
}

// Factory method for "Or" Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateOrStep(int64_t expr_id) {
  return std::make_unique<LogicalOpStep>(OpType::kOr, expr_id);
}

// Factory method for recursive logical not "!" Execution step
std::unique_ptr<DirectExpressionStep> CreateDirectNotStep(
    std::unique_ptr<DirectExpressionStep> operand, int64_t expr_id) {
  return std::make_unique<DirectNotStep>(std::move(operand), expr_id);
}

// Factory method for iterative logical not "!" Execution step
std::unique_ptr<ExpressionStep> CreateNotStep(int64_t expr_id) {
  return std::make_unique<IterativeNotStep>(expr_id);
}

// Factory method for recursive logical "@not_strictly_false" Execution step.
std::unique_ptr<DirectExpressionStep> CreateDirectNotStrictlyFalseStep(
    std::unique_ptr<DirectExpressionStep> operand, int64_t expr_id) {
  return std::make_unique<DirectNotStrictlyFalseStep>(std::move(operand),
                                                      expr_id);
}

// Factory method for iterative logical "@not_strictly_false" Execution step.
std::unique_ptr<ExpressionStep> CreateNotStrictlyFalseStep(int64_t expr_id) {
  return std::make_unique<IterativeNotStrictlyFalseStep>(expr_id);
}

}  // namespace google::api::expr::runtime
