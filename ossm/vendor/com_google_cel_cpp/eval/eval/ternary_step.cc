#include "eval/eval/ternary_step.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/builtins.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/internal/errors.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::builtin::kTernary;
using ::cel::runtime_internal::CreateNoMatchingOverloadError;

inline constexpr size_t kTernaryStepCondition = 0;
inline constexpr size_t kTernaryStepTrue = 1;
inline constexpr size_t kTernaryStepFalse = 2;

class ExhaustiveDirectTernaryStep : public DirectExpressionStep {
 public:
  ExhaustiveDirectTernaryStep(std::unique_ptr<DirectExpressionStep> condition,
                              std::unique_ptr<DirectExpressionStep> left,
                              std::unique_ptr<DirectExpressionStep> right,
                              int64_t expr_id)
      : DirectExpressionStep(expr_id),
        condition_(std::move(condition)),
        left_(std::move(left)),
        right_(std::move(right)) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, cel::Value& result,
                        AttributeTrail& attribute) const override {
    cel::Value condition;
    cel::Value lhs;
    cel::Value rhs;

    AttributeTrail condition_attr;
    AttributeTrail lhs_attr;
    AttributeTrail rhs_attr;

    CEL_RETURN_IF_ERROR(condition_->Evaluate(frame, condition, condition_attr));
    CEL_RETURN_IF_ERROR(left_->Evaluate(frame, lhs, lhs_attr));
    CEL_RETURN_IF_ERROR(right_->Evaluate(frame, rhs, rhs_attr));

    if (condition.IsError() || condition.IsUnknown()) {
      result = std::move(condition);
      attribute = std::move(condition_attr);
      return absl::OkStatus();
    }

    if (!condition.IsBool()) {
      result = cel::ErrorValue(CreateNoMatchingOverloadError(kTernary));
      return absl::OkStatus();
    }

    if (condition.GetBool().NativeValue()) {
      result = std::move(lhs);
      attribute = std::move(lhs_attr);
    } else {
      result = std::move(rhs);
      attribute = std::move(rhs_attr);
    }
    return absl::OkStatus();
  }

 private:
  std::unique_ptr<DirectExpressionStep> condition_;
  std::unique_ptr<DirectExpressionStep> left_;
  std::unique_ptr<DirectExpressionStep> right_;
};

class ShortcircuitingDirectTernaryStep : public DirectExpressionStep {
 public:
  ShortcircuitingDirectTernaryStep(
      std::unique_ptr<DirectExpressionStep> condition,
      std::unique_ptr<DirectExpressionStep> left,
      std::unique_ptr<DirectExpressionStep> right, int64_t expr_id)
      : DirectExpressionStep(expr_id),
        condition_(std::move(condition)),
        left_(std::move(left)),
        right_(std::move(right)) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, cel::Value& result,
                        AttributeTrail& attribute) const override {
    cel::Value condition;

    AttributeTrail condition_attr;

    CEL_RETURN_IF_ERROR(condition_->Evaluate(frame, condition, condition_attr));

    if (condition.IsError() || condition.IsUnknown()) {
      result = std::move(condition);
      attribute = std::move(condition_attr);
      return absl::OkStatus();
    }

    if (!condition.IsBool()) {
      result = cel::ErrorValue(CreateNoMatchingOverloadError(kTernary));
      return absl::OkStatus();
    }

    if (condition.GetBool().NativeValue()) {
      return left_->Evaluate(frame, result, attribute);
    }
    return right_->Evaluate(frame, result, attribute);
  }

 private:
  std::unique_ptr<DirectExpressionStep> condition_;
  std::unique_ptr<DirectExpressionStep> left_;
  std::unique_ptr<DirectExpressionStep> right_;
};

class TernaryStep : public ExpressionStepBase {
 public:
  // Constructs FunctionStep that uses overloads specified.
  explicit TernaryStep(int64_t expr_id) : ExpressionStepBase(expr_id) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;
};

absl::Status TernaryStep::Evaluate(ExecutionFrame* frame) const {
  // Must have 3 or more values on the stack.
  if (!frame->value_stack().HasEnough(3)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }

  // Create Span object that contains input arguments to the function.
  auto args = frame->value_stack().GetSpan(3);

  const auto& condition = args[kTernaryStepCondition];
  // As opposed to regular functions, ternary treats unknowns or errors on the
  // condition (arg0) as blocking. If we get an error or unknown then we
  // ignore the other arguments and forward the condition as the result.
  if (frame->enable_unknowns()) {
    // Check if unknown?
    if (condition.IsUnknown()) {
      frame->value_stack().Pop(2);
      return absl::OkStatus();
    }
  }

  if (condition.IsError()) {
    frame->value_stack().Pop(2);
    return absl::OkStatus();
  }

  cel::Value result;
  if (!condition.IsBool()) {
    result = cel::ErrorValue(CreateNoMatchingOverloadError(kTernary));
  } else if (condition.GetBool().NativeValue()) {
    result = args[kTernaryStepTrue];
  } else {
    result = args[kTernaryStepFalse];
  }

  frame->value_stack().PopAndPush(args.size(), std::move(result));

  return absl::OkStatus();
}

}  // namespace

// Factory method for ternary (_?_:_) recursive execution step
std::unique_ptr<DirectExpressionStep> CreateDirectTernaryStep(
    std::unique_ptr<DirectExpressionStep> condition,
    std::unique_ptr<DirectExpressionStep> left,
    std::unique_ptr<DirectExpressionStep> right, int64_t expr_id,
    bool shortcircuiting) {
  if (shortcircuiting) {
    return std::make_unique<ShortcircuitingDirectTernaryStep>(
        std::move(condition), std::move(left), std::move(right), expr_id);
  }

  return std::make_unique<ExhaustiveDirectTernaryStep>(
      std::move(condition), std::move(left), std::move(right), expr_id);
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateTernaryStep(
    int64_t expr_id) {
  return std::make_unique<TernaryStep>(expr_id);
}

}  // namespace google::api::expr::runtime
