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

#include "runtime/optional_types.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "base/function_adapter.h"
#include "common/casting.h"
#include "common/type.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "internal/casts.h"
#include "internal/number.h"
#include "internal/status_macros.h"
#include "runtime/function_registry.h"
#include "runtime/internal/errors.h"
#include "runtime/internal/runtime_friend_access.h"
#include "runtime/internal/runtime_impl.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"

namespace cel::extensions {

namespace {

Value OptionalOf(ValueManager& value_manager, const Value& value) {
  return OptionalValue::Of(value_manager.GetMemoryManager(), value);
}

Value OptionalNone(ValueManager&) { return OptionalValue::None(); }

Value OptionalOfNonZeroValue(ValueManager& value_manager, const Value& value) {
  if (value.IsZeroValue()) {
    return OptionalNone(value_manager);
  }
  return OptionalOf(value_manager, value);
}

absl::StatusOr<Value> OptionalGetValue(ValueManager& value_manager,
                                       const OpaqueValue& opaque_value) {
  if (auto optional_value = As<OptionalValue>(opaque_value); optional_value) {
    return optional_value->Value();
  }
  return ErrorValue{runtime_internal::CreateNoMatchingOverloadError("value")};
}

absl::StatusOr<Value> OptionalHasValue(ValueManager& value_manager,
                                       const OpaqueValue& opaque_value) {
  if (auto optional_value = As<OptionalValue>(opaque_value); optional_value) {
    return BoolValue{optional_value->HasValue()};
  }
  return ErrorValue{
      runtime_internal::CreateNoMatchingOverloadError("hasValue")};
}

absl::StatusOr<Value> SelectOptionalFieldStruct(ValueManager& value_manager,
                                                const StructValue& struct_value,
                                                const StringValue& key) {
  std::string field_name;
  auto field_name_view = key.NativeString(field_name);
  CEL_ASSIGN_OR_RETURN(auto has_field,
                       struct_value.HasFieldByName(field_name_view));
  if (!has_field) {
    return OptionalValue::None();
  }
  CEL_ASSIGN_OR_RETURN(
      auto field, struct_value.GetFieldByName(value_manager, field_name_view));
  return OptionalValue::Of(value_manager.GetMemoryManager(), std::move(field));
}

absl::StatusOr<Value> SelectOptionalFieldMap(ValueManager& value_manager,
                                             const MapValue& map,
                                             const StringValue& key) {
  Value value;
  bool ok;
  CEL_ASSIGN_OR_RETURN(std::tie(value, ok), map.Find(value_manager, key));
  if (ok) {
    return OptionalValue::Of(value_manager.GetMemoryManager(),
                             std::move(value));
  }
  return OptionalValue::None();
}

absl::StatusOr<Value> SelectOptionalField(ValueManager& value_manager,
                                          const OpaqueValue& opaque_value,
                                          const StringValue& key) {
  if (auto optional_value = As<OptionalValue>(opaque_value); optional_value) {
    if (!optional_value->HasValue()) {
      return OptionalValue::None();
    }
    auto container = optional_value->Value();
    if (auto map_value = As<MapValue>(container); map_value) {
      return SelectOptionalFieldMap(value_manager, *map_value, key);
    }
    if (auto struct_value = As<StructValue>(container); struct_value) {
      return SelectOptionalFieldStruct(value_manager, *struct_value, key);
    }
  }
  return ErrorValue{runtime_internal::CreateNoMatchingOverloadError("_[?_]")};
}

absl::StatusOr<Value> MapOptIndexOptionalValue(ValueManager& value_manager,
                                               const MapValue& map,
                                               const Value& key) {
  Value value;
  bool ok;
  if (auto double_key = cel::As<DoubleValue>(key); double_key) {
    // Try int/uint.
    auto number = internal::Number::FromDouble(double_key->NativeValue());
    if (number.LosslessConvertibleToInt()) {
      CEL_ASSIGN_OR_RETURN(std::tie(value, ok),
                           map.Find(value_manager, IntValue{number.AsInt()}));
      if (ok) {
        return OptionalValue::Of(value_manager.GetMemoryManager(),
                                 std::move(value));
      }
    }
    if (number.LosslessConvertibleToUint()) {
      CEL_ASSIGN_OR_RETURN(std::tie(value, ok),
                           map.Find(value_manager, UintValue{number.AsUint()}));
      if (ok) {
        return OptionalValue::Of(value_manager.GetMemoryManager(),
                                 std::move(value));
      }
    }
  } else {
    CEL_ASSIGN_OR_RETURN(std::tie(value, ok), map.Find(value_manager, key));
    if (ok) {
      return OptionalValue::Of(value_manager.GetMemoryManager(),
                               std::move(value));
    }
    if (auto int_key = cel::As<IntValue>(key);
        int_key && int_key->NativeValue() >= 0) {
      CEL_ASSIGN_OR_RETURN(
          std::tie(value, ok),
          map.Find(value_manager,
                   UintValue{static_cast<uint64_t>(int_key->NativeValue())}));
      if (ok) {
        return OptionalValue::Of(value_manager.GetMemoryManager(),
                                 std::move(value));
      }
    } else if (auto uint_key = cel::As<UintValue>(key);
               uint_key &&
               uint_key->NativeValue() <=
                   static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
      CEL_ASSIGN_OR_RETURN(
          std::tie(value, ok),
          map.Find(value_manager,
                   IntValue{static_cast<int64_t>(uint_key->NativeValue())}));
      if (ok) {
        return OptionalValue::Of(value_manager.GetMemoryManager(),
                                 std::move(value));
      }
    }
  }
  return OptionalValue::None();
}

absl::StatusOr<Value> ListOptIndexOptionalInt(ValueManager& value_manager,
                                              const ListValue& list,
                                              int64_t key) {
  CEL_ASSIGN_OR_RETURN(auto list_size, list.Size());
  if (key < 0 || static_cast<size_t>(key) >= list_size) {
    return OptionalValue::None();
  }
  CEL_ASSIGN_OR_RETURN(auto element,
                       list.Get(value_manager, static_cast<size_t>(key)));
  return OptionalValue::Of(value_manager.GetMemoryManager(),
                           std::move(element));
}

absl::StatusOr<Value> OptionalOptIndexOptionalValue(
    ValueManager& value_manager, const OpaqueValue& opaque_value,
    const Value& key) {
  if (auto optional_value = As<OptionalValue>(opaque_value); optional_value) {
    if (!optional_value->HasValue()) {
      return OptionalValue::None();
    }
    auto container = optional_value->Value();
    if (auto map_value = cel::As<MapValue>(container); map_value) {
      return MapOptIndexOptionalValue(value_manager, *map_value, key);
    }
    if (auto list_value = cel::As<ListValue>(container); list_value) {
      if (auto int_value = cel::As<IntValue>(key); int_value) {
        return ListOptIndexOptionalInt(value_manager, *list_value,
                                       int_value->NativeValue());
      }
    }
  }
  return ErrorValue{runtime_internal::CreateNoMatchingOverloadError("_[?_]")};
}

absl::Status RegisterOptionalTypeFunctions(FunctionRegistry& registry,
                                           const RuntimeOptions& options) {
  if (!options.enable_qualified_type_identifiers) {
    return absl::FailedPreconditionError(
        "optional_type requires "
        "RuntimeOptions.enable_qualified_type_identifiers");
  }
  if (!options.enable_heterogeneous_equality) {
    return absl::FailedPreconditionError(
        "optional_type requires RuntimeOptions.enable_heterogeneous_equality");
  }
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, Value>::CreateDescriptor("optional.of",
                                                           false),
      UnaryFunctionAdapter<Value, Value>::WrapFunction(&OptionalOf)));
  CEL_RETURN_IF_ERROR(
      registry.Register(UnaryFunctionAdapter<Value, Value>::CreateDescriptor(
                            "optional.ofNonZeroValue", false),
                        UnaryFunctionAdapter<Value, Value>::WrapFunction(
                            &OptionalOfNonZeroValue)));
  CEL_RETURN_IF_ERROR(registry.Register(
      VariadicFunctionAdapter<Value>::CreateDescriptor("optional.none", false),
      VariadicFunctionAdapter<Value>::WrapFunction(&OptionalNone)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<absl::StatusOr<Value>,
                           OpaqueValue>::CreateDescriptor("value", true),
      UnaryFunctionAdapter<absl::StatusOr<Value>, OpaqueValue>::WrapFunction(
          &OptionalGetValue)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<absl::StatusOr<Value>,
                           OpaqueValue>::CreateDescriptor("hasValue", true),
      UnaryFunctionAdapter<absl::StatusOr<Value>, OpaqueValue>::WrapFunction(
          &OptionalHasValue)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<absl::StatusOr<Value>, StructValue,
                            StringValue>::CreateDescriptor("_?._", false),
      BinaryFunctionAdapter<absl::StatusOr<Value>, StructValue, StringValue>::
          WrapFunction(&SelectOptionalFieldStruct)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<absl::StatusOr<Value>, MapValue,
                            StringValue>::CreateDescriptor("_?._", false),
      BinaryFunctionAdapter<absl::StatusOr<Value>, MapValue, StringValue>::
          WrapFunction(&SelectOptionalFieldMap)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<absl::StatusOr<Value>, OpaqueValue,
                            StringValue>::CreateDescriptor("_?._", false),
      BinaryFunctionAdapter<absl::StatusOr<Value>, OpaqueValue,
                            StringValue>::WrapFunction(&SelectOptionalField)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<absl::StatusOr<Value>, MapValue,
                            Value>::CreateDescriptor("_[?_]", false),
      BinaryFunctionAdapter<absl::StatusOr<Value>, MapValue,
                            Value>::WrapFunction(&MapOptIndexOptionalValue)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<absl::StatusOr<Value>, ListValue,
                            int64_t>::CreateDescriptor("_[?_]", false),
      BinaryFunctionAdapter<absl::StatusOr<Value>, ListValue,
                            int64_t>::WrapFunction(&ListOptIndexOptionalInt)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<absl::StatusOr<Value>, OpaqueValue,
                            Value>::CreateDescriptor("_[?_]", false),
      BinaryFunctionAdapter<absl::StatusOr<Value>, OpaqueValue, Value>::
          WrapFunction(&OptionalOptIndexOptionalValue)));
  return absl::OkStatus();
}

class OptionalTypeProvider final : public TypeReflector {
 protected:
  absl::StatusOr<absl::optional<Type>> FindTypeImpl(
      TypeFactory&, absl::string_view name) const override {
    if (name != "optional_type") {
      return absl::nullopt;
    }
    return OptionalType{};
  }
};

}  // namespace

absl::Status EnableOptionalTypes(RuntimeBuilder& builder) {
  auto& runtime = cel::internal::down_cast<runtime_internal::RuntimeImpl&>(
      runtime_internal::RuntimeFriendAccess::GetMutableRuntime(builder));
  CEL_RETURN_IF_ERROR(RegisterOptionalTypeFunctions(
      builder.function_registry(), runtime.expr_builder().options()));
  builder.type_registry().AddTypeProvider(
      std::make_unique<OptionalTypeProvider>());
  runtime.expr_builder().enable_optional_types();
  return absl::OkStatus();
}

}  // namespace cel::extensions
