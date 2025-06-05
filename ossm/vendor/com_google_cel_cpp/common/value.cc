// Copyright 2023 Google LLC
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

#include "common/value.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "google/protobuf/struct.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/functional/overload.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "base/attribute.h"
#include "common/allocator.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/optional_ref.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/values.h"
#include "internal/number.h"
#include "internal/protobuf_runtime_version.h"
#include "internal/status_macros.h"
#include "internal/well_known_types.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {
namespace {

static constexpr std::array<ValueKind, 25> kValueToKindArray = {
    ValueKind::kError,     ValueKind::kBool,     ValueKind::kBytes,
    ValueKind::kDouble,    ValueKind::kDuration, ValueKind::kError,
    ValueKind::kInt,       ValueKind::kList,     ValueKind::kList,
    ValueKind::kList,      ValueKind::kList,     ValueKind::kMap,
    ValueKind::kMap,       ValueKind::kMap,      ValueKind::kMap,
    ValueKind::kNull,      ValueKind::kOpaque,   ValueKind::kString,
    ValueKind::kStruct,    ValueKind::kStruct,   ValueKind::kStruct,
    ValueKind::kTimestamp, ValueKind::kType,     ValueKind::kUint,
    ValueKind::kUnknown};

static_assert(kValueToKindArray.size() ==
                  absl::variant_size<common_internal::ValueVariant>(),
              "Kind indexer must match variant declaration for cel::Value.");

}  // namespace

Type Value::GetRuntimeType() const {
  AssertIsValid();
  switch (kind()) {
    case ValueKind::kNull:
      return NullType();
    case ValueKind::kBool:
      return BoolType();
    case ValueKind::kInt:
      return IntType();
    case ValueKind::kUint:
      return UintType();
    case ValueKind::kDouble:
      return DoubleType();
    case ValueKind::kString:
      return StringType();
    case ValueKind::kBytes:
      return BytesType();
    case ValueKind::kStruct:
      return this->GetStruct().GetRuntimeType();
    case ValueKind::kDuration:
      return DurationType();
    case ValueKind::kTimestamp:
      return TimestampType();
    case ValueKind::kList:
      return ListType();
    case ValueKind::kMap:
      return MapType();
    case ValueKind::kUnknown:
      return UnknownType();
    case ValueKind::kType:
      return TypeType();
    case ValueKind::kError:
      return ErrorType();
    case ValueKind::kOpaque:
      return this->GetOpaque().GetRuntimeType();
    default:
      return cel::Type();
  }
}

ValueKind Value::kind() const {
  ABSL_DCHECK_NE(variant_.index(), 0)
      << "kind() called on uninitialized cel::Value.";
  return kValueToKindArray[variant_.index()];
}

namespace {

template <typename T>
struct IsMonostate : std::is_same<absl::remove_cvref_t<T>, absl::monostate> {};

}  // namespace

absl::string_view Value::GetTypeName() const {
  AssertIsValid();
  return absl::visit(
      [](const auto& alternative) -> absl::string_view {
        if constexpr (IsMonostate<decltype(alternative)>::value) {
          // In optimized builds, we just return an empty string. In debug
          // builds we cannot reach here.
          return absl::string_view();
        } else {
          return alternative.GetTypeName();
        }
      },
      variant_);
}

std::string Value::DebugString() const {
  AssertIsValid();
  return absl::visit(
      [](const auto& alternative) -> std::string {
        if constexpr (IsMonostate<decltype(alternative)>::value) {
          // In optimized builds, we just return an empty string. In debug
          // builds we cannot reach here.
          return std::string();
        } else {
          return alternative.DebugString();
        }
      },
      variant_);
}

absl::Status Value::SerializeTo(AnyToJsonConverter& value_manager,
                                absl::Cord& value) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, &value](const auto& alternative) -> absl::Status {
        if constexpr (IsMonostate<decltype(alternative)>::value) {
          // In optimized builds, we just return an error. In debug builds we
          // cannot reach here.
          return absl::InternalError("use of invalid Value");
        } else {
          return alternative.SerializeTo(value_manager, value);
        }
      },
      variant_);
}

absl::StatusOr<Json> Value::ConvertToJson(
    AnyToJsonConverter& value_manager) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager](const auto& alternative) -> absl::StatusOr<Json> {
        if constexpr (IsMonostate<decltype(alternative)>::value) {
          // In optimized builds, we just return an error. In debug
          // builds we cannot reach here.
          return absl::InternalError("use of invalid Value");
        } else {
          return alternative.ConvertToJson(value_manager);
        }
      },
      variant_);
}

absl::Status Value::Equal(ValueManager& value_manager, const Value& other,
                          Value& result) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, &other,
       &result](const auto& alternative) -> absl::Status {
        if constexpr (IsMonostate<decltype(alternative)>::value) {
          // In optimized builds, we just return an error. In debug
          // builds we cannot reach here.
          return absl::InternalError("use of invalid Value");
        } else {
          return alternative.Equal(value_manager, other, result);
        }
      },
      variant_);
}

absl::StatusOr<Value> Value::Equal(ValueManager& value_manager,
                                   const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

bool Value::IsZeroValue() const {
  AssertIsValid();
  return absl::visit(
      [](const auto& alternative) -> bool {
        if constexpr (IsMonostate<decltype(alternative)>::value) {
          // In optimized builds, we just return false. In debug
          // builds we cannot reach here.
          return false;
        } else {
          return alternative.IsZeroValue();
        }
      },
      variant_);
}

namespace {

template <typename, typename = void>
struct HasCloneMethod : std::false_type {};

template <typename T>
struct HasCloneMethod<T, std::void_t<decltype(std::declval<const T>().Clone(
                             std::declval<Allocator<>>()))>> : std::true_type {
};

}  // namespace

Value Value::Clone(Allocator<> allocator) const {
  AssertIsValid();
  return absl::visit(
      [allocator](const auto& alternative) -> Value {
        if constexpr (IsMonostate<decltype(alternative)>::value) {
          return Value();
        } else if constexpr (HasCloneMethod<absl::remove_cvref_t<
                                 decltype(alternative)>>::value) {
          return alternative.Clone(allocator);
        } else {
          return alternative;
        }
      },
      variant_);
}

void swap(Value& lhs, Value& rhs) noexcept { lhs.variant_.swap(rhs.variant_); }

std::ostream& operator<<(std::ostream& out, const Value& value) {
  return absl::visit(
      [&out](const auto& alternative) -> std::ostream& {
        if constexpr (IsMonostate<decltype(alternative)>::value) {
          return out << "default ctor Value";
        } else {
          return out << alternative;
        }
      },
      value.variant_);
}

absl::StatusOr<Value> BytesValue::Equal(ValueManager& value_manager,
                                        const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> ErrorValue::Equal(ValueManager& value_manager,
                                        const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> ListValue::Equal(ValueManager& value_manager,
                                       const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> MapValue::Equal(ValueManager& value_manager,
                                      const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> OpaqueValue::Equal(ValueManager& value_manager,
                                         const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> StringValue::Equal(ValueManager& value_manager,
                                         const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> StructValue::Equal(ValueManager& value_manager,
                                         const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> TypeValue::Equal(ValueManager& value_manager,
                                       const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> UnknownValue::Equal(ValueManager& value_manager,
                                          const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::Status ListValue::Get(ValueManager& value_manager, size_t index,
                            Value& result) const {
  return absl::visit(
      [&value_manager, index,
       &result](const auto& alternative) -> absl::Status {
        return alternative.Get(value_manager, index, result);
      },
      variant_);
}

absl::StatusOr<Value> ListValue::Get(ValueManager& value_manager,
                                     size_t index) const {
  Value result;
  CEL_RETURN_IF_ERROR(Get(value_manager, index, result));
  return result;
}

absl::Status ListValue::ForEach(ValueManager& value_manager,
                                ForEachCallback callback) const {
  return absl::visit(
      [&value_manager, callback](const auto& alternative) -> absl::Status {
        return alternative.ForEach(value_manager, callback);
      },
      variant_);
}

absl::Status ListValue::ForEach(ValueManager& value_manager,
                                ForEachWithIndexCallback callback) const {
  return absl::visit(
      [&value_manager, callback](const auto& alternative) -> absl::Status {
        return alternative.ForEach(value_manager, callback);
      },
      variant_);
}

absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> ListValue::NewIterator(
    ValueManager& value_manager) const {
  return absl::visit(
      [&value_manager](const auto& alternative)
          -> absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> {
        return alternative.NewIterator(value_manager);
      },
      variant_);
}

absl::Status ListValue::Equal(ValueManager& value_manager, const Value& other,
                              Value& result) const {
  return absl::visit(
      [&value_manager, &other,
       &result](const auto& alternative) -> absl::Status {
        return alternative.Equal(value_manager, other, result);
      },
      variant_);
}

absl::Status ListValue::Contains(ValueManager& value_manager,
                                 const Value& other, Value& result) const {
  return absl::visit(
      [&value_manager, &other,
       &result](const auto& alternative) -> absl::Status {
        return alternative.Contains(value_manager, other, result);
      },
      variant_);
}

absl::StatusOr<Value> ListValue::Contains(ValueManager& value_manager,
                                          const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Contains(value_manager, other, result));
  return result;
}

absl::Status MapValue::Get(ValueManager& value_manager, const Value& key,
                           Value& result) const {
  return absl::visit(
      [&value_manager, &key, &result](const auto& alternative) -> absl::Status {
        return alternative.Get(value_manager, key, result);
      },
      variant_);
}

absl::StatusOr<Value> MapValue::Get(ValueManager& value_manager,
                                    const Value& key) const {
  Value result;
  CEL_RETURN_IF_ERROR(Get(value_manager, key, result));
  return result;
}

absl::StatusOr<bool> MapValue::Find(ValueManager& value_manager,
                                    const Value& key, Value& result) const {
  return absl::visit(
      [&value_manager, &key,
       &result](const auto& alternative) -> absl::StatusOr<bool> {
        return alternative.Find(value_manager, key, result);
      },
      variant_);
}

absl::StatusOr<std::pair<Value, bool>> MapValue::Find(
    ValueManager& value_manager, const Value& key) const {
  Value result;
  CEL_ASSIGN_OR_RETURN(auto ok, Find(value_manager, key, result));
  return std::pair{std::move(result), ok};
}

absl::Status MapValue::Has(ValueManager& value_manager, const Value& key,
                           Value& result) const {
  return absl::visit(
      [&value_manager, &key, &result](const auto& alternative) -> absl::Status {
        return alternative.Has(value_manager, key, result);
      },
      variant_);
}

absl::StatusOr<Value> MapValue::Has(ValueManager& value_manager,
                                    const Value& key) const {
  Value result;
  CEL_RETURN_IF_ERROR(Has(value_manager, key, result));
  return result;
}

absl::Status MapValue::ListKeys(ValueManager& value_manager,
                                ListValue& result) const {
  return absl::visit(
      [&value_manager, &result](const auto& alternative) -> absl::Status {
        return alternative.ListKeys(value_manager, result);
      },
      variant_);
}

absl::StatusOr<ListValue> MapValue::ListKeys(
    ValueManager& value_manager) const {
  ListValue result;
  CEL_RETURN_IF_ERROR(ListKeys(value_manager, result));
  return result;
}

absl::Status MapValue::ForEach(ValueManager& value_manager,
                               ForEachCallback callback) const {
  return absl::visit(
      [&value_manager, callback](const auto& alternative) -> absl::Status {
        return alternative.ForEach(value_manager, callback);
      },
      variant_);
}

absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> MapValue::NewIterator(
    ValueManager& value_manager) const {
  return absl::visit(
      [&value_manager](const auto& alternative)
          -> absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> {
        return alternative.NewIterator(value_manager);
      },
      variant_);
}

absl::Status MapValue::Equal(ValueManager& value_manager, const Value& other,
                             Value& result) const {
  return absl::visit(
      [&value_manager, &other,
       &result](const auto& alternative) -> absl::Status {
        return alternative.Equal(value_manager, other, result);
      },
      variant_);
}

absl::Status StructValue::GetFieldByName(
    ValueManager& value_manager, absl::string_view name, Value& result,
    ProtoWrapperTypeOptions unboxing_options) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, name, &result,
       unboxing_options](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.GetFieldByName(value_manager, name, result,
                                            unboxing_options);
        }
      },
      variant_);
}

absl::StatusOr<Value> StructValue::GetFieldByName(
    ValueManager& value_manager, absl::string_view name,
    ProtoWrapperTypeOptions unboxing_options) const {
  Value result;
  CEL_RETURN_IF_ERROR(
      GetFieldByName(value_manager, name, result, unboxing_options));
  return result;
}

absl::Status StructValue::GetFieldByNumber(
    ValueManager& value_manager, int64_t number, Value& result,
    ProtoWrapperTypeOptions unboxing_options) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, number, &result,
       unboxing_options](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.GetFieldByNumber(value_manager, number, result,
                                              unboxing_options);
        }
      },
      variant_);
}

absl::StatusOr<Value> StructValue::GetFieldByNumber(
    ValueManager& value_manager, int64_t number,
    ProtoWrapperTypeOptions unboxing_options) const {
  Value result;
  CEL_RETURN_IF_ERROR(
      GetFieldByNumber(value_manager, number, result, unboxing_options));
  return result;
}

absl::Status StructValue::Equal(ValueManager& value_manager, const Value& other,
                                Value& result) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, &other,
       &result](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.Equal(value_manager, other, result);
        }
      },
      variant_);
}

absl::Status StructValue::ForEachField(ValueManager& value_manager,
                                       ForEachFieldCallback callback) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, callback](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.ForEachField(value_manager, callback);
        }
      },
      variant_);
}

absl::StatusOr<int> StructValue::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test, Value& result) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, qualifiers, presence_test,
       &result](const auto& alternative) -> absl::StatusOr<int> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.Qualify(value_manager, qualifiers, presence_test,
                                     result);
        }
      },
      variant_);
}

absl::StatusOr<std::pair<Value, int>> StructValue::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test) const {
  Value result;
  CEL_ASSIGN_OR_RETURN(
      auto count, Qualify(value_manager, qualifiers, presence_test, result));
  return std::pair{std::move(result), count};
}

namespace {

Value NonNullEnumValue(
    absl::Nonnull<const google::protobuf::EnumValueDescriptor*> value) {
  ABSL_DCHECK(value != nullptr);
  return IntValue(value->number());
}

Value NonNullEnumValue(absl::Nonnull<const google::protobuf::EnumDescriptor*> type,
                       int32_t number) {
  ABSL_DCHECK(type != nullptr);
  if (type->is_closed()) {
    if (ABSL_PREDICT_FALSE(type->FindValueByNumber(number) == nullptr)) {
      return ErrorValue(absl::InvalidArgumentError(absl::StrCat(
          "closed enum has no such value: ", type->full_name(), ".", number)));
    }
  }
  return IntValue(number);
}

}  // namespace

Value Value::Enum(absl::Nonnull<const google::protobuf::EnumValueDescriptor*> value) {
  ABSL_DCHECK(value != nullptr);
  if (value->type()->full_name() == "google.protobuf.NullValue") {
    ABSL_DCHECK_EQ(value->number(), 0);
    return NullValue();
  }
  return NonNullEnumValue(value);
}

Value Value::Enum(absl::Nonnull<const google::protobuf::EnumDescriptor*> type,
                  int32_t number) {
  ABSL_DCHECK(type != nullptr);
  if (type->full_name() == "google.protobuf.NullValue") {
    ABSL_DCHECK_EQ(number, 0);
    return NullValue();
  }
  return NonNullEnumValue(type, number);
}

namespace common_internal {

namespace {

void BoolMapFieldKeyAccessor(Allocator<>, Borrower, const google::protobuf::MapKey& key,
                             Value& result) {
  result = BoolValue(key.GetBoolValue());
}

void Int32MapFieldKeyAccessor(Allocator<>, Borrower, const google::protobuf::MapKey& key,
                              Value& result) {
  result = IntValue(key.GetInt32Value());
}

void Int64MapFieldKeyAccessor(Allocator<>, Borrower, const google::protobuf::MapKey& key,
                              Value& result) {
  result = IntValue(key.GetInt64Value());
}

void UInt32MapFieldKeyAccessor(Allocator<>, Borrower, const google::protobuf::MapKey& key,
                               Value& result) {
  result = UintValue(key.GetUInt32Value());
}

void UInt64MapFieldKeyAccessor(Allocator<>, Borrower, const google::protobuf::MapKey& key,
                               Value& result) {
  result = UintValue(key.GetUInt64Value());
}

void StringMapFieldKeyAccessor(Allocator<> allocator, Borrower borrower,
                               const google::protobuf::MapKey& key, Value& result) {
#if CEL_INTERNAL_PROTOBUF_OSS_VERSION_PREREQ(5, 30, 0)
  static_cast<void>(allocator);
  result = StringValue(borrower, key.GetStringValue());
#else
  static_cast<void>(borrower);
  result = StringValue(allocator, key.GetStringValue());
#endif
}

}  // namespace

absl::StatusOr<MapFieldKeyAccessor> MapFieldKeyAccessorFor(
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field) {
  switch (field->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return &BoolMapFieldKeyAccessor;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      return &Int32MapFieldKeyAccessor;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      return &Int64MapFieldKeyAccessor;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      return &UInt32MapFieldKeyAccessor;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return &UInt64MapFieldKeyAccessor;
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
      return &StringMapFieldKeyAccessor;
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("unexpected map key type: ", field->cpp_type_name()));
  }
}

namespace {

void DoubleMapFieldValueAccessor(
    Borrower, const google::protobuf::MapValueConstRef& value,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>, Value& result) {
  ABSL_DCHECK(!field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE);
  result = DoubleValue(value.GetDoubleValue());
}

void FloatMapFieldValueAccessor(
    Borrower, const google::protobuf::MapValueConstRef& value,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>, Value& result) {
  ABSL_DCHECK(!field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_FLOAT);
  result = DoubleValue(value.GetFloatValue());
}

void Int64MapFieldValueAccessor(
    Borrower, const google::protobuf::MapValueConstRef& value,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>, Value& result) {
  ABSL_DCHECK(!field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_INT64);
  result = IntValue(value.GetInt64Value());
}

void UInt64MapFieldValueAccessor(
    Borrower, const google::protobuf::MapValueConstRef& value,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>, Value& result) {
  ABSL_DCHECK(!field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_UINT64);
  result = UintValue(value.GetUInt64Value());
}

void Int32MapFieldValueAccessor(
    Borrower, const google::protobuf::MapValueConstRef& value,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>, Value& result) {
  ABSL_DCHECK(!field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_INT32);
  result = IntValue(value.GetInt32Value());
}

void UInt32MapFieldValueAccessor(
    Borrower, const google::protobuf::MapValueConstRef& value,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>, Value& result) {
  ABSL_DCHECK(!field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_UINT32);
  result = UintValue(value.GetUInt32Value());
}

void BoolMapFieldValueAccessor(
    Borrower, const google::protobuf::MapValueConstRef& value,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>, Value& result) {
  ABSL_DCHECK(!field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_BOOL);
  result = BoolValue(value.GetBoolValue());
}

void StringMapFieldValueAccessor(
    Borrower borrower, const google::protobuf::MapValueConstRef& value,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>, Value& result) {
  ABSL_DCHECK(!field->is_repeated());
  ABSL_DCHECK_EQ(field->type(), google::protobuf::FieldDescriptor::TYPE_STRING);
  result = StringValue(borrower, value.GetStringValue());
}

void MessageMapFieldValueAccessor(
    Borrower borrower, const google::protobuf::MapValueConstRef& value,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory, Value& result) {
  ABSL_DCHECK(!field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE);
  result = Value::Message(Borrowed(borrower, &value.GetMessageValue()),
                          descriptor_pool, message_factory);
}

void BytesMapFieldValueAccessor(
    Borrower borrower, const google::protobuf::MapValueConstRef& value,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>, Value& result) {
  ABSL_DCHECK(!field->is_repeated());
  ABSL_DCHECK_EQ(field->type(), google::protobuf::FieldDescriptor::TYPE_BYTES);
  result = BytesValue(borrower, value.GetStringValue());
}

void EnumMapFieldValueAccessor(
    Borrower, const google::protobuf::MapValueConstRef& value,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>, Value& result) {
  ABSL_DCHECK(!field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_ENUM);
  result = NonNullEnumValue(field->enum_type(), value.GetEnumValue());
}

void NullMapFieldValueAccessor(
    Borrower, const google::protobuf::MapValueConstRef&,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>, Value& result) {
  ABSL_DCHECK(!field->is_repeated());
  ABSL_DCHECK(field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_ENUM &&
              field->enum_type()->full_name() == "google.protobuf.NullValue");
  result = NullValue();
}

}  // namespace

absl::StatusOr<MapFieldValueAccessor> MapFieldValueAccessorFor(
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field) {
  switch (field->type()) {
    case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
      return &DoubleMapFieldValueAccessor;
    case google::protobuf::FieldDescriptor::TYPE_FLOAT:
      return &FloatMapFieldValueAccessor;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_INT64:
      return &Int64MapFieldValueAccessor;
    case google::protobuf::FieldDescriptor::TYPE_FIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_UINT64:
      return &UInt64MapFieldValueAccessor;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_INT32:
      return &Int32MapFieldValueAccessor;
    case google::protobuf::FieldDescriptor::TYPE_BOOL:
      return &BoolMapFieldValueAccessor;
    case google::protobuf::FieldDescriptor::TYPE_STRING:
      return &StringMapFieldValueAccessor;
    case google::protobuf::FieldDescriptor::TYPE_GROUP:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
      return &MessageMapFieldValueAccessor;
    case google::protobuf::FieldDescriptor::TYPE_BYTES:
      return &BytesMapFieldValueAccessor;
    case google::protobuf::FieldDescriptor::TYPE_FIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_UINT32:
      return &UInt32MapFieldValueAccessor;
    case google::protobuf::FieldDescriptor::TYPE_ENUM:
      if (field->enum_type()->full_name() == "google.protobuf.NullValue") {
        return &NullMapFieldValueAccessor;
      }
      return &EnumMapFieldValueAccessor;
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("unexpected protocol buffer message field type: ",
                       field->type_name()));
  }
}

namespace {

void DoubleRepeatedFieldAccessor(
    Allocator<>, Borrowed<const google::protobuf::Message> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::Reflection*> reflection, int index,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>, Value& result) {
  ABSL_DCHECK_EQ(reflection, message->GetReflection());
  ABSL_DCHECK_EQ(field->containing_type(), message->GetDescriptor());
  ABSL_DCHECK(field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE);
  ABSL_DCHECK_GE(index, 0);
  ABSL_DCHECK_LT(index, reflection->FieldSize(*message, field));
  result = DoubleValue(reflection->GetRepeatedDouble(*message, field, index));
}

void FloatRepeatedFieldAccessor(
    Allocator<>, Borrowed<const google::protobuf::Message> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::Reflection*> reflection, int index,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>, Value& result) {
  ABSL_DCHECK_EQ(reflection, message->GetReflection());
  ABSL_DCHECK_EQ(field->containing_type(), message->GetDescriptor());
  ABSL_DCHECK(field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_FLOAT);
  ABSL_DCHECK_GE(index, 0);
  ABSL_DCHECK_LT(index, reflection->FieldSize(*message, field));
  result = DoubleValue(reflection->GetRepeatedFloat(*message, field, index));
}

void Int64RepeatedFieldAccessor(
    Allocator<>, Borrowed<const google::protobuf::Message> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::Reflection*> reflection, int index,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>, Value& result) {
  ABSL_DCHECK_EQ(reflection, message->GetReflection());
  ABSL_DCHECK_EQ(field->containing_type(), message->GetDescriptor());
  ABSL_DCHECK(field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_INT64);
  ABSL_DCHECK_GE(index, 0);
  ABSL_DCHECK_LT(index, reflection->FieldSize(*message, field));
  result = IntValue(reflection->GetRepeatedInt64(*message, field, index));
}

void UInt64RepeatedFieldAccessor(
    Allocator<>, Borrowed<const google::protobuf::Message> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::Reflection*> reflection, int index,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>, Value& result) {
  ABSL_DCHECK_EQ(reflection, message->GetReflection());
  ABSL_DCHECK_EQ(field->containing_type(), message->GetDescriptor());
  ABSL_DCHECK(field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_UINT64);
  ABSL_DCHECK_GE(index, 0);
  ABSL_DCHECK_LT(index, reflection->FieldSize(*message, field));
  result = UintValue(reflection->GetRepeatedUInt64(*message, field, index));
}

void Int32RepeatedFieldAccessor(
    Allocator<>, Borrowed<const google::protobuf::Message> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::Reflection*> reflection, int index,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>, Value& result) {
  ABSL_DCHECK_EQ(reflection, message->GetReflection());
  ABSL_DCHECK_EQ(field->containing_type(), message->GetDescriptor());
  ABSL_DCHECK(field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_INT32);
  ABSL_DCHECK_GE(index, 0);
  ABSL_DCHECK_LT(index, reflection->FieldSize(*message, field));
  result = IntValue(reflection->GetRepeatedInt32(*message, field, index));
}

void UInt32RepeatedFieldAccessor(
    Allocator<>, Borrowed<const google::protobuf::Message> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::Reflection*> reflection, int index,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>, Value& result) {
  ABSL_DCHECK_EQ(reflection, message->GetReflection());
  ABSL_DCHECK_EQ(field->containing_type(), message->GetDescriptor());
  ABSL_DCHECK(field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_UINT32);
  ABSL_DCHECK_GE(index, 0);
  ABSL_DCHECK_LT(index, reflection->FieldSize(*message, field));
  result = UintValue(reflection->GetRepeatedUInt32(*message, field, index));
}

void BoolRepeatedFieldAccessor(
    Allocator<>, Borrowed<const google::protobuf::Message> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::Reflection*> reflection, int index,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>, Value& result) {
  ABSL_DCHECK_EQ(reflection, message->GetReflection());
  ABSL_DCHECK_EQ(field->containing_type(), message->GetDescriptor());
  ABSL_DCHECK(field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_BOOL);
  ABSL_DCHECK_GE(index, 0);
  ABSL_DCHECK_LT(index, reflection->FieldSize(*message, field));
  result = BoolValue(reflection->GetRepeatedBool(*message, field, index));
}

void StringRepeatedFieldAccessor(
    Allocator<> allocator, Borrowed<const google::protobuf::Message> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::Reflection*> reflection, int index,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>, Value& result) {
  ABSL_DCHECK_EQ(reflection, message->GetReflection());
  ABSL_DCHECK_EQ(field->containing_type(), message->GetDescriptor());
  ABSL_DCHECK(field->is_repeated());
  ABSL_DCHECK_EQ(field->type(), google::protobuf::FieldDescriptor::TYPE_STRING);
  ABSL_DCHECK_GE(index, 0);
  ABSL_DCHECK_LT(index, reflection->FieldSize(*message, field));
  std::string scratch;
  absl::visit(
      absl::Overload(
          [&](absl::string_view string) {
            if (string.data() == scratch.data() &&
                string.size() == scratch.size()) {
              result = StringValue(allocator, std::move(scratch));
            } else {
              result = StringValue(Borrower(message), string);
            }
          },
          [&](absl::Cord&& cord) { result = StringValue(std::move(cord)); }),
      well_known_types::AsVariant(well_known_types::GetRepeatedStringField(
          *message, field, index, scratch)));
}

void MessageRepeatedFieldAccessor(
    Allocator<> allocator, Borrowed<const google::protobuf::Message> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::Reflection*> reflection, int index,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory, Value& result) {
  ABSL_DCHECK_EQ(reflection, message->GetReflection());
  ABSL_DCHECK_EQ(field->containing_type(), message->GetDescriptor());
  ABSL_DCHECK(field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE);
  ABSL_DCHECK_GE(index, 0);
  ABSL_DCHECK_LT(index, reflection->FieldSize(*message, field));
  result = Value::Message(Borrowed(message, &reflection->GetRepeatedMessage(
                                                *message, field, index)),
                          descriptor_pool, message_factory);
}

void BytesRepeatedFieldAccessor(
    Allocator<> allocator, Borrowed<const google::protobuf::Message> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::Reflection*> reflection, int index,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>, Value& result) {
  ABSL_DCHECK_EQ(reflection, message->GetReflection());
  ABSL_DCHECK_EQ(field->containing_type(), message->GetDescriptor());
  ABSL_DCHECK(field->is_repeated());
  ABSL_DCHECK_EQ(field->type(), google::protobuf::FieldDescriptor::TYPE_BYTES);
  ABSL_DCHECK_GE(index, 0);
  ABSL_DCHECK_LT(index, reflection->FieldSize(*message, field));
  std::string scratch;
  absl::visit(
      absl::Overload(
          [&](absl::string_view string) {
            if (string.data() == scratch.data() &&
                string.size() == scratch.size()) {
              result = BytesValue(allocator, std::move(scratch));
            } else {
              result = BytesValue(Borrower(message), string);
            }
          },
          [&](absl::Cord&& cord) { result = BytesValue(std::move(cord)); }),
      well_known_types::AsVariant(well_known_types::GetRepeatedBytesField(
          *message, field, index, scratch)));
}

void EnumRepeatedFieldAccessor(
    Allocator<>, Borrowed<const google::protobuf::Message> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::Reflection*> reflection, int index,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>, Value& result) {
  ABSL_DCHECK_EQ(reflection, message->GetReflection());
  ABSL_DCHECK_EQ(field->containing_type(), message->GetDescriptor());
  ABSL_DCHECK(field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_ENUM);
  ABSL_DCHECK_GE(index, 0);
  ABSL_DCHECK_LT(index, reflection->FieldSize(*message, field));
  result = NonNullEnumValue(
      field->enum_type(),
      reflection->GetRepeatedEnumValue(*message, field, index));
}

void NullRepeatedFieldAccessor(
    Allocator<>, Borrowed<const google::protobuf::Message> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::Reflection*> reflection, int index,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>, Value& result) {
  ABSL_DCHECK_EQ(reflection, message->GetReflection());
  ABSL_DCHECK_EQ(field->containing_type(), message->GetDescriptor());
  ABSL_DCHECK(field->is_repeated());
  ABSL_DCHECK(field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_ENUM &&
              field->enum_type()->full_name() == "google.protobuf.NullValue");
  ABSL_DCHECK_GE(index, 0);
  ABSL_DCHECK_LT(index, reflection->FieldSize(*message, field));
  result = NullValue();
}

}  // namespace

absl::StatusOr<RepeatedFieldAccessor> RepeatedFieldAccessorFor(
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field) {
  switch (field->type()) {
    case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
      return &DoubleRepeatedFieldAccessor;
    case google::protobuf::FieldDescriptor::TYPE_FLOAT:
      return &FloatRepeatedFieldAccessor;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_INT64:
      return &Int64RepeatedFieldAccessor;
    case google::protobuf::FieldDescriptor::TYPE_FIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_UINT64:
      return &UInt64RepeatedFieldAccessor;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_INT32:
      return &Int32RepeatedFieldAccessor;
    case google::protobuf::FieldDescriptor::TYPE_BOOL:
      return &BoolRepeatedFieldAccessor;
    case google::protobuf::FieldDescriptor::TYPE_STRING:
      return &StringRepeatedFieldAccessor;
    case google::protobuf::FieldDescriptor::TYPE_GROUP:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
      return &MessageRepeatedFieldAccessor;
    case google::protobuf::FieldDescriptor::TYPE_BYTES:
      return &BytesRepeatedFieldAccessor;
    case google::protobuf::FieldDescriptor::TYPE_FIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_UINT32:
      return &UInt32RepeatedFieldAccessor;
    case google::protobuf::FieldDescriptor::TYPE_ENUM:
      if (field->enum_type()->full_name() == "google.protobuf.NullValue") {
        return &NullRepeatedFieldAccessor;
      }
      return &EnumRepeatedFieldAccessor;
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("unexpected protocol buffer message field type: ",
                       field->type_name()));
  }
}

}  // namespace common_internal

namespace {

// WellKnownTypesValueVisitor is the base visitor for `well_known_types::Value`
// which handles the primitive values which require no special handling based on
// allocators.
struct WellKnownTypesValueVisitor {
  Value operator()(std::nullptr_t) const { return NullValue(); }

  Value operator()(bool value) const { return BoolValue(value); }

  Value operator()(int32_t value) const { return IntValue(value); }

  Value operator()(int64_t value) const { return IntValue(value); }

  Value operator()(uint32_t value) const { return UintValue(value); }

  Value operator()(uint64_t value) const { return UintValue(value); }

  Value operator()(float value) const { return DoubleValue(value); }

  Value operator()(double value) const { return DoubleValue(value); }

  Value operator()(absl::Duration value) const { return DurationValue(value); }

  Value operator()(absl::Time value) const { return TimestampValue(value); }
};

struct OwningWellKnownTypesValueVisitor : public WellKnownTypesValueVisitor {
  absl::Nullable<google::protobuf::Arena*> arena;
  absl::Nonnull<std::string*> scratch;

  using WellKnownTypesValueVisitor::operator();

  Value operator()(well_known_types::BytesValue&& value) const {
    return absl::visit(absl::Overload(
                           [&](absl::string_view string) -> BytesValue {
                             if (string.empty()) {
                               return BytesValue();
                             }
                             if (scratch->data() == string.data() &&
                                 scratch->size() == string.size()) {
                               return BytesValue(Allocator(arena),
                                                 std::move(*scratch));
                             }
                             return BytesValue(Allocator(arena), string);
                           },
                           [&](absl::Cord&& cord) -> BytesValue {
                             if (cord.empty()) {
                               return BytesValue();
                             }
                             return BytesValue(Allocator(arena), cord);
                           }),
                       well_known_types::AsVariant(std::move(value)));
  }

  Value operator()(well_known_types::StringValue&& value) const {
    return absl::visit(absl::Overload(
                           [&](absl::string_view string) -> StringValue {
                             if (string.empty()) {
                               return StringValue();
                             }
                             if (scratch->data() == string.data() &&
                                 scratch->size() == string.size()) {
                               return StringValue(Allocator(arena),
                                                  std::move(*scratch));
                             }
                             return StringValue(Allocator(arena), string);
                           },
                           [&](absl::Cord&& cord) -> StringValue {
                             if (cord.empty()) {
                               return StringValue();
                             }
                             return StringValue(Allocator(arena), cord);
                           }),
                       well_known_types::AsVariant(std::move(value)));
  }

  Value operator()(well_known_types::ListValue&& value) const {
    return absl::visit(
        absl::Overload(
            [&](well_known_types::ListValueConstRef value) -> ListValue {
              auto cloned = WrapShared(value.get().New(arena), arena);
              cloned->CopyFrom(value.get());
              return ParsedJsonListValue(std::move(cloned));
            },
            [&](well_known_types::ListValuePtr value) -> ListValue {
              if (value.arena() != arena) {
                auto cloned = WrapShared(value->New(arena), arena);
                cloned->CopyFrom(*value);
                return ParsedJsonListValue(std::move(cloned));
              }
              return ParsedJsonListValue(Owned(std::move(value)));
            }),
        well_known_types::AsVariant(std::move(value)));
  }

  Value operator()(well_known_types::Struct&& value) const {
    return absl::visit(
        absl::Overload(
            [&](well_known_types::StructConstRef value) -> MapValue {
              auto cloned = WrapShared(value.get().New(arena), arena);
              cloned->CopyFrom(value.get());
              return ParsedJsonMapValue(std::move(cloned));
            },
            [&](well_known_types::StructPtr value) -> MapValue {
              if (value.arena() != arena) {
                auto cloned = WrapShared(value->New(arena), arena);
                cloned->CopyFrom(*value);
                return ParsedJsonMapValue(std::move(cloned));
              }
              return ParsedJsonMapValue(Owned(std::move(value)));
            }),
        well_known_types::AsVariant(std::move(value)));
  }

  Value operator()(Unique<google::protobuf::Message> value) const {
    if (value.arena() != arena) {
      auto cloned = WrapShared(value->New(arena), arena);
      cloned->CopyFrom(*value);
      return ParsedMessageValue(std::move(cloned));
    }
    return ParsedMessageValue(Owned(std::move(value)));
  }
};

struct BorrowingWellKnownTypesValueVisitor : public WellKnownTypesValueVisitor {
  Borrower borrower;
  absl::Nonnull<std::string*> scratch;

  using WellKnownTypesValueVisitor::operator();

  Value operator()(well_known_types::BytesValue&& value) const {
    return absl::visit(absl::Overload(
                           [&](absl::string_view string) -> BytesValue {
                             if (string.data() == scratch->data() &&
                                 string.size() == scratch->size()) {
                               return BytesValue(borrower.arena(),
                                                 std::move(*scratch));
                             } else {
                               return BytesValue(borrower, string);
                             }
                           },
                           [&](absl::Cord&& cord) -> BytesValue {
                             return BytesValue(std::move(cord));
                           }),
                       well_known_types::AsVariant(std::move(value)));
  }

  Value operator()(well_known_types::StringValue&& value) const {
    return absl::visit(absl::Overload(
                           [&](absl::string_view string) -> StringValue {
                             if (string.data() == scratch->data() &&
                                 string.size() == scratch->size()) {
                               return StringValue(borrower.arena(),
                                                  std::move(*scratch));
                             } else {
                               return StringValue(borrower, string);
                             }
                           },
                           [&](absl::Cord&& cord) -> StringValue {
                             return StringValue(std::move(cord));
                           }),
                       well_known_types::AsVariant(std::move(value)));
  }

  Value operator()(well_known_types::ListValue&& value) const {
    return absl::visit(
        absl::Overload(
            [&](well_known_types::ListValueConstRef value)
                -> ParsedJsonListValue {
              return ParsedJsonListValue(Owned(Owner(borrower), &value.get()));
            },
            [&](well_known_types::ListValuePtr value) -> ParsedJsonListValue {
              return ParsedJsonListValue(Owned(std::move(value)));
            }),
        well_known_types::AsVariant(std::move(value)));
  }

  Value operator()(well_known_types::Struct&& value) const {
    return absl::visit(
        absl::Overload(
            [&](well_known_types::StructConstRef value) -> ParsedJsonMapValue {
              return ParsedJsonMapValue(Owned(Owner(borrower), &value.get()));
            },
            [&](well_known_types::StructPtr value) -> ParsedJsonMapValue {
              return ParsedJsonMapValue(Owned(std::move(value)));
            }),
        well_known_types::AsVariant(std::move(value)));
  }

  Value operator()(Unique<google::protobuf::Message>&& value) const {
    return ParsedMessageValue(Owned(std::move(value)));
  }
};

}  // namespace

Value Value::Message(
    Allocator<> allocator, const google::protobuf::Message& message,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  std::string scratch;
  auto status_or_adapted = well_known_types::AdaptFromMessage(
      allocator.arena(), message, descriptor_pool, message_factory, scratch);
  if (ABSL_PREDICT_FALSE(!status_or_adapted.ok())) {
    return ErrorValue(std::move(status_or_adapted).status());
  }
  return absl::visit(absl::Overload(
                         OwningWellKnownTypesValueVisitor{
                             .arena = allocator.arena(), .scratch = &scratch},
                         [&](absl::monostate) -> Value {
                           auto cloned = WrapShared(
                               message.New(allocator.arena()), allocator);
                           cloned->CopyFrom(message);
                           return ParsedMessageValue(std::move(cloned));
                         }),
                     std::move(status_or_adapted).value());
}

Value Value::Message(
    Allocator<> allocator, google::protobuf::Message&& message,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  std::string scratch;
  auto status_or_adapted = well_known_types::AdaptFromMessage(
      allocator.arena(), message, descriptor_pool, message_factory, scratch);
  if (ABSL_PREDICT_FALSE(!status_or_adapted.ok())) {
    return ErrorValue(std::move(status_or_adapted).status());
  }
  return absl::visit(
      absl::Overload(
          OwningWellKnownTypesValueVisitor{.arena = allocator.arena(),
                                           .scratch = &scratch},
          [&](absl::monostate) -> Value {
            auto cloned = WrapShared(message.New(allocator.arena()), allocator);
            cloned->GetReflection()->Swap(cel::to_address(cloned), &message);
            return ParsedMessageValue(std::move(cloned));
          }),
      std::move(status_or_adapted).value());
}

Value Value::Message(
    Borrowed<const google::protobuf::Message> message,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  std::string scratch;
  auto status_or_adapted = well_known_types::AdaptFromMessage(
      message.arena(), *message, descriptor_pool, message_factory, scratch);
  if (ABSL_PREDICT_FALSE(!status_or_adapted.ok())) {
    return ErrorValue(std::move(status_or_adapted).status());
  }
  return absl::visit(
      absl::Overload(BorrowingWellKnownTypesValueVisitor{.borrower = message,
                                                         .scratch = &scratch},
                     [&](absl::monostate) -> Value {
                       return ParsedMessageValue(Owned(message));
                     }),
      std::move(status_or_adapted).value());
}

Value Value::Field(Borrowed<const google::protobuf::Message> message,
                   absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
                   ProtoWrapperTypeOptions wrapper_type_options) {
  const auto* descriptor = message->GetDescriptor();
  const auto* reflection = message->GetReflection();
  return Field(std::move(message), field, descriptor->file()->pool(),
               reflection->GetMessageFactory(), wrapper_type_options);
}

namespace {

bool IsWellKnownMessageWrapperType(
    absl::Nonnull<const google::protobuf::Descriptor*> descriptor) {
  switch (descriptor->well_known_type()) {
    case google::protobuf::Descriptor::WELLKNOWNTYPE_BOOLVALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_INT32VALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_INT64VALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT32VALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT64VALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_FLOATVALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_DOUBLEVALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_BYTESVALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_STRINGVALUE:
      return true;
    default:
      return false;
  }
}

}  // namespace

Value Value::Field(Borrowed<const google::protobuf::Message> message,
                   absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
                   absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
                   absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
                   ProtoWrapperTypeOptions wrapper_type_options) {
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK_EQ(message->GetDescriptor(), field->containing_type());
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(!IsWellKnownMessageType(message->GetDescriptor()));
  const auto* reflection = message->GetReflection();
  if (field->is_map()) {
    if (reflection->FieldSize(*message, field) == 0) {
      return MapValue();
    }
    return ParsedMapFieldValue(Owned(message), field);
  }
  if (field->is_repeated()) {
    if (reflection->FieldSize(*message, field) == 0) {
      return ListValue();
    }
    return ParsedRepeatedFieldValue(Owned(message), field);
  }
  switch (field->type()) {
    case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
      return DoubleValue(reflection->GetDouble(*message, field));
    case google::protobuf::FieldDescriptor::TYPE_FLOAT:
      return DoubleValue(reflection->GetFloat(*message, field));
    case google::protobuf::FieldDescriptor::TYPE_INT64:
      return IntValue(reflection->GetInt64(*message, field));
    case google::protobuf::FieldDescriptor::TYPE_UINT64:
      return UintValue(reflection->GetUInt64(*message, field));
    case google::protobuf::FieldDescriptor::TYPE_INT32:
      return IntValue(reflection->GetInt32(*message, field));
    case google::protobuf::FieldDescriptor::TYPE_FIXED64:
      return UintValue(reflection->GetUInt64(*message, field));
    case google::protobuf::FieldDescriptor::TYPE_FIXED32:
      return UintValue(reflection->GetUInt32(*message, field));
    case google::protobuf::FieldDescriptor::TYPE_BOOL:
      return BoolValue(reflection->GetBool(*message, field));
    case google::protobuf::FieldDescriptor::TYPE_STRING: {
      std::string scratch;
      return absl::visit(
          absl::Overload(
              [&](absl::string_view string) -> StringValue {
                if (string.data() == scratch.data() &&
                    string.size() == scratch.size()) {
                  return StringValue(message.arena(), std::move(scratch));
                } else {
                  return StringValue(message, string);
                }
              },
              [&](absl::Cord&& cord) -> StringValue {
                return StringValue(std::move(cord));
              }),
          well_known_types::AsVariant(
              well_known_types::GetStringField(*message, field, scratch)));
    }
    case google::protobuf::FieldDescriptor::TYPE_GROUP:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
      if (wrapper_type_options == ProtoWrapperTypeOptions::kUnsetNull &&
          IsWellKnownMessageWrapperType(field->message_type()) &&
          !reflection->HasField(*message, field)) {
        return NullValue();
      }
      return Message(
          Borrowed(message, &reflection->GetMessage(*message, field)),
          descriptor_pool, message_factory);
    case google::protobuf::FieldDescriptor::TYPE_BYTES: {
      std::string scratch;
      return absl::visit(
          absl::Overload(
              [&](absl::string_view string) -> BytesValue {
                if (string.data() == scratch.data() &&
                    string.size() == scratch.size()) {
                  return BytesValue(message.arena(), std::move(scratch));
                } else {
                  return BytesValue(message, string);
                }
              },
              [&](absl::Cord&& cord) -> BytesValue {
                return BytesValue(std::move(cord));
              }),
          well_known_types::AsVariant(
              well_known_types::GetBytesField(*message, field, scratch)));
    }
    case google::protobuf::FieldDescriptor::TYPE_UINT32:
      return UintValue(reflection->GetUInt32(*message, field));
    case google::protobuf::FieldDescriptor::TYPE_ENUM:
      return Value::Enum(field->enum_type(),
                         reflection->GetEnumValue(*message, field));
    case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
      return IntValue(reflection->GetInt32(*message, field));
    case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
      return IntValue(reflection->GetInt64(*message, field));
    case google::protobuf::FieldDescriptor::TYPE_SINT32:
      return IntValue(reflection->GetInt32(*message, field));
    case google::protobuf::FieldDescriptor::TYPE_SINT64:
      return IntValue(reflection->GetInt64(*message, field));
    default:
      return ErrorValue(absl::InvalidArgumentError(
          absl::StrCat("unexpected protocol buffer message field type: ",
                       field->type_name())));
  }
}

Value Value::RepeatedField(Borrowed<const google::protobuf::Message> message,
                           absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
                           int index) {
  return RepeatedField(message, field, index,
                       message->GetDescriptor()->file()->pool(),
                       message->GetReflection()->GetMessageFactory());
}

Value Value::RepeatedField(
    Borrowed<const google::protobuf::Message> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory) {
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK_EQ(field->containing_type(), message->GetDescriptor());
  ABSL_DCHECK(!field->is_map() && field->is_repeated());
  ABSL_DCHECK_GE(index, 0);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  const auto* reflection = message->GetReflection();
  const int size = reflection->FieldSize(*message, field);
  if (ABSL_PREDICT_FALSE(index < 0 || index >= size)) {
    return ErrorValue(absl::InvalidArgumentError(
        absl::StrCat("index out of bounds: ", index)));
  }
  switch (field->type()) {
    case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
      return DoubleValue(reflection->GetRepeatedDouble(*message, field, index));
    case google::protobuf::FieldDescriptor::TYPE_FLOAT:
      return DoubleValue(reflection->GetRepeatedFloat(*message, field, index));
    case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_INT64:
      return IntValue(reflection->GetRepeatedInt64(*message, field, index));
    case google::protobuf::FieldDescriptor::TYPE_FIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_UINT64:
      return UintValue(reflection->GetRepeatedUInt64(*message, field, index));
    case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_INT32:
      return IntValue(reflection->GetRepeatedInt32(*message, field, index));
    case google::protobuf::FieldDescriptor::TYPE_BOOL:
      return BoolValue(reflection->GetRepeatedBool(*message, field, index));
    case google::protobuf::FieldDescriptor::TYPE_STRING: {
      std::string scratch;
      return absl::visit(
          absl::Overload(
              [&](absl::string_view string) -> StringValue {
                if (string.data() == scratch.data() &&
                    string.size() == scratch.size()) {
                  return StringValue(message.arena(), std::move(scratch));
                } else {
                  return StringValue(message, string);
                }
              },
              [&](absl::Cord&& cord) -> StringValue {
                return StringValue(std::move(cord));
              }),
          well_known_types::AsVariant(well_known_types::GetRepeatedStringField(
              reflection, *message, field, index, scratch)));
    }
    case google::protobuf::FieldDescriptor::TYPE_GROUP:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
      return Message(Borrowed(message, &reflection->GetRepeatedMessage(
                                           *message, field, index)),
                     descriptor_pool, message_factory);
    case google::protobuf::FieldDescriptor::TYPE_BYTES: {
      std::string scratch;
      return absl::visit(
          absl::Overload(
              [&](absl::string_view string) -> BytesValue {
                if (string.data() == scratch.data() &&
                    string.size() == scratch.size()) {
                  return BytesValue(message.arena(), std::move(scratch));
                } else {
                  return BytesValue(message, string);
                }
              },
              [&](absl::Cord&& cord) -> BytesValue {
                return BytesValue(std::move(cord));
              }),
          well_known_types::AsVariant(well_known_types::GetRepeatedBytesField(
              reflection, *message, field, index, scratch)));
    }
    case google::protobuf::FieldDescriptor::TYPE_FIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_UINT32:
      return UintValue(reflection->GetRepeatedUInt32(*message, field, index));
    case google::protobuf::FieldDescriptor::TYPE_ENUM:
      return Enum(field->enum_type(),
                  reflection->GetRepeatedEnumValue(*message, field, index));
    default:
      return ErrorValue(absl::InvalidArgumentError(
          absl::StrCat("unexpected message field type: ", field->type_name())));
  }
}

StringValue Value::MapFieldKeyString(Borrowed<const google::protobuf::Message> message,
                                     const google::protobuf::MapKey& key) {
  ABSL_DCHECK(message);
  ABSL_DCHECK_EQ(key.type(), google::protobuf::FieldDescriptor::CPPTYPE_STRING);
#if CEL_INTERNAL_PROTOBUF_OSS_VERSION_PREREQ(5, 30, 0)
  return StringValue(message, key.GetStringValue());
#else
  return StringValue(Allocator<>{message.arena()}, key.GetStringValue());
#endif
}

Value Value::MapFieldValue(Borrowed<const google::protobuf::Message> message,
                           absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
                           const google::protobuf::MapValueConstRef& value) {
  return MapFieldValue(message, field, value,
                       message->GetDescriptor()->file()->pool(),
                       message->GetReflection()->GetMessageFactory());
}

Value Value::MapFieldValue(
    Borrowed<const google::protobuf::Message> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    const google::protobuf::MapValueConstRef& value,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory) {
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK_EQ(field->containing_type()->containing_type(),
                 message->GetDescriptor());
  ABSL_DCHECK(!field->is_map() && !field->is_repeated());
  ABSL_DCHECK_EQ(value.type(), field->cpp_type());
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  switch (field->type()) {
    case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
      return DoubleValue(value.GetDoubleValue());
    case google::protobuf::FieldDescriptor::TYPE_FLOAT:
      return DoubleValue(value.GetFloatValue());
    case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_INT64:
      return IntValue(value.GetInt64Value());
    case google::protobuf::FieldDescriptor::TYPE_FIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_UINT64:
      return UintValue(value.GetUInt64Value());
    case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_INT32:
      return IntValue(value.GetInt32Value());
    case google::protobuf::FieldDescriptor::TYPE_BOOL:
      return BoolValue(value.GetBoolValue());
    case google::protobuf::FieldDescriptor::TYPE_STRING:
      return StringValue(message, value.GetStringValue());
    case google::protobuf::FieldDescriptor::TYPE_GROUP:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
      return Message(Borrowed<const google::protobuf::Message>(Borrower(message),
                                                     &value.GetMessageValue()),
                     descriptor_pool, message_factory);
    case google::protobuf::FieldDescriptor::TYPE_BYTES:
      return BytesValue(message, value.GetStringValue());
    case google::protobuf::FieldDescriptor::TYPE_FIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_UINT32:
      return UintValue(value.GetUInt32Value());
    case google::protobuf::FieldDescriptor::TYPE_ENUM:
      return Enum(field->enum_type(), value.GetEnumValue());
    default:
      return ErrorValue(absl::InvalidArgumentError(
          absl::StrCat("unexpected message field type: ", field->type_name())));
  }
}

absl::optional<BoolValue> Value::AsBool() const {
  if (const auto* alternative = absl::get_if<BoolValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

optional_ref<const BytesValue> Value::AsBytes() const& {
  if (const auto* alternative = absl::get_if<BytesValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<BytesValue> Value::AsBytes() && {
  if (auto* alternative = absl::get_if<BytesValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

absl::optional<DoubleValue> Value::AsDouble() const {
  if (const auto* alternative = absl::get_if<DoubleValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<DurationValue> Value::AsDuration() const {
  if (const auto* alternative = absl::get_if<DurationValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

optional_ref<const ErrorValue> Value::AsError() const& {
  if (const auto* alternative = absl::get_if<ErrorValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ErrorValue> Value::AsError() && {
  if (auto* alternative = absl::get_if<ErrorValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

absl::optional<IntValue> Value::AsInt() const {
  if (const auto* alternative = absl::get_if<IntValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ListValue> Value::AsList() const& {
  if (const auto* alternative =
          absl::get_if<common_internal::LegacyListValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedListValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative =
          absl::get_if<ParsedRepeatedFieldValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedJsonListValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ListValue> Value::AsList() && {
  if (auto* alternative =
          absl::get_if<common_internal::LegacyListValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedListValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedRepeatedFieldValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedJsonListValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

absl::optional<MapValue> Value::AsMap() const& {
  if (const auto* alternative =
          absl::get_if<common_internal::LegacyMapValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedMapValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedMapFieldValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedJsonMapValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<MapValue> Value::AsMap() && {
  if (auto* alternative =
          absl::get_if<common_internal::LegacyMapValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedMapValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedMapFieldValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedJsonMapValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

absl::optional<MessageValue> Value::AsMessage() const& {
  if (const auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<MessageValue> Value::AsMessage() && {
  if (auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

absl::optional<NullValue> Value::AsNull() const {
  if (const auto* alternative = absl::get_if<NullValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

optional_ref<const OpaqueValue> Value::AsOpaque() const& {
  if (const auto* alternative = absl::get_if<OpaqueValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<OpaqueValue> Value::AsOpaque() && {
  if (auto* alternative = absl::get_if<OpaqueValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const OptionalValue> Value::AsOptional() const& {
  if (const auto* alternative = absl::get_if<OpaqueValue>(&variant_);
      alternative != nullptr && alternative->IsOptional()) {
    return static_cast<const OptionalValue&>(*alternative);
  }
  return absl::nullopt;
}

absl::optional<OptionalValue> Value::AsOptional() && {
  if (auto* alternative = absl::get_if<OpaqueValue>(&variant_);
      alternative != nullptr && alternative->IsOptional()) {
    return static_cast<OptionalValue&&>(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const ParsedJsonListValue> Value::AsParsedJsonList() const& {
  if (const auto* alternative = absl::get_if<ParsedJsonListValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedJsonListValue> Value::AsParsedJsonList() && {
  if (auto* alternative = absl::get_if<ParsedJsonListValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const ParsedJsonMapValue> Value::AsParsedJsonMap() const& {
  if (const auto* alternative = absl::get_if<ParsedJsonMapValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedJsonMapValue> Value::AsParsedJsonMap() && {
  if (auto* alternative = absl::get_if<ParsedJsonMapValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const ParsedListValue> Value::AsParsedList() const& {
  if (const auto* alternative = absl::get_if<ParsedListValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedListValue> Value::AsParsedList() && {
  if (auto* alternative = absl::get_if<ParsedListValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const ParsedMapValue> Value::AsParsedMap() const& {
  if (const auto* alternative = absl::get_if<ParsedMapValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedMapValue> Value::AsParsedMap() && {
  if (auto* alternative = absl::get_if<ParsedMapValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const ParsedMapFieldValue> Value::AsParsedMapField() const& {
  if (const auto* alternative = absl::get_if<ParsedMapFieldValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedMapFieldValue> Value::AsParsedMapField() && {
  if (auto* alternative = absl::get_if<ParsedMapFieldValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const ParsedMessageValue> Value::AsParsedMessage() const& {
  if (const auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedMessageValue> Value::AsParsedMessage() && {
  if (auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const ParsedRepeatedFieldValue> Value::AsParsedRepeatedField()
    const& {
  if (const auto* alternative =
          absl::get_if<ParsedRepeatedFieldValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedRepeatedFieldValue> Value::AsParsedRepeatedField() && {
  if (auto* alternative = absl::get_if<ParsedRepeatedFieldValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const ParsedStructValue> Value::AsParsedStruct() const& {
  if (const auto* alternative = absl::get_if<ParsedStructValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedStructValue> Value::AsParsedStruct() && {
  if (auto* alternative = absl::get_if<ParsedStructValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const StringValue> Value::AsString() const& {
  if (const auto* alternative = absl::get_if<StringValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<StringValue> Value::AsString() && {
  if (auto* alternative = absl::get_if<StringValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

absl::optional<StructValue> Value::AsStruct() const& {
  if (const auto* alternative =
          absl::get_if<common_internal::LegacyStructValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedStructValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<StructValue> Value::AsStruct() && {
  if (auto* alternative =
          absl::get_if<common_internal::LegacyStructValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedStructValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

absl::optional<TimestampValue> Value::AsTimestamp() const {
  if (const auto* alternative = absl::get_if<TimestampValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

optional_ref<const TypeValue> Value::AsType() const& {
  if (const auto* alternative = absl::get_if<TypeValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<TypeValue> Value::AsType() && {
  if (auto* alternative = absl::get_if<TypeValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

absl::optional<UintValue> Value::AsUint() const {
  if (const auto* alternative = absl::get_if<UintValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

optional_ref<const UnknownValue> Value::AsUnknown() const& {
  if (const auto* alternative = absl::get_if<UnknownValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<UnknownValue> Value::AsUnknown() && {
  if (auto* alternative = absl::get_if<UnknownValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

BoolValue Value::GetBool() const {
  ABSL_DCHECK(IsBool()) << *this;
  return absl::get<BoolValue>(variant_);
}

const BytesValue& Value::GetBytes() const& {
  ABSL_DCHECK(IsBytes()) << *this;
  return absl::get<BytesValue>(variant_);
}

BytesValue Value::GetBytes() && {
  ABSL_DCHECK(IsBytes()) << *this;
  return absl::get<BytesValue>(std::move(variant_));
}

DoubleValue Value::GetDouble() const {
  ABSL_DCHECK(IsDouble()) << *this;
  return absl::get<DoubleValue>(variant_);
}

DurationValue Value::GetDuration() const {
  ABSL_DCHECK(IsDuration()) << *this;
  return absl::get<DurationValue>(variant_);
}

const ErrorValue& Value::GetError() const& {
  ABSL_DCHECK(IsError()) << *this;
  return absl::get<ErrorValue>(variant_);
}

ErrorValue Value::GetError() && {
  ABSL_DCHECK(IsError()) << *this;
  return absl::get<ErrorValue>(std::move(variant_));
}

IntValue Value::GetInt() const {
  ABSL_DCHECK(IsInt()) << *this;
  return absl::get<IntValue>(variant_);
}

#ifdef ABSL_HAVE_EXCEPTIONS
#define CEL_VALUE_THROW_BAD_VARIANT_ACCESS() throw absl::bad_variant_access()
#else
#define CEL_VALUE_THROW_BAD_VARIANT_ACCESS() \
  ABSL_LOG(FATAL) << absl::bad_variant_access().what() /* Crash OK */
#endif

ListValue Value::GetList() const& {
  ABSL_DCHECK(IsList()) << *this;
  if (const auto* alternative =
          absl::get_if<common_internal::LegacyListValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedListValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative =
          absl::get_if<ParsedRepeatedFieldValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedJsonListValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  CEL_VALUE_THROW_BAD_VARIANT_ACCESS();
}

ListValue Value::GetList() && {
  ABSL_DCHECK(IsList()) << *this;
  if (auto* alternative =
          absl::get_if<common_internal::LegacyListValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedListValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedRepeatedFieldValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedJsonListValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  CEL_VALUE_THROW_BAD_VARIANT_ACCESS();
}

MapValue Value::GetMap() const& {
  ABSL_DCHECK(IsMap()) << *this;
  if (const auto* alternative =
          absl::get_if<common_internal::LegacyMapValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedMapValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedMapFieldValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedJsonMapValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  CEL_VALUE_THROW_BAD_VARIANT_ACCESS();
}

MapValue Value::GetMap() && {
  ABSL_DCHECK(IsMap()) << *this;
  if (auto* alternative =
          absl::get_if<common_internal::LegacyMapValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedMapValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedMapFieldValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedJsonMapValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  CEL_VALUE_THROW_BAD_VARIANT_ACCESS();
}

MessageValue Value::GetMessage() const& {
  ABSL_DCHECK(IsMessage()) << *this;
  return absl::get<ParsedMessageValue>(variant_);
}

MessageValue Value::GetMessage() && {
  ABSL_DCHECK(IsMessage()) << *this;
  return absl::get<ParsedMessageValue>(std::move(variant_));
}

NullValue Value::GetNull() const {
  ABSL_DCHECK(IsNull()) << *this;
  return absl::get<NullValue>(variant_);
}

const OpaqueValue& Value::GetOpaque() const& {
  ABSL_DCHECK(IsOpaque()) << *this;
  return absl::get<OpaqueValue>(variant_);
}

OpaqueValue Value::GetOpaque() && {
  ABSL_DCHECK(IsOpaque()) << *this;
  return absl::get<OpaqueValue>(std::move(variant_));
}

const OptionalValue& Value::GetOptional() const& {
  ABSL_DCHECK(IsOptional()) << *this;
  return static_cast<const OptionalValue&>(absl::get<OpaqueValue>(variant_));
}

OptionalValue Value::GetOptional() && {
  ABSL_DCHECK(IsOptional()) << *this;
  return static_cast<OptionalValue&&>(
      absl::get<OpaqueValue>(std::move(variant_)));
}

const ParsedJsonListValue& Value::GetParsedJsonList() const& {
  ABSL_DCHECK(IsParsedJsonList()) << *this;
  return absl::get<ParsedJsonListValue>(variant_);
}

ParsedJsonListValue Value::GetParsedJsonList() && {
  ABSL_DCHECK(IsParsedJsonList()) << *this;
  return absl::get<ParsedJsonListValue>(std::move(variant_));
}

const ParsedJsonMapValue& Value::GetParsedJsonMap() const& {
  ABSL_DCHECK(IsParsedJsonMap()) << *this;
  return absl::get<ParsedJsonMapValue>(variant_);
}

ParsedJsonMapValue Value::GetParsedJsonMap() && {
  ABSL_DCHECK(IsParsedJsonMap()) << *this;
  return absl::get<ParsedJsonMapValue>(std::move(variant_));
}

const ParsedListValue& Value::GetParsedList() const& {
  ABSL_DCHECK(IsParsedList()) << *this;
  return absl::get<ParsedListValue>(variant_);
}

ParsedListValue Value::GetParsedList() && {
  ABSL_DCHECK(IsParsedList()) << *this;
  return absl::get<ParsedListValue>(std::move(variant_));
}

const ParsedMapValue& Value::GetParsedMap() const& {
  ABSL_DCHECK(IsParsedMap()) << *this;
  return absl::get<ParsedMapValue>(variant_);
}

ParsedMapValue Value::GetParsedMap() && {
  ABSL_DCHECK(IsParsedMap()) << *this;
  return absl::get<ParsedMapValue>(std::move(variant_));
}

const ParsedMapFieldValue& Value::GetParsedMapField() const& {
  ABSL_DCHECK(IsParsedMapField()) << *this;
  return absl::get<ParsedMapFieldValue>(variant_);
}

ParsedMapFieldValue Value::GetParsedMapField() && {
  ABSL_DCHECK(IsParsedMapField()) << *this;
  return absl::get<ParsedMapFieldValue>(std::move(variant_));
}

const ParsedMessageValue& Value::GetParsedMessage() const& {
  ABSL_DCHECK(IsParsedMessage()) << *this;
  return absl::get<ParsedMessageValue>(variant_);
}

ParsedMessageValue Value::GetParsedMessage() && {
  ABSL_DCHECK(IsParsedMessage()) << *this;
  return absl::get<ParsedMessageValue>(std::move(variant_));
}

const ParsedRepeatedFieldValue& Value::GetParsedRepeatedField() const& {
  ABSL_DCHECK(IsParsedRepeatedField()) << *this;
  return absl::get<ParsedRepeatedFieldValue>(variant_);
}

ParsedRepeatedFieldValue Value::GetParsedRepeatedField() && {
  ABSL_DCHECK(IsParsedRepeatedField()) << *this;
  return absl::get<ParsedRepeatedFieldValue>(std::move(variant_));
}

const ParsedStructValue& Value::GetParsedStruct() const& {
  ABSL_DCHECK(IsParsedMap()) << *this;
  return absl::get<ParsedStructValue>(variant_);
}

ParsedStructValue Value::GetParsedStruct() && {
  ABSL_DCHECK(IsParsedMap()) << *this;
  return absl::get<ParsedStructValue>(std::move(variant_));
}

const StringValue& Value::GetString() const& {
  ABSL_DCHECK(IsString()) << *this;
  return absl::get<StringValue>(variant_);
}

StringValue Value::GetString() && {
  ABSL_DCHECK(IsString()) << *this;
  return absl::get<StringValue>(std::move(variant_));
}

StructValue Value::GetStruct() const& {
  ABSL_DCHECK(IsStruct()) << *this;
  if (const auto* alternative =
          absl::get_if<common_internal::LegacyStructValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedStructValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  CEL_VALUE_THROW_BAD_VARIANT_ACCESS();
}

StructValue Value::GetStruct() && {
  ABSL_DCHECK(IsStruct()) << *this;
  if (auto* alternative =
          absl::get_if<common_internal::LegacyStructValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedStructValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  CEL_VALUE_THROW_BAD_VARIANT_ACCESS();
}

TimestampValue Value::GetTimestamp() const {
  ABSL_DCHECK(IsTimestamp()) << *this;
  return absl::get<TimestampValue>(variant_);
}

const TypeValue& Value::GetType() const& {
  ABSL_DCHECK(IsType()) << *this;
  return absl::get<TypeValue>(variant_);
}

TypeValue Value::GetType() && {
  ABSL_DCHECK(IsType()) << *this;
  return absl::get<TypeValue>(std::move(variant_));
}

UintValue Value::GetUint() const {
  ABSL_DCHECK(IsUint()) << *this;
  return absl::get<UintValue>(variant_);
}

const UnknownValue& Value::GetUnknown() const& {
  ABSL_DCHECK(IsUnknown()) << *this;
  return absl::get<UnknownValue>(variant_);
}

UnknownValue Value::GetUnknown() && {
  ABSL_DCHECK(IsUnknown()) << *this;
  return absl::get<UnknownValue>(std::move(variant_));
}

namespace {

class EmptyValueIterator final : public ValueIterator {
 public:
  bool HasNext() override { return false; }

  absl::Status Next(ValueManager&, Value&) override {
    return absl::FailedPreconditionError(
        "`ValueIterator::Next` called after `ValueIterator::HasNext` returned "
        "false");
  }
};

}  // namespace

absl::Nonnull<std::unique_ptr<ValueIterator>> NewEmptyValueIterator() {
  return std::make_unique<EmptyValueIterator>();
}

bool operator==(IntValue lhs, UintValue rhs) {
  return internal::Number::FromInt64(lhs.NativeValue()) ==
         internal::Number::FromUint64(rhs.NativeValue());
}

bool operator==(UintValue lhs, IntValue rhs) {
  return internal::Number::FromUint64(lhs.NativeValue()) ==
         internal::Number::FromInt64(rhs.NativeValue());
}

bool operator==(IntValue lhs, DoubleValue rhs) {
  return internal::Number::FromInt64(lhs.NativeValue()) ==
         internal::Number::FromDouble(rhs.NativeValue());
}

bool operator==(DoubleValue lhs, IntValue rhs) {
  return internal::Number::FromDouble(lhs.NativeValue()) ==
         internal::Number::FromInt64(rhs.NativeValue());
}

bool operator==(UintValue lhs, DoubleValue rhs) {
  return internal::Number::FromUint64(lhs.NativeValue()) ==
         internal::Number::FromDouble(rhs.NativeValue());
}

bool operator==(DoubleValue lhs, UintValue rhs) {
  return internal::Number::FromDouble(lhs.NativeValue()) ==
         internal::Number::FromUint64(rhs.NativeValue());
}

namespace common_internal {

TrivialValue MakeTrivialValue(const Value& value,
                              absl::Nonnull<google::protobuf::Arena*> arena) {
  return TrivialValue(value.Clone(ArenaAllocator<>{arena}));
}

absl::string_view TrivialValue::ToString() const {
  return (*this)->GetString().value_.AsStringView();
}

absl::string_view TrivialValue::ToBytes() const {
  return (*this)->GetBytes().value_.AsStringView();
}

}  // namespace common_internal

}  // namespace cel
