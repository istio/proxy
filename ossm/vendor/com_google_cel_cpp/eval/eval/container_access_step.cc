#include "eval/eval/container_access_step.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "base/ast_internal/expr.h"
#include "base/attribute.h"
#include "base/kind.h"
#include "common/casting.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/attribute_utility.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/internal/errors.h"
#include "internal/casts.h"
#include "internal/number.h"
#include "internal/status_macros.h"
#include "runtime/internal/errors.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::AttributeQualifier;
using ::cel::BoolValue;
using ::cel::Cast;
using ::cel::DoubleValue;
using ::cel::ErrorValue;
using ::cel::InstanceOf;
using ::cel::IntValue;
using ::cel::ListValue;
using ::cel::MapValue;
using ::cel::StringValue;
using ::cel::UintValue;
using ::cel::Value;
using ::cel::ValueKind;
using ::cel::ValueKindToString;
using ::cel::internal::Number;
using ::cel::runtime_internal::CreateNoSuchKeyError;

inline constexpr int kNumContainerAccessArguments = 2;

absl::optional<Number> CelNumberFromValue(const Value& value) {
  switch (value->kind()) {
    case ValueKind::kInt64:
      return Number::FromInt64(value.GetInt().NativeValue());
    case ValueKind::kUint64:
      return Number::FromUint64(value.GetUint().NativeValue());
    case ValueKind::kDouble:
      return Number::FromDouble(value.GetDouble().NativeValue());
    default:
      return absl::nullopt;
  }
}

absl::Status CheckMapKeyType(const Value& key) {
  ValueKind kind = key->kind();
  switch (kind) {
    case ValueKind::kString:
    case ValueKind::kInt64:
    case ValueKind::kUint64:
    case ValueKind::kBool:
      return absl::OkStatus();
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "Invalid map key type: '", ValueKindToString(kind), "'"));
  }
}

AttributeQualifier AttributeQualifierFromValue(const Value& v) {
  switch (v->kind()) {
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

void LookupInMap(const MapValue& cel_map, const Value& key,
                 ExecutionFrameBase& frame, Value& result) {
  if (frame.options().enable_heterogeneous_equality) {
    // Double isn't a supported key type but may be convertible to an integer.
    absl::optional<Number> number = CelNumberFromValue(key);
    if (number.has_value()) {
      // Consider uint as uint first then try coercion (prefer matching the
      // original type of the key value).
      if (key->Is<UintValue>()) {
        auto lookup = cel_map.Find(frame.value_manager(), key, result);
        if (!lookup.ok()) {
          result = frame.value_manager().CreateErrorValue(
              std::move(lookup).status());
          return;
        }
        if (*lookup) {
          return;
        }
      }
      // double / int / uint -> int
      if (number->LosslessConvertibleToInt()) {
        auto lookup = cel_map.Find(
            frame.value_manager(),
            frame.value_manager().CreateIntValue(number->AsInt()), result);
        if (!lookup.ok()) {
          result = frame.value_manager().CreateErrorValue(
              std::move(lookup).status());
          return;
        }
        if (*lookup) {
          return;
        }
      }
      // double / int -> uint
      if (number->LosslessConvertibleToUint()) {
        auto lookup = cel_map.Find(
            frame.value_manager(),
            frame.value_manager().CreateUintValue(number->AsUint()), result);
        if (!lookup.ok()) {
          result = frame.value_manager().CreateErrorValue(
              std::move(lookup).status());
          return;
        }
        if (*lookup) {
          return;
        }
      }
      result = frame.value_manager().CreateErrorValue(
          CreateNoSuchKeyError(key->DebugString()));
      return;
    }
  }

  absl::Status status = CheckMapKeyType(key);
  if (!status.ok()) {
    result = frame.value_manager().CreateErrorValue(std::move(status));
    return;
  }

  absl::Status lookup = cel_map.Get(frame.value_manager(), key, result);
  if (!lookup.ok()) {
    result = frame.value_manager().CreateErrorValue(std::move(lookup));
  }
}

void LookupInList(const ListValue& cel_list, const Value& key,
                  ExecutionFrameBase& frame, Value& result) {
  absl::optional<int64_t> maybe_idx;
  if (frame.options().enable_heterogeneous_equality) {
    auto number = CelNumberFromValue(key);
    if (number.has_value() && number->LosslessConvertibleToInt()) {
      maybe_idx = number->AsInt();
    }
  } else if (InstanceOf<IntValue>(key)) {
    maybe_idx = key.GetInt().NativeValue();
  }

  if (!maybe_idx.has_value()) {
    result = frame.value_manager().CreateErrorValue(absl::UnknownError(
        absl::StrCat("Index error: expected integer type, got ",
                     cel::KindToString(ValueKindToKind(key->kind())))));
    return;
  }

  int64_t idx = *maybe_idx;
  auto size = cel_list.Size();
  if (!size.ok()) {
    result = frame.value_manager().CreateErrorValue(size.status());
    return;
  }
  if (idx < 0 || idx >= *size) {
    result = frame.value_manager().CreateErrorValue(absl::UnknownError(
        absl::StrCat("Index error: index=", idx, " size=", *size)));
    return;
  }

  absl::Status lookup = cel_list.Get(frame.value_manager(), idx, result);

  if (!lookup.ok()) {
    result = frame.value_manager().CreateErrorValue(std::move(lookup));
  }
}

void LookupInContainer(const Value& container, const Value& key,
                       ExecutionFrameBase& frame, Value& result) {
  // Select steps can be applied to either maps or messages
  switch (container.kind()) {
    case ValueKind::kMap: {
      LookupInMap(Cast<MapValue>(container), key, frame, result);
      return;
    }
    case ValueKind::kList: {
      LookupInList(Cast<ListValue>(container), key, frame, result);
      return;
    }
    default:
      result =
          frame.value_manager().CreateErrorValue(absl::InvalidArgumentError(
              absl::StrCat("Invalid container type: '",
                           ValueKindToString(container->kind()), "'")));
      return;
  }
}

void PerformLookup(ExecutionFrameBase& frame, const Value& container,
                   const Value& key, const AttributeTrail& container_trail,
                   bool enable_optional_types, Value& result,
                   AttributeTrail& trail) {
  if (frame.unknown_processing_enabled()) {
    AttributeUtility::Accumulator unknowns =
        frame.attribute_utility().CreateAccumulator();
    unknowns.MaybeAdd(container);
    unknowns.MaybeAdd(key);

    if (!unknowns.IsEmpty()) {
      result = std::move(unknowns).Build();
      return;
    }

    trail = container_trail.Step(AttributeQualifierFromValue(key));

    if (frame.attribute_utility().CheckForUnknownExact(trail)) {
      result = frame.attribute_utility().CreateUnknownSet(trail.attribute());
      return;
    }
  }

  if (InstanceOf<ErrorValue>(container)) {
    result = container;
    return;
  }
  if (InstanceOf<ErrorValue>(key)) {
    result = key;
    return;
  }

  if (enable_optional_types &&
      cel::NativeTypeId::Of(container) ==
          cel::NativeTypeId::For<cel::OptionalValueInterface>()) {
    const auto& optional_value =
        *cel::internal::down_cast<const cel::OptionalValueInterface*>(
            cel::Cast<cel::OpaqueValue>(container).operator->());
    if (!optional_value.HasValue()) {
      result = cel::OptionalValue::None();
      return;
    }
    LookupInContainer(optional_value.Value(), key, frame, result);
    if (auto error_value = cel::As<cel::ErrorValue>(result);
        error_value && cel::IsNoSuchKey(*error_value)) {
      result = cel::OptionalValue::None();
      return;
    }
    result = cel::OptionalValue::Of(frame.value_manager().GetMemoryManager(),
                                    std::move(result));
    return;
  }

  LookupInContainer(container, key, frame, result);
}

// ContainerAccessStep performs message field access specified by Expr::Select
// message.
class ContainerAccessStep : public ExpressionStepBase {
 public:
  ContainerAccessStep(int64_t expr_id, bool enable_optional_types)
      : ExpressionStepBase(expr_id),
        enable_optional_types_(enable_optional_types) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  bool enable_optional_types_;
};

absl::Status ContainerAccessStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(kNumContainerAccessArguments)) {
    return absl::Status(
        absl::StatusCode::kInternal,
        "Insufficient arguments supplied for ContainerAccess-type expression");
  }

  Value result;
  AttributeTrail result_trail;
  auto args = frame->value_stack().GetSpan(kNumContainerAccessArguments);
  const AttributeTrail& container_trail =
      frame->value_stack().GetAttributeSpan(kNumContainerAccessArguments)[0];

  PerformLookup(*frame, args[0], args[1], container_trail,
                enable_optional_types_, result, result_trail);
  frame->value_stack().PopAndPush(kNumContainerAccessArguments,
                                  std::move(result), std::move(result_trail));

  return absl::OkStatus();
}

class DirectContainerAccessStep : public DirectExpressionStep {
 public:
  DirectContainerAccessStep(
      std::unique_ptr<DirectExpressionStep> container_step,
      std::unique_ptr<DirectExpressionStep> key_step,
      bool enable_optional_types, int64_t expr_id)
      : DirectExpressionStep(expr_id),
        container_step_(std::move(container_step)),
        key_step_(std::move(key_step)),
        enable_optional_types_(enable_optional_types) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, Value& result,
                        AttributeTrail& trail) const override;

 private:
  std::unique_ptr<DirectExpressionStep> container_step_;
  std::unique_ptr<DirectExpressionStep> key_step_;
  bool enable_optional_types_;
};

absl::Status DirectContainerAccessStep::Evaluate(ExecutionFrameBase& frame,
                                                 Value& result,
                                                 AttributeTrail& trail) const {
  Value container;
  Value key;
  AttributeTrail container_trail;
  AttributeTrail key_trail;

  CEL_RETURN_IF_ERROR(
      container_step_->Evaluate(frame, container, container_trail));
  CEL_RETURN_IF_ERROR(key_step_->Evaluate(frame, key, key_trail));

  PerformLookup(frame, container, key, container_trail, enable_optional_types_,
                result, trail);

  return absl::OkStatus();
}

}  // namespace

std::unique_ptr<DirectExpressionStep> CreateDirectContainerAccessStep(
    std::unique_ptr<DirectExpressionStep> container_step,
    std::unique_ptr<DirectExpressionStep> key_step, bool enable_optional_types,
    int64_t expr_id) {
  return std::make_unique<DirectContainerAccessStep>(
      std::move(container_step), std::move(key_step), enable_optional_types,
      expr_id);
}

// Factory method for Select - based Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateContainerAccessStep(
    const cel::ast_internal::Call& call, int64_t expr_id,
    bool enable_optional_types) {
  int arg_count = call.args().size() + (call.has_target() ? 1 : 0);
  if (arg_count != kNumContainerAccessArguments) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Invalid argument count for index operation: ", arg_count));
  }
  return std::make_unique<ContainerAccessStep>(expr_id, enable_optional_types);
}

}  // namespace google::api::expr::runtime
