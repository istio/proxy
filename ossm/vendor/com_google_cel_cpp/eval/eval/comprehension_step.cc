#include "eval/eval/comprehension_step.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/casts.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/attribute.h"
#include "common/casting.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/comprehension_slots.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/internal/errors.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {
namespace {

enum class IterableKind {
  kList = 1,
  kMap,
};

using ::cel::AttributeQualifier;
using ::cel::Cast;
using ::cel::InstanceOf;
using ::cel::UnknownValue;
using ::cel::Value;
using ::cel::ValueIterator;
using ::cel::ValueIteratorPtr;
using ::cel::ValueKind;
using ::cel::runtime_internal::CreateNoMatchingOverloadError;

AttributeQualifier AttributeQualifierFromValue(const Value& v) {
  switch (v.kind()) {
    case ValueKind::kString:
      return AttributeQualifier::OfString(v.GetString().ToString());
    case ValueKind::kInt64:
      return AttributeQualifier::OfInt(v.GetInt().NativeValue());
    case ValueKind::kUint64:
      return AttributeQualifier::OfUint(v.GetUint().NativeValue());
    case ValueKind::kBool:
      return AttributeQualifier::OfBool(v.GetBool().NativeValue());
    default:
      // Non-matching qualifier.
      return AttributeQualifier();
  }
}

class ComprehensionFinishStep final : public ExpressionStepBase {
 public:
  ComprehensionFinishStep(size_t accu_slot, int64_t expr_id)
      : ExpressionStepBase(expr_id), accu_slot_(accu_slot) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    if (!frame->value_stack().HasEnough(2)) {
      return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
    }
    frame->value_stack().SwapAndPop(2, 1);
    frame->comprehension_slots().ClearSlot(accu_slot_);
    frame->iterator_stack().Pop();
    return absl::OkStatus();
  }

 private:
  const size_t accu_slot_;
};

class ComprehensionDirectStep final : public DirectExpressionStep {
 public:
  explicit ComprehensionDirectStep(
      size_t iter_slot, size_t iter2_slot, size_t accu_slot,
      std::unique_ptr<DirectExpressionStep> range,
      std::unique_ptr<DirectExpressionStep> accu_init,
      std::unique_ptr<DirectExpressionStep> loop_step,
      std::unique_ptr<DirectExpressionStep> condition_step,
      std::unique_ptr<DirectExpressionStep> result_step, bool shortcircuiting,
      int64_t expr_id)
      : DirectExpressionStep(expr_id),
        iter_slot_(iter_slot),
        iter2_slot_(iter2_slot),
        accu_slot_(accu_slot),
        range_(std::move(range)),
        accu_init_(std::move(accu_init)),
        loop_step_(std::move(loop_step)),
        condition_(std::move(condition_step)),
        result_step_(std::move(result_step)),
        shortcircuiting_(shortcircuiting) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, Value& result,
                        AttributeTrail& trail) const override {
    return iter_slot_ == iter2_slot_ ? Evaluate1(frame, result, trail)
                                     : Evaluate2(frame, result, trail);
  }

 private:
  absl::Status Evaluate1(ExecutionFrameBase& frame, Value& result,
                         AttributeTrail& trail) const;

  absl::StatusOr<bool> Evaluate1Unknown(
      ExecutionFrameBase& frame, IterableKind range_iter_kind,
      const AttributeTrail& range_iter_attr,
      ValueIterator* absl_nonnull range_iter,
      ComprehensionSlots::Slot* absl_nonnull accu_slot,
      ComprehensionSlots::Slot* absl_nonnull iter_slot, Value& result,
      AttributeTrail& trail) const;

  absl::StatusOr<bool> Evaluate1Known(
      ExecutionFrameBase& frame, ValueIterator* absl_nonnull range_iter,
      ComprehensionSlots::Slot* absl_nonnull accu_slot,
      ComprehensionSlots::Slot* absl_nonnull iter_slot, Value& result,
      AttributeTrail& trail) const;

  absl::Status Evaluate2(ExecutionFrameBase& frame, Value& result,
                         AttributeTrail& trail) const;

  const size_t iter_slot_;
  const size_t iter2_slot_;
  const size_t accu_slot_;
  const std::unique_ptr<DirectExpressionStep> range_;
  const std::unique_ptr<DirectExpressionStep> accu_init_;
  const std::unique_ptr<DirectExpressionStep> loop_step_;
  const std::unique_ptr<DirectExpressionStep> condition_;
  const std::unique_ptr<DirectExpressionStep> result_step_;
  const bool shortcircuiting_;
};

absl::Status ComprehensionDirectStep::Evaluate1(ExecutionFrameBase& frame,
                                                Value& result,
                                                AttributeTrail& trail) const {
  Value range;
  AttributeTrail range_attr;
  CEL_RETURN_IF_ERROR(range_->Evaluate(frame, range, range_attr));

  if (frame.unknown_processing_enabled() && range.IsMap()) {
    if (frame.attribute_utility().CheckForUnknownPartial(range_attr)) {
      result =
          frame.attribute_utility().CreateUnknownSet(range_attr.attribute());
      return absl::OkStatus();
    }
  }

  absl_nullability_unknown ValueIteratorPtr range_iter;
  IterableKind iterable_kind;
  switch (range.kind()) {
    case ValueKind::kList: {
      CEL_ASSIGN_OR_RETURN(range_iter, range.GetList().NewIterator());
      iterable_kind = IterableKind::kList;
    } break;
    case ValueKind::kMap: {
      CEL_ASSIGN_OR_RETURN(range_iter, range.GetMap().NewIterator());
      iterable_kind = IterableKind::kMap;
    } break;
    case ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUnknown:
      result = std::move(range);
      return absl::OkStatus();
    default:
      result = cel::ErrorValue(CreateNoMatchingOverloadError("<iter_range>"));
      return absl::OkStatus();
  }
  ABSL_DCHECK(range_iter != nullptr);

  ComprehensionSlots::Slot* accu_slot =
      frame.comprehension_slots().Get(accu_slot_);
  ABSL_DCHECK(accu_slot != nullptr);

  {
    Value accu_init;
    AttributeTrail accu_init_attr;
    CEL_RETURN_IF_ERROR(accu_init_->Evaluate(frame, accu_init, accu_init_attr));
    accu_slot->Set(std::move(accu_init), std::move(accu_init_attr));
  }

  ComprehensionSlots::Slot* iter_slot =
      frame.comprehension_slots().Get(iter_slot_);
  ABSL_DCHECK(iter_slot != nullptr);
  iter_slot->Set();

  bool should_skip_result;
  if (frame.unknown_processing_enabled()) {
    CEL_ASSIGN_OR_RETURN(
        should_skip_result,
        Evaluate1Unknown(frame, iterable_kind, range_attr, range_iter.get(),
                         accu_slot, iter_slot, result, trail));
  } else {
    CEL_ASSIGN_OR_RETURN(should_skip_result,
                         Evaluate1Known(frame, range_iter.get(), accu_slot,
                                        iter_slot, result, trail));
  }

  frame.comprehension_slots().ClearSlot(iter_slot_);
  if (!should_skip_result) {
    CEL_RETURN_IF_ERROR(result_step_->Evaluate(frame, result, trail));
  }
  frame.comprehension_slots().ClearSlot(accu_slot_);
  return absl::OkStatus();
}

absl::StatusOr<bool> ComprehensionDirectStep::Evaluate1Unknown(
    ExecutionFrameBase& frame, IterableKind range_iter_kind,
    const AttributeTrail& range_iter_attr,
    ValueIterator* absl_nonnull range_iter,
    ComprehensionSlots::Slot* absl_nonnull accu_slot,
    ComprehensionSlots::Slot* absl_nonnull iter_slot, Value& result,
    AttributeTrail& trail) const {
  Value condition;
  AttributeTrail condition_attr;
  Value key_or_value;
  Value* key;
  Value* value;

  switch (range_iter_kind) {
    case IterableKind::kList:
      key = &key_or_value;
      value = iter_slot->mutable_value();
      break;
    case IterableKind::kMap:
      key = iter_slot->mutable_value();
      value = nullptr;
      break;
    default:
      ABSL_UNREACHABLE();
  }
  while (true) {
    CEL_ASSIGN_OR_RETURN(bool ok, range_iter->Next2(frame.descriptor_pool(),
                                                    frame.message_factory(),
                                                    frame.arena(), key, value));
    if (!ok) {
      break;
    }
    CEL_RETURN_IF_ERROR(frame.IncrementIterations());
    *iter_slot->mutable_attribute() =
        range_iter_attr.Step(AttributeQualifierFromValue(*key));
    if (frame.attribute_utility().CheckForUnknownExact(
            iter_slot->attribute())) {
      *iter_slot->mutable_value() = frame.attribute_utility().CreateUnknownSet(
          iter_slot->attribute().attribute());
    }

    // Evaluate the loop condition.
    CEL_RETURN_IF_ERROR(condition_->Evaluate(frame, condition, condition_attr));

    switch (condition.kind()) {
      case ValueKind::kBool:
        break;
      case ValueKind::kError:
        ABSL_FALLTHROUGH_INTENDED;
      case ValueKind::kUnknown:
        result = std::move(condition);
        return true;
      default:
        result =
            cel::ErrorValue(CreateNoMatchingOverloadError("<loop_condition>"));
        return true;
    }

    if (shortcircuiting_ && !absl::implicit_cast<bool>(condition.GetBool())) {
      break;
    }

    // Evaluate the loop step.
    CEL_RETURN_IF_ERROR(loop_step_->Evaluate(frame, *accu_slot->mutable_value(),
                                             *accu_slot->mutable_attribute()));
  }
  return false;
}

absl::StatusOr<bool> ComprehensionDirectStep::Evaluate1Known(
    ExecutionFrameBase& frame, ValueIterator* absl_nonnull range_iter,
    ComprehensionSlots::Slot* absl_nonnull accu_slot,
    ComprehensionSlots::Slot* absl_nonnull iter_slot, Value& result,
    AttributeTrail& trail) const {
  Value condition;
  AttributeTrail condition_attr;

  while (true) {
    CEL_ASSIGN_OR_RETURN(
        bool ok,
        range_iter->Next1(frame.descriptor_pool(), frame.message_factory(),
                          frame.arena(), iter_slot->mutable_value()));
    if (!ok) {
      break;
    }
    CEL_RETURN_IF_ERROR(frame.IncrementIterations());

    // Evaluate the loop condition.
    CEL_RETURN_IF_ERROR(condition_->Evaluate(frame, condition, condition_attr));

    switch (condition.kind()) {
      case ValueKind::kBool:
        break;
      case ValueKind::kError:
        ABSL_FALLTHROUGH_INTENDED;
      case ValueKind::kUnknown:
        result = std::move(condition);
        return true;
      default:
        result =
            cel::ErrorValue(CreateNoMatchingOverloadError("<loop_condition>"));
        return true;
    }

    if (shortcircuiting_ && !absl::implicit_cast<bool>(condition.GetBool())) {
      break;
    }

    // Evaluate the loop step.
    CEL_RETURN_IF_ERROR(loop_step_->Evaluate(frame, *accu_slot->mutable_value(),
                                             *accu_slot->mutable_attribute()));
  }
  return false;
}

absl::Status ComprehensionDirectStep::Evaluate2(ExecutionFrameBase& frame,
                                                Value& result,
                                                AttributeTrail& trail) const {
  Value range;
  AttributeTrail range_attr;
  CEL_RETURN_IF_ERROR(range_->Evaluate(frame, range, range_attr));

  if (frame.unknown_processing_enabled() && range.IsMap()) {
    if (frame.attribute_utility().CheckForUnknownPartial(range_attr)) {
      result =
          frame.attribute_utility().CreateUnknownSet(range_attr.attribute());
      return absl::OkStatus();
    }
  }

  absl_nullability_unknown ValueIteratorPtr range_iter;
  switch (range.kind()) {
    case ValueKind::kList: {
      CEL_ASSIGN_OR_RETURN(range_iter, range.GetList().NewIterator());
    } break;
    case ValueKind::kMap: {
      CEL_ASSIGN_OR_RETURN(range_iter, range.GetMap().NewIterator());
    } break;
    case ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUnknown:
      result = std::move(range);
      return absl::OkStatus();
    default:
      result = cel::ErrorValue(CreateNoMatchingOverloadError("<iter_range>"));
      return absl::OkStatus();
  }
  ABSL_DCHECK(range_iter != nullptr);

  ComprehensionSlots::Slot* accu_slot =
      frame.comprehension_slots().Get(accu_slot_);
  ABSL_DCHECK(accu_slot != nullptr);

  {
    Value accu_init;
    AttributeTrail accu_init_attr;
    CEL_RETURN_IF_ERROR(accu_init_->Evaluate(frame, accu_init, accu_init_attr));
    accu_slot->Set(std::move(accu_init), std::move(accu_init_attr));
  }

  ComprehensionSlots::Slot* iter_slot =
      frame.comprehension_slots().Get(iter_slot_);
  ABSL_DCHECK(iter_slot != nullptr);
  iter_slot->Set();

  ComprehensionSlots::Slot* iter2_slot =
      frame.comprehension_slots().Get(iter2_slot_);
  ABSL_DCHECK(iter2_slot != nullptr);
  iter2_slot->Set();

  Value condition;
  AttributeTrail condition_attr;
  bool should_skip_result = false;

  while (true) {
    CEL_ASSIGN_OR_RETURN(
        bool ok,
        range_iter->Next2(frame.descriptor_pool(), frame.message_factory(),
                          frame.arena(), iter_slot->mutable_value(),
                          iter2_slot->mutable_value()));
    if (!ok) {
      break;
    }
    CEL_RETURN_IF_ERROR(frame.IncrementIterations());
    if (frame.unknown_processing_enabled()) {
      *iter_slot->mutable_attribute() = *iter2_slot->mutable_attribute() =
          range_attr.Step(AttributeQualifierFromValue(iter_slot->value()));
      if (frame.attribute_utility().CheckForUnknownExact(
              iter_slot->attribute())) {
        *iter2_slot->mutable_value() =
            frame.attribute_utility().CreateUnknownSet(
                iter_slot->attribute().attribute());
      }
    }

    // Evaluate the loop condition.
    CEL_RETURN_IF_ERROR(condition_->Evaluate(frame, condition, condition_attr));

    switch (condition.kind()) {
      case ValueKind::kBool:
        break;
      case ValueKind::kError:
        ABSL_FALLTHROUGH_INTENDED;
      case ValueKind::kUnknown:
        result = std::move(condition);
        should_skip_result = true;
        goto finish;
      default:
        result =
            cel::ErrorValue(CreateNoMatchingOverloadError("<loop_condition>"));
        should_skip_result = true;
        goto finish;
    }

    if (shortcircuiting_ && !absl::implicit_cast<bool>(condition.GetBool())) {
      break;
    }

    // Evaluate the loop step.
    CEL_RETURN_IF_ERROR(loop_step_->Evaluate(frame, *accu_slot->mutable_value(),
                                             *accu_slot->mutable_attribute()));
  }

finish:
  iter_slot->Clear();
  iter2_slot->Clear();
  if (!should_skip_result) {
    CEL_RETURN_IF_ERROR(result_step_->Evaluate(frame, result, trail));
  }
  accu_slot->Clear();
  return absl::OkStatus();
}

}  // namespace

absl::Status ComprehensionInitStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(1)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }

  const Value& top = frame->value_stack().Peek();
  if (top.IsError() || top.IsUnknown()) {
    return frame->JumpTo(error_jump_offset_);
  }

  if (frame->enable_unknowns() && top.IsMap()) {
    const AttributeTrail& top_attr = frame->value_stack().PeekAttribute();
    if (frame->attribute_utility().CheckForUnknownPartial(top_attr)) {
      frame->value_stack().PopAndPush(
          frame->attribute_utility().CreateUnknownSet(top_attr.attribute()));
      return frame->JumpTo(error_jump_offset_);
    }
  }

  switch (top.kind()) {
    case ValueKind::kList: {
      CEL_ASSIGN_OR_RETURN(auto iterator, top.GetList().NewIterator());
      frame->iterator_stack().Push(std::move(iterator));
    } break;
    case ValueKind::kMap: {
      CEL_ASSIGN_OR_RETURN(auto iterator, top.GetMap().NewIterator());
      frame->iterator_stack().Push(std::move(iterator));
    } break;
    default:
      // Replace <iter_range> with an error and jump past
      // ComprehensionFinishStep.
      frame->value_stack().PopAndPush(
          cel::ErrorValue(CreateNoMatchingOverloadError("<iter_range>")));
      return frame->JumpTo(error_jump_offset_);
  }

  return absl::OkStatus();
}

absl::Status ComprehensionNextStep::Evaluate1(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(2)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }

  {
    Value& accu_var = frame->value_stack().Peek();
    AttributeTrail& accu_var_attr = frame->value_stack().PeekAttribute();
    frame->comprehension_slots().Set(accu_slot_, std::move(accu_var),
                                     std::move(accu_var_attr));
    frame->value_stack().Pop(1);
  }

  ComprehensionSlots::Slot* iter_slot =
      frame->comprehension_slots().Get(iter_slot_);
  ABSL_DCHECK(iter_slot != nullptr);
  iter_slot->Set();

  if (frame->enable_unknowns()) {
    Value key_or_value;
    Value* key;
    Value* value;
    switch (frame->value_stack().Peek().kind()) {
      case ValueKind::kList:
        key = &key_or_value;
        value = iter_slot->mutable_value();
        break;
      case ValueKind::kMap:
        key = iter_slot->mutable_value();
        value = nullptr;
        break;
      default:
        ABSL_UNREACHABLE();
    }
    CEL_ASSIGN_OR_RETURN(bool ok,
                         frame->iterator_stack().Peek()->Next2(
                             frame->descriptor_pool(), frame->message_factory(),
                             frame->arena(), key, value));
    if (!ok) {
      iter_slot->Clear();
      return frame->JumpTo(jump_offset_);
    }
    CEL_RETURN_IF_ERROR(frame->IncrementIterations());
    *iter_slot->mutable_attribute() = frame->value_stack().PeekAttribute().Step(
        AttributeQualifierFromValue(*key));
    if (frame->attribute_utility().CheckForUnknownExact(
            iter_slot->attribute())) {
      *iter_slot->mutable_value() = frame->attribute_utility().CreateUnknownSet(
          iter_slot->attribute().attribute());
    }
  } else {
    CEL_ASSIGN_OR_RETURN(bool ok,
                         frame->iterator_stack().Peek()->Next1(
                             frame->descriptor_pool(), frame->message_factory(),
                             frame->arena(), iter_slot->mutable_value()));
    if (!ok) {
      iter_slot->Clear();
      return frame->JumpTo(jump_offset_);
    }
    CEL_RETURN_IF_ERROR(frame->IncrementIterations());
  }
  return absl::OkStatus();
}

absl::Status ComprehensionNextStep::Evaluate2(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(2)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }

  {
    Value& accu_var = frame->value_stack().Peek();
    AttributeTrail& accu_var_attr = frame->value_stack().PeekAttribute();
    frame->comprehension_slots().Set(accu_slot_, std::move(accu_var),
                                     std::move(accu_var_attr));
    frame->value_stack().Pop(1);
  }

  ComprehensionSlots::Slot* iter_slot =
      frame->comprehension_slots().Get(iter_slot_);
  ABSL_DCHECK(iter_slot != nullptr);
  iter_slot->Set();

  ComprehensionSlots::Slot* iter2_slot =
      frame->comprehension_slots().Get(iter2_slot_);
  ABSL_DCHECK(iter2_slot != nullptr);
  iter2_slot->Set();

  CEL_ASSIGN_OR_RETURN(
      bool ok,
      frame->iterator_stack().Peek()->Next2(
          frame->descriptor_pool(), frame->message_factory(), frame->arena(),
          iter_slot->mutable_value(), iter2_slot->mutable_value()));
  if (!ok) {
    iter_slot->Clear();
    iter2_slot->Clear();
    return frame->JumpTo(jump_offset_);
  }
  CEL_RETURN_IF_ERROR(frame->IncrementIterations());
  if (frame->enable_unknowns()) {
    *iter_slot->mutable_attribute() = *iter2_slot->mutable_attribute() =
        frame->value_stack().PeekAttribute().Step(
            AttributeQualifierFromValue(iter_slot->value()));
    if (frame->attribute_utility().CheckForUnknownExact(
            iter2_slot->attribute())) {
      *iter2_slot->mutable_value() =
          frame->attribute_utility().CreateUnknownSet(
              iter2_slot->attribute().attribute());
    }
  }
  return absl::OkStatus();
}

absl::Status ComprehensionCondStep::Evaluate1(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(2)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }
  const Value& top = frame->value_stack().Peek();
  switch (top.kind()) {
    case ValueKind::kBool:
      break;
    case ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUnknown:
      frame->value_stack().SwapAndPop(2, 1);
      frame->comprehension_slots().ClearSlot(iter_slot_);
      frame->comprehension_slots().ClearSlot(accu_slot_);
      frame->iterator_stack().Pop();
      return frame->JumpTo(error_jump_offset_);
    default:
      frame->value_stack().PopAndPush(
          2,
          cel::ErrorValue(CreateNoMatchingOverloadError("<loop_condition>")));
      frame->comprehension_slots().ClearSlot(iter_slot_);
      frame->comprehension_slots().ClearSlot(accu_slot_);
      frame->iterator_stack().Pop();
      return frame->JumpTo(error_jump_offset_);
  }
  const bool loop_condition = absl::implicit_cast<bool>(top.GetBool());
  frame->value_stack().Pop(1);  // loop_condition
  if (!loop_condition && shortcircuiting_) {
    return frame->JumpTo(jump_offset_);
  }
  return absl::OkStatus();
}

absl::Status ComprehensionCondStep::Evaluate2(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(2)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }
  const Value& top = frame->value_stack().Peek();
  switch (top.kind()) {
    case ValueKind::kBool:
      break;
    case ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUnknown:
      frame->value_stack().SwapAndPop(2, 1);
      frame->comprehension_slots().ClearSlot(iter_slot_);
      frame->comprehension_slots().ClearSlot(iter2_slot_);
      frame->comprehension_slots().ClearSlot(accu_slot_);
      frame->iterator_stack().Pop();
      return frame->JumpTo(error_jump_offset_);
    default:
      frame->value_stack().PopAndPush(
          2,
          cel::ErrorValue(CreateNoMatchingOverloadError("<loop_condition>")));
      frame->comprehension_slots().ClearSlot(iter_slot_);
      frame->comprehension_slots().ClearSlot(iter2_slot_);
      frame->comprehension_slots().ClearSlot(accu_slot_);
      frame->iterator_stack().Pop();
      return frame->JumpTo(error_jump_offset_);
  }
  const bool loop_condition = absl::implicit_cast<bool>(top.GetBool());
  frame->value_stack().Pop(1);  // loop_condition
  if (!loop_condition && shortcircuiting_) {
    return frame->JumpTo(jump_offset_);
  }
  return absl::OkStatus();
}

std::unique_ptr<DirectExpressionStep> CreateDirectComprehensionStep(
    size_t iter_slot, size_t iter2_slot, size_t accu_slot,
    std::unique_ptr<DirectExpressionStep> range,
    std::unique_ptr<DirectExpressionStep> accu_init,
    std::unique_ptr<DirectExpressionStep> loop_step,
    std::unique_ptr<DirectExpressionStep> condition_step,
    std::unique_ptr<DirectExpressionStep> result_step, bool shortcircuiting,
    int64_t expr_id) {
  return std::make_unique<ComprehensionDirectStep>(
      iter_slot, iter2_slot, accu_slot, std::move(range), std::move(accu_init),
      std::move(loop_step), std::move(condition_step), std::move(result_step),
      shortcircuiting, expr_id);
}

std::unique_ptr<ExpressionStep> CreateComprehensionFinishStep(size_t accu_slot,
                                                              int64_t expr_id) {
  return std::make_unique<ComprehensionFinishStep>(accu_slot, expr_id);
}

}  // namespace google::api::expr::runtime
