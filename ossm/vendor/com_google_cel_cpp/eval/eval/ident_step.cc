#include "eval/eval/ident_step.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/ast_internal/expr.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/comprehension_slots.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/internal/errors.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::Value;
using ::cel::runtime_internal::CreateError;

class IdentStep : public ExpressionStepBase {
 public:
  IdentStep(absl::string_view name, int64_t expr_id)
      : ExpressionStepBase(expr_id), name_(name) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  std::string name_;
};

absl::Status LookupIdent(const std::string& name, ExecutionFrameBase& frame,
                         Value& result, AttributeTrail& attribute) {
  if (frame.attribute_tracking_enabled()) {
    attribute = AttributeTrail(name);
    if (frame.missing_attribute_errors_enabled() &&
        frame.attribute_utility().CheckForMissingAttribute(attribute)) {
      CEL_ASSIGN_OR_RETURN(
          result, frame.attribute_utility().CreateMissingAttributeError(
                      attribute.attribute()));
      return absl::OkStatus();
    }
    if (frame.unknown_processing_enabled() &&
        frame.attribute_utility().CheckForUnknownExact(attribute)) {
      result =
          frame.attribute_utility().CreateUnknownSet(attribute.attribute());
      return absl::OkStatus();
    }
  }

  CEL_ASSIGN_OR_RETURN(auto found, frame.activation().FindVariable(
                                       frame.value_manager(), name, result));

  if (found) {
    return absl::OkStatus();
  }

  result = frame.value_manager().CreateErrorValue(CreateError(
      absl::StrCat("No value with name \"", name, "\" found in Activation")));

  return absl::OkStatus();
}

absl::Status IdentStep::Evaluate(ExecutionFrame* frame) const {
  Value value;
  AttributeTrail attribute;

  CEL_RETURN_IF_ERROR(LookupIdent(name_, *frame, value, attribute));

  frame->value_stack().Push(std::move(value), std::move(attribute));

  return absl::OkStatus();
}

absl::StatusOr<absl::Nonnull<const ComprehensionSlots::Slot*>> LookupSlot(
    absl::string_view name, size_t slot_index, ExecutionFrameBase& frame) {
  const ComprehensionSlots::Slot* slot =
      frame.comprehension_slots().Get(slot_index);
  if (slot == nullptr) {
    return absl::InternalError(
        absl::StrCat("Comprehension variable accessed out of scope: ", name));
  }
  return slot;
}

class SlotStep : public ExpressionStepBase {
 public:
  SlotStep(absl::string_view name, size_t slot_index, int64_t expr_id)
      : ExpressionStepBase(expr_id), name_(name), slot_index_(slot_index) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    CEL_ASSIGN_OR_RETURN(const ComprehensionSlots::Slot* slot,
                         LookupSlot(name_, slot_index_, *frame));

    frame->value_stack().Push(slot->value, slot->attribute);
    return absl::OkStatus();
  }

 private:
  std::string name_;

  size_t slot_index_;
};

class DirectIdentStep : public DirectExpressionStep {
 public:
  DirectIdentStep(absl::string_view name, int64_t expr_id)
      : DirectExpressionStep(expr_id), name_(name) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, Value& result,
                        AttributeTrail& attribute) const override {
    return LookupIdent(name_, frame, result, attribute);
  }

 private:
  std::string name_;
};

class DirectSlotStep : public DirectExpressionStep {
 public:
  DirectSlotStep(std::string name, size_t slot_index, int64_t expr_id)
      : DirectExpressionStep(expr_id),
        name_(std::move(name)),
        slot_index_(slot_index) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, Value& result,
                        AttributeTrail& attribute) const override {
    CEL_ASSIGN_OR_RETURN(const ComprehensionSlots::Slot* slot,
                         LookupSlot(name_, slot_index_, frame));

    if (frame.attribute_tracking_enabled()) {
      attribute = slot->attribute;
    }
    result = slot->value;
    return absl::OkStatus();
  }

 private:
  std::string name_;
  size_t slot_index_;
};

}  // namespace

std::unique_ptr<DirectExpressionStep> CreateDirectIdentStep(
    absl::string_view identifier, int64_t expr_id) {
  return std::make_unique<DirectIdentStep>(identifier, expr_id);
}

std::unique_ptr<DirectExpressionStep> CreateDirectSlotIdentStep(
    absl::string_view identifier, size_t slot_index, int64_t expr_id) {
  return std::make_unique<DirectSlotStep>(std::string(identifier), slot_index,
                                          expr_id);
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateIdentStep(
    const cel::ast_internal::Ident& ident_expr, int64_t expr_id) {
  return std::make_unique<IdentStep>(ident_expr.name(), expr_id);
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateIdentStepForSlot(
    const cel::ast_internal::Ident& ident_expr, size_t slot_index,
    int64_t expr_id) {
  return std::make_unique<SlotStep>(ident_expr.name(), slot_index, expr_id);
}

}  // namespace google::api::expr::runtime
