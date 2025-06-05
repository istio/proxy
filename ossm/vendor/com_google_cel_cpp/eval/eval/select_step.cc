#include "eval/eval/select_step.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "base/kind.h"
#include "common/casting.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/internal/errors.h"
#include "internal/casts.h"
#include "internal/status_macros.h"
#include "runtime/runtime_options.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::BoolValue;
using ::cel::Cast;
using ::cel::ErrorValue;
using ::cel::InstanceOf;
using ::cel::MapValue;
using ::cel::NullValue;
using ::cel::OptionalValue;
using ::cel::ProtoWrapperTypeOptions;
using ::cel::StringValue;
using ::cel::StructValue;
using ::cel::UnknownValue;
using ::cel::Value;
using ::cel::ValueKind;

// Common error for cases where evaluation attempts to perform select operations
// on an unsupported type.
//
// This should not happen under normal usage of the evaluator, but useful for
// troubleshooting broken invariants.
absl::Status InvalidSelectTargetError() {
  return absl::Status(absl::StatusCode::kInvalidArgument,
                      "Applying SELECT to non-message type");
}

absl::optional<Value> CheckForMarkedAttributes(const AttributeTrail& trail,
                                               ExecutionFrameBase& frame) {
  if (frame.unknown_processing_enabled() &&
      frame.attribute_utility().CheckForUnknownExact(trail)) {
    return frame.attribute_utility().CreateUnknownSet(trail.attribute());
  }

  if (frame.missing_attribute_errors_enabled() &&
      frame.attribute_utility().CheckForMissingAttribute(trail)) {
    auto result = frame.attribute_utility().CreateMissingAttributeError(
        trail.attribute());

    if (result.ok()) {
      return std::move(result).value();
    }
    // Invariant broken (an invalid CEL Attribute shouldn't match anything).
    // Log and return a CelError.
    ABSL_LOG(ERROR) << "Invalid attribute pattern matched select path: "
                    << result.status().ToString();  // NOLINT: OSS compatibility
    return frame.value_manager().CreateErrorValue(std::move(result).status());
  }

  return absl::nullopt;
}

void TestOnlySelect(const StructValue& msg, const std::string& field,
                    cel::ValueManager& value_factory, Value& result) {
  absl::StatusOr<bool> has_field = msg.HasFieldByName(field);

  if (!has_field.ok()) {
    result = value_factory.CreateErrorValue(std::move(has_field).status());
    return;
  }
  result = BoolValue{*has_field};
}

void TestOnlySelect(const MapValue& map, const StringValue& field_name,
                    cel::ValueManager& value_factory, Value& result) {
  // Field presence only supports string keys containing valid identifier
  // characters.
  absl::Status presence = map.Has(value_factory, field_name, result);

  if (!presence.ok()) {
    result = value_factory.CreateErrorValue(std::move(presence));
    return;
  }
}

// SelectStep performs message field access specified by Expr::Select
// message.
class SelectStep : public ExpressionStepBase {
 public:
  SelectStep(StringValue value, bool test_field_presence, int64_t expr_id,
             bool enable_wrapper_type_null_unboxing, bool enable_optional_types)
      : ExpressionStepBase(expr_id),
        field_value_(std::move(value)),
        field_(field_value_.ToString()),
        test_field_presence_(test_field_presence),
        unboxing_option_(enable_wrapper_type_null_unboxing
                             ? ProtoWrapperTypeOptions::kUnsetNull
                             : ProtoWrapperTypeOptions::kUnsetProtoDefault),
        enable_optional_types_(enable_optional_types) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  absl::Status PerformTestOnlySelect(ExecutionFrame* frame,
                                     const Value& arg) const;
  absl::StatusOr<bool> PerformSelect(ExecutionFrame* frame, const Value& arg,
                                     Value& result) const;

  cel::StringValue field_value_;
  std::string field_;
  bool test_field_presence_;
  ProtoWrapperTypeOptions unboxing_option_;
  bool enable_optional_types_;
};

absl::Status SelectStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(1)) {
    return absl::Status(absl::StatusCode::kInternal,
                        "No arguments supplied for Select-type expression");
  }

  const Value& arg = frame->value_stack().Peek();
  const AttributeTrail& trail = frame->value_stack().PeekAttribute();

  if (InstanceOf<UnknownValue>(arg) || InstanceOf<ErrorValue>(arg)) {
    // Bubble up unknowns and errors.
    return absl::OkStatus();
  }

  AttributeTrail result_trail;

  // Handle unknown resolution.
  if (frame->enable_unknowns() || frame->enable_missing_attribute_errors()) {
    result_trail = trail.Step(&field_);
  }

  if (arg->Is<NullValue>()) {
    frame->value_stack().PopAndPush(
        frame->value_factory().CreateErrorValue(
            cel::runtime_internal::CreateError("Message is NULL")),
        std::move(result_trail));
    return absl::OkStatus();
  }

  const cel::OptionalValueInterface* optional_arg = nullptr;

  if (enable_optional_types_ &&
      cel::NativeTypeId::Of(arg) ==
          cel::NativeTypeId::For<cel::OptionalValueInterface>()) {
    optional_arg = cel::internal::down_cast<const cel::OptionalValueInterface*>(
        cel::Cast<cel::OpaqueValue>(arg).operator->());
  }

  if (!(optional_arg != nullptr || arg->Is<MapValue>() ||
        arg->Is<StructValue>())) {
    frame->value_stack().PopAndPush(
        frame->value_factory().CreateErrorValue(InvalidSelectTargetError()),
        std::move(result_trail));
    return absl::OkStatus();
  }

  absl::optional<Value> marked_attribute_check =
      CheckForMarkedAttributes(result_trail, *frame);
  if (marked_attribute_check.has_value()) {
    frame->value_stack().PopAndPush(std::move(marked_attribute_check).value(),
                                    std::move(result_trail));
    return absl::OkStatus();
  }

  // Handle test only Select.
  if (test_field_presence_) {
    if (optional_arg != nullptr) {
      if (!optional_arg->HasValue()) {
        frame->value_stack().PopAndPush(cel::BoolValue{false});
        return absl::OkStatus();
      }
      return PerformTestOnlySelect(frame, optional_arg->Value());
    }
    return PerformTestOnlySelect(frame, arg);
  }

  // Normal select path.
  // Select steps can be applied to either maps or messages
  if (optional_arg != nullptr) {
    if (!optional_arg->HasValue()) {
      // Leave optional_arg at the top of the stack. Its empty.
      return absl::OkStatus();
    }
    Value result;
    bool ok;
    CEL_ASSIGN_OR_RETURN(ok,
                         PerformSelect(frame, optional_arg->Value(), result));
    if (!ok) {
      frame->value_stack().PopAndPush(cel::OptionalValue::None(),
                                      std::move(result_trail));
      return absl::OkStatus();
    }
    frame->value_stack().PopAndPush(
        cel::OptionalValue::Of(frame->memory_manager(), std::move(result)),
        std::move(result_trail));
    return absl::OkStatus();
  }

  // Normal select path.
  // Select steps can be applied to either maps or messages
  switch (arg->kind()) {
    case ValueKind::kStruct: {
      Value result;
      auto status = arg.GetStruct().GetFieldByName(
          frame->value_factory(), field_, result, unboxing_option_);
      if (!status.ok()) {
        result = ErrorValue(std::move(status));
      }
      frame->value_stack().PopAndPush(std::move(result),
                                      std::move(result_trail));
      return absl::OkStatus();
    }
    case ValueKind::kMap: {
      Value result;
      auto status =
          arg.GetMap().Get(frame->value_factory(), field_value_, result);
      if (!status.ok()) {
        result = ErrorValue(std::move(status));
      }
      frame->value_stack().PopAndPush(std::move(result),
                                      std::move(result_trail));
      return absl::OkStatus();
    }
    default:
      // Control flow should have returned earlier.
      return InvalidSelectTargetError();
  }
}

absl::Status SelectStep::PerformTestOnlySelect(ExecutionFrame* frame,
                                               const Value& arg) const {
  switch (arg->kind()) {
    case ValueKind::kMap: {
      Value result;
      TestOnlySelect(arg.GetMap(), field_value_, frame->value_factory(),
                     result);
      frame->value_stack().PopAndPush(std::move(result));
      return absl::OkStatus();
    }
    case ValueKind::kMessage: {
      Value result;
      TestOnlySelect(arg.GetStruct(), field_, frame->value_factory(), result);
      frame->value_stack().PopAndPush(std::move(result));
      return absl::OkStatus();
    }
    default:
      // Control flow should have returned earlier.
      return InvalidSelectTargetError();
  }
}

absl::StatusOr<bool> SelectStep::PerformSelect(ExecutionFrame* frame,
                                               const Value& arg,
                                               Value& result) const {
  switch (arg->kind()) {
    case ValueKind::kStruct: {
      const auto& struct_value = arg.GetStruct();
      CEL_ASSIGN_OR_RETURN(auto ok, struct_value.HasFieldByName(field_));
      if (!ok) {
        result = NullValue{};
        return false;
      }
      CEL_RETURN_IF_ERROR(struct_value.GetFieldByName(
          frame->value_factory(), field_, result, unboxing_option_));
      return true;
    }
    case ValueKind::kMap: {
      return arg.GetMap().Find(frame->value_factory(), field_value_, result);
    }
    default:
      // Control flow should have returned earlier.
      return InvalidSelectTargetError();
  }
}

class DirectSelectStep : public DirectExpressionStep {
 public:
  DirectSelectStep(int64_t expr_id,
                   std::unique_ptr<DirectExpressionStep> operand,
                   StringValue field, bool test_only,
                   bool enable_wrapper_type_null_unboxing,
                   bool enable_optional_types)
      : DirectExpressionStep(expr_id),
        operand_(std::move(operand)),
        field_value_(std::move(field)),
        field_(field_value_.ToString()),
        test_only_(test_only),
        unboxing_option_(enable_wrapper_type_null_unboxing
                             ? ProtoWrapperTypeOptions::kUnsetNull
                             : ProtoWrapperTypeOptions::kUnsetProtoDefault),
        enable_optional_types_(enable_optional_types) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, Value& result,
                        AttributeTrail& attribute) const override {
    CEL_RETURN_IF_ERROR(operand_->Evaluate(frame, result, attribute));

    if (InstanceOf<ErrorValue>(result) || InstanceOf<UnknownValue>(result)) {
      // Just forward.
      return absl::OkStatus();
    }

    if (frame.attribute_tracking_enabled()) {
      attribute = attribute.Step(&field_);
      absl::optional<Value> value = CheckForMarkedAttributes(attribute, frame);
      if (value.has_value()) {
        result = std::move(value).value();
        return absl::OkStatus();
      }
    }

    const cel::OptionalValueInterface* optional_arg = nullptr;

    if (enable_optional_types_ &&
        cel::NativeTypeId::Of(result) ==
            cel::NativeTypeId::For<cel::OptionalValueInterface>()) {
      optional_arg =
          cel::internal::down_cast<const cel::OptionalValueInterface*>(
              cel::Cast<cel::OpaqueValue>(result).operator->());
    }

    switch (result.kind()) {
      case ValueKind::kStruct:
      case ValueKind::kMap:
        break;
      case ValueKind::kNull:
        result = frame.value_manager().CreateErrorValue(
            cel::runtime_internal::CreateError("Message is NULL"));
        return absl::OkStatus();
      default:
        if (optional_arg != nullptr) {
          break;
        }
        result =
            frame.value_manager().CreateErrorValue(InvalidSelectTargetError());
        return absl::OkStatus();
    }

    if (test_only_) {
      if (optional_arg != nullptr) {
        if (!optional_arg->HasValue()) {
          result = cel::BoolValue{false};
          return absl::OkStatus();
        }
        PerformTestOnlySelect(frame, optional_arg->Value(), result);
        return absl::OkStatus();
      }
      PerformTestOnlySelect(frame, result, result);
      return absl::OkStatus();
    }

    if (optional_arg != nullptr) {
      if (!optional_arg->HasValue()) {
        // result is still buffer for the container. just return.
        return absl::OkStatus();
      }
      return PerformOptionalSelect(frame, optional_arg->Value(), result);
    }

    auto status = PerformSelect(frame, result, result);
    if (!status.ok()) {
      result = ErrorValue(std::move(status));
    }
    return absl::OkStatus();
  }

 private:
  std::unique_ptr<DirectExpressionStep> operand_;

  void PerformTestOnlySelect(ExecutionFrameBase& frame, const Value& value,
                             Value& result) const;
  absl::Status PerformOptionalSelect(ExecutionFrameBase& frame,
                                     const Value& value, Value& result) const;
  absl::Status PerformSelect(ExecutionFrameBase& frame, const Value& value,
                             Value& result) const;

  // Field name in formats supported by each of the map and struct field access
  // APIs.
  //
  // ToString or ValueManager::CreateString may force a copy so we do this at
  // plan time.
  StringValue field_value_;
  std::string field_;

  // whether this is a has() expression.
  bool test_only_;
  ProtoWrapperTypeOptions unboxing_option_;
  bool enable_optional_types_;
};

void DirectSelectStep::PerformTestOnlySelect(ExecutionFrameBase& frame,
                                             const cel::Value& value,
                                             Value& result) const {
  switch (value.kind()) {
    case ValueKind::kMap:
      TestOnlySelect(Cast<MapValue>(value), field_value_, frame.value_manager(),
                     result);
      return;
    case ValueKind::kMessage:
      TestOnlySelect(Cast<StructValue>(value), field_, frame.value_manager(),
                     result);
      return;
    default:
      // Control flow should have returned earlier.
      result =
          frame.value_manager().CreateErrorValue(InvalidSelectTargetError());
      return;
  }
}

absl::Status DirectSelectStep::PerformOptionalSelect(ExecutionFrameBase& frame,
                                                     const Value& value,
                                                     Value& result) const {
  switch (value.kind()) {
    case ValueKind::kStruct: {
      auto struct_value = Cast<StructValue>(value);
      CEL_ASSIGN_OR_RETURN(auto ok, struct_value.HasFieldByName(field_));
      if (!ok) {
        result = OptionalValue::None();
        return absl::OkStatus();
      }
      CEL_RETURN_IF_ERROR(struct_value.GetFieldByName(
          frame.value_manager(), field_, result, unboxing_option_));
      result = OptionalValue::Of(frame.value_manager().GetMemoryManager(),
                                 std::move(result));
      return absl::OkStatus();
    }
    case ValueKind::kMap: {
      CEL_ASSIGN_OR_RETURN(auto found,
                           Cast<MapValue>(value).Find(frame.value_manager(),
                                                      field_value_, result));
      if (!found) {
        result = OptionalValue::None();
        return absl::OkStatus();
      }
      result = OptionalValue::Of(frame.value_manager().GetMemoryManager(),
                                 std::move(result));
      return absl::OkStatus();
    }
    default:
      // Control flow should have returned earlier.
      return InvalidSelectTargetError();
  }
}

absl::Status DirectSelectStep::PerformSelect(ExecutionFrameBase& frame,
                                             const cel::Value& value,
                                             Value& result) const {
  switch (value.kind()) {
    case ValueKind::kStruct:
      return Cast<StructValue>(value).GetFieldByName(
          frame.value_manager(), field_, result, unboxing_option_);
    case ValueKind::kMap:
      return Cast<MapValue>(value).Get(frame.value_manager(), field_value_,
                                       result);
    default:
      // Control flow should have returned earlier.
      return InvalidSelectTargetError();
  }
}

}  // namespace

std::unique_ptr<DirectExpressionStep> CreateDirectSelectStep(
    std::unique_ptr<DirectExpressionStep> operand, StringValue field,
    bool test_only, int64_t expr_id, bool enable_wrapper_type_null_unboxing,
    bool enable_optional_types) {
  return std::make_unique<DirectSelectStep>(
      expr_id, std::move(operand), std::move(field), test_only,
      enable_wrapper_type_null_unboxing, enable_optional_types);
}

// Factory method for Select - based Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateSelectStep(
    const cel::ast_internal::Select& select_expr, int64_t expr_id,
    bool enable_wrapper_type_null_unboxing, cel::ValueManager& value_factory,
    bool enable_optional_types) {
  return std::make_unique<SelectStep>(
      value_factory.CreateUncheckedStringValue(select_expr.field()),
      select_expr.test_only(), expr_id, enable_wrapper_type_null_unboxing,
      enable_optional_types);
}

}  // namespace google::api::expr::runtime
