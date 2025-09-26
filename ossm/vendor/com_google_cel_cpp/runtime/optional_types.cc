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
#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "base/function_adapter.h"
#include "common/casting.h"
#include "common/type.h"
#include "common/value.h"
#include "internal/casts.h"
#include "internal/number.h"
#include "internal/status_macros.h"
#include "runtime/function_registry.h"
#include "runtime/internal/errors.h"
#include "runtime/internal/runtime_friend_access.h"
#include "runtime/internal/runtime_impl.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::extensions {

namespace {

Value OptionalOf(const Value& value, const google::protobuf::DescriptorPool* absl_nonnull,
                 google::protobuf::MessageFactory* absl_nonnull,
                 google::protobuf::Arena* absl_nonnull arena) {
  return OptionalValue::Of(value, arena);
}

Value OptionalNone() { return OptionalValue::None(); }

Value OptionalOfNonZeroValue(
    const Value& value,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  if (value.IsZeroValue()) {
    return OptionalNone();
  }
  return OptionalOf(value, descriptor_pool, message_factory, arena);
}

absl::StatusOr<Value> OptionalGetValue(const OpaqueValue& opaque_value) {
  if (auto optional_value = opaque_value.AsOptional(); optional_value) {
    return optional_value->Value();
  }
  return ErrorValue{runtime_internal::CreateNoMatchingOverloadError("value")};
}

absl::StatusOr<Value> OptionalHasValue(const OpaqueValue& opaque_value) {
  if (auto optional_value = opaque_value.AsOptional(); optional_value) {
    return BoolValue{optional_value->HasValue()};
  }
  return ErrorValue{
      runtime_internal::CreateNoMatchingOverloadError("hasValue")};
}

absl::StatusOr<Value> SelectOptionalFieldStruct(
    const StructValue& struct_value, const StringValue& key,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  std::string field_name;
  auto field_name_view = key.NativeString(field_name);
  CEL_ASSIGN_OR_RETURN(auto has_field,
                       struct_value.HasFieldByName(field_name_view));
  if (!has_field) {
    return OptionalValue::None();
  }
  CEL_ASSIGN_OR_RETURN(
      auto field, struct_value.GetFieldByName(field_name_view, descriptor_pool,
                                              message_factory, arena));
  return OptionalValue::Of(std::move(field), arena);
}

absl::StatusOr<Value> SelectOptionalFieldMap(
    const MapValue& map, const StringValue& key,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  absl::optional<Value> value;
  CEL_ASSIGN_OR_RETURN(value,
                       map.Find(key, descriptor_pool, message_factory, arena));
  if (value) {
    return OptionalValue::Of(std::move(*value), arena);
  }
  return OptionalValue::None();
}

absl::StatusOr<Value> SelectOptionalField(
    const OpaqueValue& opaque_value, const StringValue& key,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  if (auto optional_value = opaque_value.AsOptional(); optional_value) {
    if (!optional_value->HasValue()) {
      return OptionalValue::None();
    }
    auto container = optional_value->Value();
    if (auto map_value = container.AsMap(); map_value) {
      return SelectOptionalFieldMap(*map_value, key, descriptor_pool,
                                    message_factory, arena);
    }
    if (auto struct_value = container.AsStruct(); struct_value) {
      return SelectOptionalFieldStruct(*struct_value, key, descriptor_pool,
                                       message_factory, arena);
    }
  }
  return ErrorValue{runtime_internal::CreateNoMatchingOverloadError("_[?_]")};
}

absl::StatusOr<Value> MapOptIndexOptionalValue(
    const MapValue& map, const Value& key,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  absl::optional<Value> value;
  if (auto double_key = cel::As<DoubleValue>(key); double_key) {
    // Try int/uint.
    auto number = internal::Number::FromDouble(double_key->NativeValue());
    if (number.LosslessConvertibleToInt()) {
      CEL_ASSIGN_OR_RETURN(value,
                           map.Find(IntValue{number.AsInt()}, descriptor_pool,
                                    message_factory, arena));
      if (value) {
        return OptionalValue::Of(std::move(*value), arena);
      }
    }
    if (number.LosslessConvertibleToUint()) {
      CEL_ASSIGN_OR_RETURN(value,
                           map.Find(UintValue{number.AsUint()}, descriptor_pool,
                                    message_factory, arena));
      if (value) {
        return OptionalValue::Of(std::move(*value), arena);
      }
    }
  } else {
    CEL_ASSIGN_OR_RETURN(
        value, map.Find(key, descriptor_pool, message_factory, arena));
    if (value) {
      return OptionalValue::Of(std::move(*value), arena);
    }
    if (auto int_key = key.AsInt(); int_key && int_key->NativeValue() >= 0) {
      CEL_ASSIGN_OR_RETURN(
          value,
          map.Find(UintValue{static_cast<uint64_t>(int_key->NativeValue())},
                   descriptor_pool, message_factory, arena));
      if (value) {
        return OptionalValue::Of(std::move(*value), arena);
      }
    } else if (auto uint_key = key.AsUint();
               uint_key &&
               uint_key->NativeValue() <=
                   static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
      CEL_ASSIGN_OR_RETURN(
          value,
          map.Find(IntValue{static_cast<int64_t>(uint_key->NativeValue())},
                   descriptor_pool, message_factory, arena));
      if (value) {
        return OptionalValue::Of(std::move(*value), arena);
      }
    }
  }
  return OptionalValue::None();
}

absl::StatusOr<Value> ListOptIndexOptionalInt(
    const ListValue& list, int64_t key,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  CEL_ASSIGN_OR_RETURN(auto list_size, list.Size());
  if (key < 0 || static_cast<size_t>(key) >= list_size) {
    return OptionalValue::None();
  }
  CEL_ASSIGN_OR_RETURN(auto element,
                       list.Get(static_cast<size_t>(key), descriptor_pool,
                                message_factory, arena));
  return OptionalValue::Of(std::move(element), arena);
}

absl::StatusOr<Value> OptionalOptIndexOptionalValue(
    const OpaqueValue& opaque_value, const Value& key,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  if (auto optional_value = As<OptionalValue>(opaque_value); optional_value) {
    if (!optional_value->HasValue()) {
      return OptionalValue::None();
    }
    auto container = optional_value->Value();
    if (auto map_value = cel::As<MapValue>(container); map_value) {
      return MapOptIndexOptionalValue(*map_value, key, descriptor_pool,
                                      message_factory, arena);
    }
    if (auto list_value = cel::As<ListValue>(container); list_value) {
      if (auto int_value = cel::As<IntValue>(key); int_value) {
        return ListOptIndexOptionalInt(*list_value, int_value->NativeValue(),
                                       descriptor_pool, message_factory, arena);
      }
    }
  }
  return ErrorValue{runtime_internal::CreateNoMatchingOverloadError("_[?_]")};
}

absl::StatusOr<Value> ListUnwrapOpt(
    const ListValue& list,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  auto builder = NewListValueBuilder(arena);
  CEL_ASSIGN_OR_RETURN(auto list_size, list.Size());
  builder->Reserve(list_size);

  absl::Status status = list.ForEach(
      [&](const Value& value) -> absl::StatusOr<bool> {
        if (auto optional_value = value.AsOptional(); optional_value) {
          if (optional_value->HasValue()) {
            CEL_RETURN_IF_ERROR(builder->Add(optional_value->Value()));
          }
        } else {
          return absl::InvalidArgumentError(absl::StrFormat(
              "optional.unwrap() expected a list(optional(T)), but %s "
              "was found in the list.",
              value.GetTypeName()));
        }
        return true;
      },
      descriptor_pool, message_factory, arena);
  if (!status.ok()) {
    return ErrorValue(status);
  }
  return std::move(*builder).Build();
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
      NullaryFunctionAdapter<Value>::CreateDescriptor("optional.none", false),
      NullaryFunctionAdapter<Value>::WrapFunction(&OptionalNone)));
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
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<absl::StatusOr<Value>, ListValue>::CreateDescriptor(
          "optional.unwrap", false),
      UnaryFunctionAdapter<absl::StatusOr<Value>, ListValue>::WrapFunction(
          &ListUnwrapOpt)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<absl::StatusOr<Value>, ListValue>::CreateDescriptor(
          "unwrapOpt", true),
      UnaryFunctionAdapter<absl::StatusOr<Value>, ListValue>::WrapFunction(
          &ListUnwrapOpt)));
  return absl::OkStatus();
}

}  // namespace

absl::Status EnableOptionalTypes(RuntimeBuilder& builder) {
  auto& runtime = cel::internal::down_cast<runtime_internal::RuntimeImpl&>(
      runtime_internal::RuntimeFriendAccess::GetMutableRuntime(builder));
  CEL_RETURN_IF_ERROR(RegisterOptionalTypeFunctions(
      builder.function_registry(), runtime.expr_builder().options()));
  CEL_RETURN_IF_ERROR(builder.type_registry().RegisterType(OptionalType()));
  runtime.expr_builder().enable_optional_types();
  return absl::OkStatus();
}

}  // namespace cel::extensions
