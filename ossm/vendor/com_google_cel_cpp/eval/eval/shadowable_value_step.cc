#include "eval/eval/shadowable_value_step.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::Value;

class ShadowableValueStep : public ExpressionStepBase {
 public:
  ShadowableValueStep(std::string identifier, cel::Value value, int64_t expr_id)
      : ExpressionStepBase(expr_id),
        identifier_(std::move(identifier)),
        value_(std::move(value)) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  std::string identifier_;
  Value value_;
};

absl::Status ShadowableValueStep::Evaluate(ExecutionFrame* frame) const {
  cel::Value result;
  CEL_ASSIGN_OR_RETURN(auto found,
                       frame->modern_activation().FindVariable(
                           identifier_, frame->descriptor_pool(),
                           frame->message_factory(), frame->arena(), &result));
  if (found) {
    frame->value_stack().Push(std::move(result));
  } else {
    frame->value_stack().Push(value_);
  }
  return absl::OkStatus();
}

class DirectShadowableValueStep : public DirectExpressionStep {
 public:
  DirectShadowableValueStep(std::string identifier, cel::Value value,
                            int64_t expr_id)
      : DirectExpressionStep(expr_id),
        identifier_(std::move(identifier)),
        value_(std::move(value)) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, Value& result,
                        AttributeTrail& attribute) const override;

 private:
  std::string identifier_;
  Value value_;
};

// TODO(uncreated-issue/67): Attribute tracking is skipped for the shadowed case. May
// cause problems for users with unknown tracking and variables named like
// 'list' etc, but follows the current behavior of the stack machine version.
absl::Status DirectShadowableValueStep::Evaluate(
    ExecutionFrameBase& frame, Value& result, AttributeTrail& attribute) const {
  CEL_ASSIGN_OR_RETURN(auto found,
                       frame.activation().FindVariable(
                           identifier_, frame.descriptor_pool(),
                           frame.message_factory(), frame.arena(), &result));
  if (!found) {
    result = value_;
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateShadowableValueStep(
    std::string identifier, cel::Value value, int64_t expr_id) {
  return absl::make_unique<ShadowableValueStep>(std::move(identifier),
                                                std::move(value), expr_id);
}

std::unique_ptr<DirectExpressionStep> CreateDirectShadowableValueStep(
    std::string identifier, cel::Value value, int64_t expr_id) {
  return std::make_unique<DirectShadowableValueStep>(std::move(identifier),
                                                     std::move(value), expr_id);
}

}  // namespace google::api::expr::runtime
