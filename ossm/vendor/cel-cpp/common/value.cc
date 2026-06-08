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

#include <cstddef>
#include <cstdint>
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
#include "absl/types/variant.h"
#include "common/allocator.h"
#include "common/memory.h"
#include "common/optional_ref.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/list_value_builder.h"
#include "common/values/map_value_builder.h"
#include "common/values/struct_value_builder.h"
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

google::protobuf::Arena* absl_nonnull MessageArenaOr(
    const google::protobuf::Message* absl_nonnull message,
    google::protobuf::Arena* absl_nonnull or_arena) {
  google::protobuf::Arena* absl_nullable arena = message->GetArena();
  if (arena == nullptr) {
    arena = or_arena;
  }
  return arena;
}

}  // namespace

Type Value::GetRuntimeType() const {
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

namespace {

template <typename T>
struct IsMonostate : std::is_same<absl::remove_cvref_t<T>, absl::monostate> {};

}  // namespace

absl::string_view Value::GetTypeName() const {
  return variant_.Visit([](const auto& alternative) -> absl::string_view {
    return alternative.GetTypeName();
  });
}

std::string Value::DebugString() const {
  return variant_.Visit([](const auto& alternative) -> std::string {
    return alternative.DebugString();
  });
}

absl::Status Value::SerializeTo(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(output != nullptr);

  return variant_.Visit([&](const auto& alternative) -> absl::Status {
    return alternative.SerializeTo(descriptor_pool, message_factory, output);
  });
}

absl::Status Value::ConvertToJson(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE);

  return variant_.Visit([descriptor_pool, message_factory,
                         json](const auto& alternative) -> absl::Status {
    return alternative.ConvertToJson(descriptor_pool, message_factory, json);
  });
}

absl::Status Value::ConvertToJsonArray(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE);

  return variant_.Visit(absl::Overload(
      [](absl::monostate) -> absl::Status {
        return absl::InternalError("use of invalid Value");
      },
      [descriptor_pool, message_factory, json](
          const common_internal::LegacyListValue& alternative) -> absl::Status {
        return alternative.ConvertToJsonArray(descriptor_pool, message_factory,
                                              json);
      },
      [descriptor_pool, message_factory,
       json](const CustomListValue& alternative) -> absl::Status {
        return alternative.ConvertToJsonArray(descriptor_pool, message_factory,
                                              json);
      },
      [descriptor_pool, message_factory,
       json](const ParsedRepeatedFieldValue& alternative) -> absl::Status {
        return alternative.ConvertToJsonArray(descriptor_pool, message_factory,
                                              json);
      },
      [descriptor_pool, message_factory,
       json](const ParsedJsonListValue& alternative) -> absl::Status {
        return alternative.ConvertToJsonArray(descriptor_pool, message_factory,
                                              json);
      },
      [](const auto& alternative) -> absl::Status {
        return TypeConversionError(alternative.GetTypeName(),
                                   "google.protobuf.ListValue")
            .NativeValue();
      }));
}

absl::Status Value::ConvertToJsonObject(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT);

  return variant_.Visit(absl::Overload(
      [](absl::monostate) -> absl::Status {
        return absl::InternalError("use of invalid Value");
      },
      [descriptor_pool, message_factory, json](
          const common_internal::LegacyMapValue& alternative) -> absl::Status {
        return alternative.ConvertToJsonObject(descriptor_pool, message_factory,
                                               json);
      },
      [descriptor_pool, message_factory,
       json](const CustomMapValue& alternative) -> absl::Status {
        return alternative.ConvertToJsonObject(descriptor_pool, message_factory,
                                               json);
      },
      [descriptor_pool, message_factory,
       json](const ParsedMapFieldValue& alternative) -> absl::Status {
        return alternative.ConvertToJsonObject(descriptor_pool, message_factory,
                                               json);
      },
      [descriptor_pool, message_factory,
       json](const ParsedJsonMapValue& alternative) -> absl::Status {
        return alternative.ConvertToJsonObject(descriptor_pool, message_factory,
                                               json);
      },
      [descriptor_pool, message_factory,
       json](const common_internal::LegacyStructValue& alternative)
          -> absl::Status {
        return alternative.ConvertToJsonObject(descriptor_pool, message_factory,
                                               json);
      },
      [descriptor_pool, message_factory,
       json](const CustomStructValue& alternative) -> absl::Status {
        return alternative.ConvertToJsonObject(descriptor_pool, message_factory,
                                               json);
      },
      [descriptor_pool, message_factory,
       json](const ParsedMessageValue& alternative) -> absl::Status {
        return alternative.ConvertToJsonObject(descriptor_pool, message_factory,
                                               json);
      },
      [](const auto& alternative) -> absl::Status {
        return TypeConversionError(alternative.GetTypeName(),
                                   "google.protobuf.Struct")
            .NativeValue();
      }));
}

absl::Status Value::Equal(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  return variant_.Visit([&other, descriptor_pool, message_factory, arena,
                         result](const auto& alternative) -> absl::Status {
    return alternative.Equal(other, descriptor_pool, message_factory, arena,
                             result);
  });
}

bool Value::IsZeroValue() const {
  return variant_.Visit([](const auto& alternative) -> bool {
    return alternative.IsZeroValue();
  });
}

namespace {

template <typename, typename = void>
struct HasCloneMethod : std::false_type {};

template <typename T>
struct HasCloneMethod<T, std::void_t<decltype(std::declval<const T>().Clone(
                             std::declval<google::protobuf::Arena* absl_nonnull>()))>>
    : std::true_type {};

}  // namespace

Value Value::Clone(google::protobuf::Arena* absl_nonnull arena) const {
  return variant_.Visit([arena](const auto& alternative) -> Value {
    if constexpr (IsMonostate<decltype(alternative)>::value) {
      return Value();
    } else if constexpr (HasCloneMethod<absl::remove_cvref_t<
                             decltype(alternative)>>::value) {
      return alternative.Clone(arena);
    } else {
      return alternative;
    }
  });
}

std::ostream& operator<<(std::ostream& out, const Value& value) {
  return value.variant_.Visit([&out](const auto& alternative) -> std::ostream& {
    return out << alternative;
  });
}

namespace {

Value NonNullEnumValue(const google::protobuf::EnumValueDescriptor* absl_nonnull value) {
  ABSL_DCHECK(value != nullptr);
  return IntValue(value->number());
}

Value NonNullEnumValue(const google::protobuf::EnumDescriptor* absl_nonnull type,
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

Value Value::Enum(const google::protobuf::EnumValueDescriptor* absl_nonnull value) {
  ABSL_DCHECK(value != nullptr);
  if (value->type()->full_name() == "google.protobuf.NullValue") {
    ABSL_DCHECK_EQ(value->number(), 0);
    return NullValue();
  }
  return NonNullEnumValue(value);
}

Value Value::Enum(const google::protobuf::EnumDescriptor* absl_nonnull type,
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

void BoolMapFieldKeyAccessor(const google::protobuf::MapKey& key,
                             const google::protobuf::Message* absl_nonnull message,
                             google::protobuf::Arena* absl_nonnull arena,
                             Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  *result = BoolValue(key.GetBoolValue());
}

void Int32MapFieldKeyAccessor(const google::protobuf::MapKey& key,
                              const google::protobuf::Message* absl_nonnull message,
                              google::protobuf::Arena* absl_nonnull arena,
                              Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  *result = IntValue(key.GetInt32Value());
}

void Int64MapFieldKeyAccessor(const google::protobuf::MapKey& key,
                              const google::protobuf::Message* absl_nonnull message,
                              google::protobuf::Arena* absl_nonnull arena,
                              Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  *result = IntValue(key.GetInt64Value());
}

void UInt32MapFieldKeyAccessor(const google::protobuf::MapKey& key,
                               const google::protobuf::Message* absl_nonnull message,
                               google::protobuf::Arena* absl_nonnull arena,
                               Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  *result = UintValue(key.GetUInt32Value());
}

void UInt64MapFieldKeyAccessor(const google::protobuf::MapKey& key,
                               const google::protobuf::Message* absl_nonnull message,
                               google::protobuf::Arena* absl_nonnull arena,
                               Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  *result = UintValue(key.GetUInt64Value());
}

void StringMapFieldKeyAccessor(const google::protobuf::MapKey& key,
                               const google::protobuf::Message* absl_nonnull message,
                               google::protobuf::Arena* absl_nonnull arena,
                               Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

#if CEL_INTERNAL_PROTOBUF_OSS_VERSION_PREREQ(5, 30, 0)
  *result = StringValue(Borrower::Arena(MessageArenaOr(message, arena)),
                        key.GetStringValue());
#else
  *result = StringValue(arena, key.GetStringValue());
#endif
}

}  // namespace

absl::StatusOr<MapFieldKeyAccessor> MapFieldKeyAccessorFor(
    const google::protobuf::FieldDescriptor* absl_nonnull field) {
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
    const google::protobuf::MapValueConstRef& value,
    const google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK(!field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE);

  *result = DoubleValue(value.GetDoubleValue());
}

void FloatMapFieldValueAccessor(
    const google::protobuf::MapValueConstRef& value,
    const google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK(!field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_FLOAT);

  *result = DoubleValue(value.GetFloatValue());
}

void Int64MapFieldValueAccessor(
    const google::protobuf::MapValueConstRef& value,
    const google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK(!field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_INT64);

  *result = IntValue(value.GetInt64Value());
}

void UInt64MapFieldValueAccessor(
    const google::protobuf::MapValueConstRef& value,
    const google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK(!field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_UINT64);

  *result = UintValue(value.GetUInt64Value());
}

void Int32MapFieldValueAccessor(
    const google::protobuf::MapValueConstRef& value,
    const google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK(!field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_INT32);

  *result = IntValue(value.GetInt32Value());
}

void UInt32MapFieldValueAccessor(
    const google::protobuf::MapValueConstRef& value,
    const google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK(!field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_UINT32);

  *result = UintValue(value.GetUInt32Value());
}

void BoolMapFieldValueAccessor(
    const google::protobuf::MapValueConstRef& value,
    const google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK(!field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_BOOL);

  *result = BoolValue(value.GetBoolValue());
}

void StringMapFieldValueAccessor(
    const google::protobuf::MapValueConstRef& value,
    const google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK(!field->is_repeated());
  ABSL_DCHECK_EQ(field->type(), google::protobuf::FieldDescriptor::TYPE_STRING);

  if (message->GetArena() == nullptr) {
    *result = StringValue(arena, value.GetStringValue());
  } else {
    *result = StringValue(Borrower::Arena(arena), value.GetStringValue());
  }
}

void MessageMapFieldValueAccessor(
    const google::protobuf::MapValueConstRef& value,
    const google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK(!field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE);

  *result = Value::WrapMessage(&value.GetMessageValue(), descriptor_pool,
                               message_factory, arena);
}

void BytesMapFieldValueAccessor(
    const google::protobuf::MapValueConstRef& value,
    const google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK(!field->is_repeated());
  ABSL_DCHECK_EQ(field->type(), google::protobuf::FieldDescriptor::TYPE_BYTES);

  if (message->GetArena() == nullptr) {
    *result = BytesValue(arena, value.GetStringValue());
  } else {
    *result = BytesValue(Borrower::Arena(arena), value.GetStringValue());
  }
}

void EnumMapFieldValueAccessor(
    const google::protobuf::MapValueConstRef& value,
    const google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK(!field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_ENUM);

  *result = NonNullEnumValue(field->enum_type(), value.GetEnumValue());
}

void NullMapFieldValueAccessor(
    const google::protobuf::MapValueConstRef& value,
    const google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK(!field->is_repeated());
  ABSL_DCHECK(field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_ENUM &&
              field->enum_type()->full_name() == "google.protobuf.NullValue");

  *result = NullValue();
}

}  // namespace

absl::StatusOr<MapFieldValueAccessor> MapFieldValueAccessorFor(
    const google::protobuf::FieldDescriptor* absl_nonnull field) {
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
    int index, const google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field,
    const google::protobuf::Reflection* absl_nonnull reflection,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(reflection != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK_EQ(reflection, message->GetReflection());
  ABSL_DCHECK_EQ(field->containing_type(), message->GetDescriptor());
  ABSL_DCHECK(field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE);
  ABSL_DCHECK_GE(index, 0);
  ABSL_DCHECK_LT(index, reflection->FieldSize(*message, field));

  *result = DoubleValue(reflection->GetRepeatedDouble(*message, field, index));
}

void FloatRepeatedFieldAccessor(
    int index, const google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field,
    const google::protobuf::Reflection* absl_nonnull reflection,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(reflection != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK_EQ(reflection, message->GetReflection());
  ABSL_DCHECK_EQ(field->containing_type(), message->GetDescriptor());
  ABSL_DCHECK(field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_FLOAT);
  ABSL_DCHECK_GE(index, 0);
  ABSL_DCHECK_LT(index, reflection->FieldSize(*message, field));

  *result = DoubleValue(reflection->GetRepeatedFloat(*message, field, index));
}

void Int64RepeatedFieldAccessor(
    int index, const google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field,
    const google::protobuf::Reflection* absl_nonnull reflection,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(reflection != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK_EQ(reflection, message->GetReflection());
  ABSL_DCHECK_EQ(field->containing_type(), message->GetDescriptor());
  ABSL_DCHECK(field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_INT64);
  ABSL_DCHECK_GE(index, 0);
  ABSL_DCHECK_LT(index, reflection->FieldSize(*message, field));

  *result = IntValue(reflection->GetRepeatedInt64(*message, field, index));
}

void UInt64RepeatedFieldAccessor(
    int index, const google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field,
    const google::protobuf::Reflection* absl_nonnull reflection,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(reflection != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK_EQ(reflection, message->GetReflection());
  ABSL_DCHECK_EQ(field->containing_type(), message->GetDescriptor());
  ABSL_DCHECK(field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_UINT64);
  ABSL_DCHECK_GE(index, 0);
  ABSL_DCHECK_LT(index, reflection->FieldSize(*message, field));

  *result = UintValue(reflection->GetRepeatedUInt64(*message, field, index));
}

void Int32RepeatedFieldAccessor(
    int index, const google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field,
    const google::protobuf::Reflection* absl_nonnull reflection,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(reflection != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK_EQ(reflection, message->GetReflection());
  ABSL_DCHECK_EQ(field->containing_type(), message->GetDescriptor());
  ABSL_DCHECK(field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_INT32);
  ABSL_DCHECK_GE(index, 0);
  ABSL_DCHECK_LT(index, reflection->FieldSize(*message, field));

  *result = IntValue(reflection->GetRepeatedInt32(*message, field, index));
}

void UInt32RepeatedFieldAccessor(
    int index, const google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field,
    const google::protobuf::Reflection* absl_nonnull reflection,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(reflection != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK_EQ(reflection, message->GetReflection());
  ABSL_DCHECK_EQ(field->containing_type(), message->GetDescriptor());
  ABSL_DCHECK(field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_UINT32);
  ABSL_DCHECK_GE(index, 0);
  ABSL_DCHECK_LT(index, reflection->FieldSize(*message, field));

  *result = UintValue(reflection->GetRepeatedUInt32(*message, field, index));
}

void BoolRepeatedFieldAccessor(
    int index, const google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field,
    const google::protobuf::Reflection* absl_nonnull reflection,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(reflection != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK_EQ(reflection, message->GetReflection());
  ABSL_DCHECK_EQ(field->containing_type(), message->GetDescriptor());
  ABSL_DCHECK(field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_BOOL);
  ABSL_DCHECK_GE(index, 0);
  ABSL_DCHECK_LT(index, reflection->FieldSize(*message, field));

  *result = BoolValue(reflection->GetRepeatedBool(*message, field, index));
}

void StringRepeatedFieldAccessor(
    int index, const google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field,
    const google::protobuf::Reflection* absl_nonnull reflection,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(reflection != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
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
              *result = StringValue(arena, std::move(scratch));
            } else {
              if (message->GetArena() == nullptr) {
                *result = StringValue(arena, string);
              } else {
                *result = StringValue(Borrower::Arena(arena), string);
              }
            }
          },
          [&](absl::Cord&& cord) { *result = StringValue(std::move(cord)); }),
      well_known_types::AsVariant(well_known_types::GetRepeatedStringField(
          *message, field, index, scratch)));
}

void MessageRepeatedFieldAccessor(
    int index, const google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field,
    const google::protobuf::Reflection* absl_nonnull reflection,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(reflection != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK_EQ(reflection, message->GetReflection());
  ABSL_DCHECK_EQ(field->containing_type(), message->GetDescriptor());
  ABSL_DCHECK(field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE);
  ABSL_DCHECK_GE(index, 0);
  ABSL_DCHECK_LT(index, reflection->FieldSize(*message, field));

  *result = Value::WrapMessage(
      &reflection->GetRepeatedMessage(*message, field, index), descriptor_pool,
      message_factory, arena);
}

void BytesRepeatedFieldAccessor(
    int index, const google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field,
    const google::protobuf::Reflection* absl_nonnull reflection,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(reflection != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
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
              *result = BytesValue(arena, std::move(scratch));
            } else {
              if (message->GetArena() == nullptr) {
                *result = BytesValue(arena, string);
              } else {
                *result = BytesValue(Borrower::Arena(arena), string);
              }
            }
          },
          [&](absl::Cord&& cord) { *result = BytesValue(std::move(cord)); }),
      well_known_types::AsVariant(well_known_types::GetRepeatedBytesField(
          *message, field, index, scratch)));
}

void EnumRepeatedFieldAccessor(
    int index, const google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field,
    const google::protobuf::Reflection* absl_nonnull reflection,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(reflection != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK_EQ(reflection, message->GetReflection());
  ABSL_DCHECK_EQ(field->containing_type(), message->GetDescriptor());
  ABSL_DCHECK(field->is_repeated());
  ABSL_DCHECK_EQ(field->cpp_type(), google::protobuf::FieldDescriptor::CPPTYPE_ENUM);
  ABSL_DCHECK_GE(index, 0);
  ABSL_DCHECK_LT(index, reflection->FieldSize(*message, field));

  *result = NonNullEnumValue(
      field->enum_type(),
      reflection->GetRepeatedEnumValue(*message, field, index));
}

void NullRepeatedFieldAccessor(
    int index, const google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field,
    const google::protobuf::Reflection* absl_nonnull reflection,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(reflection != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK_EQ(reflection, message->GetReflection());
  ABSL_DCHECK_EQ(field->containing_type(), message->GetDescriptor());
  ABSL_DCHECK(field->is_repeated());
  ABSL_DCHECK(field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_ENUM &&
              field->enum_type()->full_name() == "google.protobuf.NullValue");
  ABSL_DCHECK_GE(index, 0);
  ABSL_DCHECK_LT(index, reflection->FieldSize(*message, field));

  *result = NullValue();
}

}  // namespace

absl::StatusOr<RepeatedFieldAccessor> RepeatedFieldAccessorFor(
    const google::protobuf::FieldDescriptor* absl_nonnull field) {
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
  google::protobuf::Arena* absl_nullable arena;
  std::string* absl_nonnull scratch;

  using WellKnownTypesValueVisitor::operator();

  Value operator()(well_known_types::BytesValue&& value) const {
    return absl::visit(absl::Overload(
                           [&](absl::string_view string) -> BytesValue {
                             if (string.empty()) {
                               return BytesValue();
                             }
                             if (scratch->data() == string.data() &&
                                 scratch->size() == string.size()) {
                               return BytesValue(arena, std::move(*scratch));
                             }
                             return BytesValue(arena, string);
                           },
                           [&](absl::Cord&& cord) -> BytesValue {
                             if (cord.empty()) {
                               return BytesValue();
                             }
                             return BytesValue(arena, cord);
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
                               return StringValue(arena, std::move(*scratch));
                             }
                             return StringValue(arena, string);
                           },
                           [&](absl::Cord&& cord) -> StringValue {
                             if (cord.empty()) {
                               return StringValue();
                             }
                             return StringValue(arena, cord);
                           }),
                       well_known_types::AsVariant(std::move(value)));
  }

  Value operator()(well_known_types::ListValue&& value) const {
    return absl::visit(
        absl::Overload(
            [&](well_known_types::ListValueConstRef value) -> ListValue {
              auto* cloned = value.get().New(arena);
              cloned->CopyFrom(value.get());
              return ParsedJsonListValue(cloned, arena);
            },
            [&](well_known_types::ListValuePtr value) -> ListValue {
              if (value->GetArena() != arena) {
                auto* cloned = value->New(arena);
                cloned->CopyFrom(*value);
                return ParsedJsonListValue(cloned, arena);
              }
              return ParsedJsonListValue(value.release(), arena);
            }),
        well_known_types::AsVariant(std::move(value)));
  }

  Value operator()(well_known_types::Struct&& value) const {
    return absl::visit(
        absl::Overload(
            [&](well_known_types::StructConstRef value) -> MapValue {
              auto* cloned = value.get().New(arena);
              cloned->CopyFrom(value.get());
              return ParsedJsonMapValue(cloned, arena);
            },
            [&](well_known_types::StructPtr value) -> MapValue {
              if (value.arena() != arena) {
                auto* cloned = value->New(arena);
                cloned->CopyFrom(*value);
                return ParsedJsonMapValue(cloned, arena);
              }
              return ParsedJsonMapValue(value.release(), arena);
            }),
        well_known_types::AsVariant(std::move(value)));
  }

  Value operator()(Unique<google::protobuf::Message> value) const {
    if (value->GetArena() != arena) {
      auto* cloned = value->New(arena);
      cloned->CopyFrom(*value);
      return ParsedMessageValue(cloned, arena);
    }
    return ParsedMessageValue(value.release(), arena);
  }
};

struct BorrowingWellKnownTypesValueVisitor : public WellKnownTypesValueVisitor {
  const google::protobuf::Message* absl_nonnull message;
  google::protobuf::Arena* absl_nonnull arena;
  std::string* absl_nonnull scratch;

  using WellKnownTypesValueVisitor::operator();

  Value operator()(well_known_types::BytesValue&& value) const {
    return absl::visit(
        absl::Overload(
            [&](absl::string_view string) -> BytesValue {
              if (string.data() == scratch->data() &&
                  string.size() == scratch->size()) {
                return BytesValue(arena, std::move(*scratch));
              } else {
                return BytesValue(
                    Borrower::Arena(MessageArenaOr(message, arena)), string);
              }
            },
            [&](absl::Cord&& cord) -> BytesValue {
              return BytesValue(std::move(cord));
            }),
        well_known_types::AsVariant(std::move(value)));
  }

  Value operator()(well_known_types::StringValue&& value) const {
    return absl::visit(
        absl::Overload(
            [&](absl::string_view string) -> StringValue {
              if (string.data() == scratch->data() &&
                  string.size() == scratch->size()) {
                return StringValue(arena, std::move(*scratch));
              } else {
                return StringValue(
                    Borrower::Arena(MessageArenaOr(message, arena)), string);
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
              return ParsedJsonListValue(&value.get(),
                                         MessageArenaOr(&value.get(), arena));
            },
            [&](well_known_types::ListValuePtr value) -> ParsedJsonListValue {
              if (value->GetArena() != arena) {
                auto* cloned = value->New(arena);
                cloned->CopyFrom(*value);
                return ParsedJsonListValue(cloned, arena);
              }
              return ParsedJsonListValue(value.release(), arena);
            }),
        well_known_types::AsVariant(std::move(value)));
  }

  Value operator()(well_known_types::Struct&& value) const {
    return absl::visit(
        absl::Overload(
            [&](well_known_types::StructConstRef value) -> ParsedJsonMapValue {
              return ParsedJsonMapValue(&value.get(),
                                        MessageArenaOr(&value.get(), arena));
            },
            [&](well_known_types::StructPtr value) -> ParsedJsonMapValue {
              if (value->GetArena() != arena) {
                auto* cloned = value->New(arena);
                cloned->CopyFrom(*value);
                return ParsedJsonMapValue(cloned, arena);
              }
              return ParsedJsonMapValue(value.release(), arena);
            }),
        well_known_types::AsVariant(std::move(value)));
  }

  Value operator()(Unique<google::protobuf::Message>&& value) const {
    if (value->GetArena() != arena) {
      auto* cloned = value->New(arena);
      cloned->CopyFrom(*value);
      return ParsedMessageValue(cloned, arena);
    }
    return ParsedMessageValue(value.release(), arena);
  }
};

}  // namespace

Value Value::FromMessage(
    const google::protobuf::Message& message,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::MessageFactory* absl_nonnull message_factory
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

  std::string scratch;
  auto status_or_adapted = well_known_types::AdaptFromMessage(
      arena, message, descriptor_pool, message_factory, scratch);
  if (ABSL_PREDICT_FALSE(!status_or_adapted.ok())) {
    return ErrorValue(std::move(status_or_adapted).status());
  }
  return absl::visit(
      absl::Overload(
          OwningWellKnownTypesValueVisitor{.arena = arena, .scratch = &scratch},
          [&](absl::monostate) -> Value {
            auto* cloned = message.New(arena);
            cloned->CopyFrom(message);
            return ParsedMessageValue(cloned, arena);
          }),
      std::move(status_or_adapted).value());
}

Value Value::FromMessage(
    google::protobuf::Message&& message,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::MessageFactory* absl_nonnull message_factory
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

  std::string scratch;
  auto status_or_adapted = well_known_types::AdaptFromMessage(
      arena, message, descriptor_pool, message_factory, scratch);
  if (ABSL_PREDICT_FALSE(!status_or_adapted.ok())) {
    return ErrorValue(std::move(status_or_adapted).status());
  }
  return absl::visit(
      absl::Overload(
          OwningWellKnownTypesValueVisitor{.arena = arena, .scratch = &scratch},
          [&](absl::monostate) -> Value {
            auto* cloned = message.New(arena);
            cloned->GetReflection()->Swap(cloned, &message);
            return ParsedMessageValue(cloned, arena);
          }),
      std::move(status_or_adapted).value());
}

Value Value::WrapMessage(
    const google::protobuf::Message* absl_nonnull message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::MessageFactory* absl_nonnull message_factory
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

  std::string scratch;
  absl::StatusOr<well_known_types::Value> adapted_value =
      well_known_types::AdaptFromMessage(arena, *message, descriptor_pool,
                                         message_factory, scratch);
  if (ABSL_PREDICT_FALSE(!adapted_value.ok())) {
    return ErrorValue(std::move(adapted_value).status());
  }
  return absl::visit(
      absl::Overload(
          BorrowingWellKnownTypesValueVisitor{
              .message = message, .arena = arena, .scratch = &scratch},
          [&](absl::monostate) -> Value {
            if (message->GetArena() != arena) {
              auto* cloned = message->New(arena);
              cloned->CopyFrom(*message);
              return ParsedMessageValue(cloned, arena);
            }
            return ParsedMessageValue(message, arena);
          }),
      std::move(adapted_value).value());
}

Value Value::WrapMessageUnsafe(
    const google::protobuf::Message* absl_nonnull message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::MessageFactory* absl_nonnull message_factory
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

  std::string scratch;
  absl::StatusOr<well_known_types::Value> adapted_value =
      well_known_types::AdaptFromMessage(arena, *message, descriptor_pool,
                                         message_factory, scratch);
  if (ABSL_PREDICT_FALSE(!adapted_value.ok())) {
    return ErrorValue(std::move(adapted_value).status());
  }
  return absl::visit(
      absl::Overload(
          BorrowingWellKnownTypesValueVisitor{
              .message = message, .arena = arena, .scratch = &scratch},
          [&](absl::monostate) -> Value {
            if (message->GetArena() != arena) {
              return UnsafeParsedMessageValue(message);
            }
            return ParsedMessageValue(message, arena);
          }),
      std::move(adapted_value).value());
}

namespace {

bool IsWellKnownMessageWrapperType(
    const google::protobuf::Descriptor* absl_nonnull descriptor) {
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

template <typename Unsafe>
Value WrapFieldImpl(
    ProtoWrapperTypeOptions wrapper_type_options,
    const google::protobuf::Message* absl_nonnull message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const google::protobuf::FieldDescriptor* absl_nonnull field
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::MessageFactory* absl_nonnull message_factory
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND) {
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
    if constexpr (Unsafe::value) {
      return UnsafeParsedMapFieldValue(message, field);
    } else {
      return ParsedMapFieldValue(message, field,
                                 MessageArenaOr(message, arena));
    }
  }
  if (field->is_repeated()) {
    if (reflection->FieldSize(*message, field) == 0) {
      return ListValue();
    }
    if constexpr (Unsafe::value) {
      return UnsafeParsedRepeatedFieldValue(message, field);
    } else {
      return ParsedRepeatedFieldValue(message, field,
                                      MessageArenaOr(message, arena));
    }
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
                  return StringValue(arena, std::move(scratch));
                } else {
                  return StringValue(
                      Borrower::Arena(MessageArenaOr(message, arena)), string);
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
      if constexpr (Unsafe::value) {
        return Value::WrapMessageUnsafe(
            &reflection->GetMessage(*message, field), descriptor_pool,
            message_factory, arena);
      } else {
        return Value::WrapMessage(&reflection->GetMessage(*message, field),
                                  descriptor_pool, message_factory, arena);
      }
    case google::protobuf::FieldDescriptor::TYPE_BYTES: {
      std::string scratch;
      return absl::visit(
          absl::Overload(
              [&](absl::string_view string) -> BytesValue {
                if (string.data() == scratch.data() &&
                    string.size() == scratch.size()) {
                  return BytesValue(arena, std::move(scratch));
                } else {
                  return BytesValue(
                      Borrower::Arena(MessageArenaOr(message, arena)), string);
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

template <typename Unsafe>
Value WrapRepeatedFieldImpl(
    int index,
    const google::protobuf::Message* absl_nonnull message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const google::protobuf::FieldDescriptor* absl_nonnull field
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::MessageFactory* absl_nonnull message_factory
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK_EQ(field->containing_type(), message->GetDescriptor());
  ABSL_DCHECK(!field->is_map() && field->is_repeated());
  ABSL_DCHECK_GE(index, 0);
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

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
                  return StringValue(arena, std::move(scratch));
                } else {
                  return StringValue(
                      Borrower::Arena(MessageArenaOr(message, arena)), string);
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
      if constexpr (Unsafe::value) {
        return Value::WrapMessageUnsafe(
            &reflection->GetRepeatedMessage(*message, field, index),
            descriptor_pool, message_factory, arena);
      } else {
        return Value::WrapMessage(
            &reflection->GetRepeatedMessage(*message, field, index),
            descriptor_pool, message_factory, arena);
      }
    case google::protobuf::FieldDescriptor::TYPE_BYTES: {
      std::string scratch;
      return absl::visit(
          absl::Overload(
              [&](absl::string_view string) -> BytesValue {
                if (string.data() == scratch.data() &&
                    string.size() == scratch.size()) {
                  return BytesValue(arena, std::move(scratch));
                } else {
                  return BytesValue(
                      Borrower::Arena(MessageArenaOr(message, arena)), string);
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
      return Value::Enum(field->enum_type(), reflection->GetRepeatedEnumValue(
                                                 *message, field, index));
    default:
      return ErrorValue(absl::InvalidArgumentError(
          absl::StrCat("unexpected message field type: ", field->type_name())));
  }
}

template <typename Unsafe>
Value WrapMapFieldValueImpl(
    const google::protobuf::MapValueConstRef& value,
    const google::protobuf::Message* absl_nonnull message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const google::protobuf::FieldDescriptor* absl_nonnull field
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::MessageFactory* absl_nonnull message_factory
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  ABSL_DCHECK(field != nullptr);
  ABSL_DCHECK_EQ(field->containing_type()->containing_type(),
                 message->GetDescriptor());
  ABSL_DCHECK(!field->is_map() && !field->is_repeated());
  ABSL_DCHECK_EQ(value.type(), field->cpp_type());
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

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
      return StringValue(Borrower::Arena(MessageArenaOr(message, arena)),
                         value.GetStringValue());
    case google::protobuf::FieldDescriptor::TYPE_GROUP:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
      if constexpr (Unsafe::value) {
        return Value::WrapMessageUnsafe(
            &value.GetMessageValue(), descriptor_pool, message_factory, arena);
      } else {
        return Value::WrapMessage(&value.GetMessageValue(), descriptor_pool,
                                  message_factory, arena);
      }
    case google::protobuf::FieldDescriptor::TYPE_BYTES:
      return BytesValue(Borrower::Arena(MessageArenaOr(message, arena)),
                        value.GetStringValue());
    case google::protobuf::FieldDescriptor::TYPE_FIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_UINT32:
      return UintValue(value.GetUInt32Value());
    case google::protobuf::FieldDescriptor::TYPE_ENUM:
      return Value::Enum(field->enum_type(), value.GetEnumValue());
    default:
      return ErrorValue(absl::InvalidArgumentError(
          absl::StrCat("unexpected message field type: ", field->type_name())));
  }
}

}  // namespace

Value Value::WrapField(
    ProtoWrapperTypeOptions wrapper_type_options,
    const google::protobuf::Message* absl_nonnull message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const google::protobuf::FieldDescriptor* absl_nonnull field
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::MessageFactory* absl_nonnull message_factory
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  using Unsafe = std::false_type;
  return WrapFieldImpl<Unsafe>(wrapper_type_options, message, field,
                               descriptor_pool, message_factory, arena);
}

Value Value::WrapFieldUnsafe(
    ProtoWrapperTypeOptions wrapper_type_options,
    const google::protobuf::Message* absl_nonnull message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const google::protobuf::FieldDescriptor* absl_nonnull field
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::MessageFactory* absl_nonnull message_factory
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  using Unsafe = std::true_type;
  return WrapFieldImpl<Unsafe>(wrapper_type_options, message, field,
                               descriptor_pool, message_factory, arena);
}

Value Value::WrapRepeatedField(
    int index,
    const google::protobuf::Message* absl_nonnull message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const google::protobuf::FieldDescriptor* absl_nonnull field
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::MessageFactory* absl_nonnull message_factory
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  using Unsafe = std::false_type;
  return WrapRepeatedFieldImpl<Unsafe>(index, message, field, descriptor_pool,
                                       message_factory, arena);
}

Value Value::WrapRepeatedFieldUnsafe(
    int index,
    const google::protobuf::Message* absl_nonnull message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const google::protobuf::FieldDescriptor* absl_nonnull field
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::MessageFactory* absl_nonnull message_factory
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  using Unsafe = std::true_type;
  return WrapRepeatedFieldImpl<Unsafe>(index, message, field, descriptor_pool,
                                       message_factory, arena);
}

StringValue Value::WrapMapFieldKeyString(
    const google::protobuf::MapKey& key,
    const google::protobuf::Message* absl_nonnull message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  ABSL_DCHECK(message != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK_EQ(key.type(), google::protobuf::FieldDescriptor::CPPTYPE_STRING);

#if CEL_INTERNAL_PROTOBUF_OSS_VERSION_PREREQ(5, 30, 0)
  return StringValue(Borrower::Arena(MessageArenaOr(message, arena)),
                     key.GetStringValue());
#else
  return StringValue(arena, key.GetStringValue());
#endif
}

Value Value::WrapMapFieldValue(
    const google::protobuf::MapValueConstRef& value,
    const google::protobuf::Message* absl_nonnull message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const google::protobuf::FieldDescriptor* absl_nonnull field
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::MessageFactory* absl_nonnull message_factory
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  using Unsafe = std::false_type;
  return WrapMapFieldValueImpl<Unsafe>(value, message, field, descriptor_pool,
                                       message_factory, arena);
}

Value Value::WrapMapFieldValueUnsafe(
    const google::protobuf::MapValueConstRef& value,
    const google::protobuf::Message* absl_nonnull message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const google::protobuf::FieldDescriptor* absl_nonnull field
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::MessageFactory* absl_nonnull message_factory
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  using Unsafe = std::true_type;
  return WrapMapFieldValueImpl<Unsafe>(value, message, field, descriptor_pool,
                                       message_factory, arena);
}

optional_ref<const BytesValue> Value::AsBytes() const& {
  if (const auto* alternative = variant_.As<BytesValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<BytesValue> Value::AsBytes() && {
  if (auto* alternative = variant_.As<BytesValue>(); alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

absl::optional<DoubleValue> Value::AsDouble() const {
  if (const auto* alternative = variant_.As<DoubleValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<DurationValue> Value::AsDuration() const {
  if (const auto* alternative = variant_.As<DurationValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

optional_ref<const ErrorValue> Value::AsError() const& {
  if (const auto* alternative = variant_.As<ErrorValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ErrorValue> Value::AsError() && {
  if (auto* alternative = variant_.As<ErrorValue>(); alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

absl::optional<IntValue> Value::AsInt() const {
  if (const auto* alternative = variant_.As<IntValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ListValue> Value::AsList() const& {
  if (const auto* alternative = variant_.As<common_internal::LegacyListValue>();
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = variant_.As<CustomListValue>();
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = variant_.As<ParsedRepeatedFieldValue>();
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = variant_.As<ParsedJsonListValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ListValue> Value::AsList() && {
  if (auto* alternative = variant_.As<common_internal::LegacyListValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = variant_.As<CustomListValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = variant_.As<ParsedRepeatedFieldValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = variant_.As<ParsedJsonListValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

absl::optional<MapValue> Value::AsMap() const& {
  if (const auto* alternative = variant_.As<common_internal::LegacyMapValue>();
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = variant_.As<CustomMapValue>();
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = variant_.As<ParsedMapFieldValue>();
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = variant_.As<ParsedJsonMapValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<MapValue> Value::AsMap() && {
  if (auto* alternative = variant_.As<common_internal::LegacyMapValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = variant_.As<CustomMapValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = variant_.As<ParsedMapFieldValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = variant_.As<ParsedJsonMapValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

absl::optional<MessageValue> Value::AsMessage() const& {
  if (const auto* alternative = variant_.As<ParsedMessageValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<MessageValue> Value::AsMessage() && {
  if (auto* alternative = variant_.As<ParsedMessageValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

absl::optional<NullValue> Value::AsNull() const {
  if (const auto* alternative = variant_.As<NullValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

optional_ref<const OpaqueValue> Value::AsOpaque() const& {
  if (const auto* alternative = variant_.As<OpaqueValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<OpaqueValue> Value::AsOpaque() && {
  if (auto* alternative = variant_.As<OpaqueValue>(); alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const OptionalValue> Value::AsOptional() const& {
  if (const auto* alternative = variant_.As<OpaqueValue>();
      alternative != nullptr && alternative->IsOptional()) {
    return static_cast<const OptionalValue&>(*alternative);
  }
  return absl::nullopt;
}

absl::optional<OptionalValue> Value::AsOptional() && {
  if (auto* alternative = variant_.As<OpaqueValue>();
      alternative != nullptr && alternative->IsOptional()) {
    return static_cast<OptionalValue&&>(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const ParsedJsonListValue> Value::AsParsedJsonList() const& {
  if (const auto* alternative = variant_.As<ParsedJsonListValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedJsonListValue> Value::AsParsedJsonList() && {
  if (auto* alternative = variant_.As<ParsedJsonListValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const ParsedJsonMapValue> Value::AsParsedJsonMap() const& {
  if (const auto* alternative = variant_.As<ParsedJsonMapValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedJsonMapValue> Value::AsParsedJsonMap() && {
  if (auto* alternative = variant_.As<ParsedJsonMapValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const CustomListValue> Value::AsCustomList() const& {
  if (const auto* alternative = variant_.As<CustomListValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<CustomListValue> Value::AsCustomList() && {
  if (auto* alternative = variant_.As<CustomListValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const CustomMapValue> Value::AsCustomMap() const& {
  if (const auto* alternative = variant_.As<CustomMapValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<CustomMapValue> Value::AsCustomMap() && {
  if (auto* alternative = variant_.As<CustomMapValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const ParsedMapFieldValue> Value::AsParsedMapField() const& {
  if (const auto* alternative = variant_.As<ParsedMapFieldValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedMapFieldValue> Value::AsParsedMapField() && {
  if (auto* alternative = variant_.As<ParsedMapFieldValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const ParsedMessageValue> Value::AsParsedMessage() const& {
  if (const auto* alternative = variant_.As<ParsedMessageValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedMessageValue> Value::AsParsedMessage() && {
  if (auto* alternative = variant_.As<ParsedMessageValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const ParsedRepeatedFieldValue> Value::AsParsedRepeatedField()
    const& {
  if (const auto* alternative = variant_.As<ParsedRepeatedFieldValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedRepeatedFieldValue> Value::AsParsedRepeatedField() && {
  if (auto* alternative = variant_.As<ParsedRepeatedFieldValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const CustomStructValue> Value::AsCustomStruct() const& {
  if (const auto* alternative = variant_.As<CustomStructValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<CustomStructValue> Value::AsCustomStruct() && {
  if (auto* alternative = variant_.As<CustomStructValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const StringValue> Value::AsString() const& {
  if (const auto* alternative = variant_.As<StringValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<StringValue> Value::AsString() && {
  if (auto* alternative = variant_.As<StringValue>(); alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

absl::optional<StructValue> Value::AsStruct() const& {
  if (const auto* alternative =
          variant_.As<common_internal::LegacyStructValue>();
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = variant_.As<CustomStructValue>();
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = variant_.As<ParsedMessageValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<StructValue> Value::AsStruct() && {
  if (auto* alternative = variant_.As<common_internal::LegacyStructValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = variant_.As<CustomStructValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = variant_.As<ParsedMessageValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

absl::optional<TimestampValue> Value::AsTimestamp() const {
  if (const auto* alternative = variant_.As<TimestampValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

optional_ref<const TypeValue> Value::AsType() const& {
  if (const auto* alternative = variant_.As<TypeValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<TypeValue> Value::AsType() && {
  if (auto* alternative = variant_.As<TypeValue>(); alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

absl::optional<UintValue> Value::AsUint() const {
  if (const auto* alternative = variant_.As<UintValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

optional_ref<const UnknownValue> Value::AsUnknown() const& {
  if (const auto* alternative = variant_.As<UnknownValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<UnknownValue> Value::AsUnknown() && {
  if (auto* alternative = variant_.As<UnknownValue>(); alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

const BytesValue& Value::GetBytes() const& {
  ABSL_DCHECK(IsBytes()) << *this;
  return variant_.Get<BytesValue>();
}

BytesValue Value::GetBytes() && {
  ABSL_DCHECK(IsBytes()) << *this;
  return std::move(variant_).Get<BytesValue>();
}

DoubleValue Value::GetDouble() const {
  ABSL_DCHECK(IsDouble()) << *this;
  return variant_.Get<DoubleValue>();
}

DurationValue Value::GetDuration() const {
  ABSL_DCHECK(IsDuration()) << *this;
  return variant_.Get<DurationValue>();
}

const ErrorValue& Value::GetError() const& {
  ABSL_DCHECK(IsError()) << *this;
  return variant_.Get<ErrorValue>();
}

ErrorValue Value::GetError() && {
  ABSL_DCHECK(IsError()) << *this;
  return std::move(variant_).Get<ErrorValue>();
}

IntValue Value::GetInt() const {
  ABSL_DCHECK(IsInt()) << *this;
  return variant_.Get<IntValue>();
}

#ifdef ABSL_HAVE_EXCEPTIONS
#define CEL_VALUE_THROW_BAD_VARIANT_ACCESS() throw absl::bad_variant_access()
#else
#define CEL_VALUE_THROW_BAD_VARIANT_ACCESS() \
  ABSL_LOG(FATAL) << absl::bad_variant_access().what() /* Crash OK */
#endif

ListValue Value::GetList() const& {
  ABSL_DCHECK(IsList()) << *this;
  if (const auto* alternative = variant_.As<common_internal::LegacyListValue>();
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = variant_.As<CustomListValue>();
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = variant_.As<ParsedRepeatedFieldValue>();
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = variant_.As<ParsedJsonListValue>();
      alternative != nullptr) {
    return *alternative;
  }
  CEL_VALUE_THROW_BAD_VARIANT_ACCESS();
}

ListValue Value::GetList() && {
  ABSL_DCHECK(IsList()) << *this;
  if (auto* alternative = variant_.As<common_internal::LegacyListValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = variant_.As<CustomListValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = variant_.As<ParsedRepeatedFieldValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = variant_.As<ParsedJsonListValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  CEL_VALUE_THROW_BAD_VARIANT_ACCESS();
}

MapValue Value::GetMap() const& {
  ABSL_DCHECK(IsMap()) << *this;
  if (const auto* alternative = variant_.As<common_internal::LegacyMapValue>();
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = variant_.As<CustomMapValue>();
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = variant_.As<ParsedMapFieldValue>();
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = variant_.As<ParsedJsonMapValue>();
      alternative != nullptr) {
    return *alternative;
  }
  CEL_VALUE_THROW_BAD_VARIANT_ACCESS();
}

MapValue Value::GetMap() && {
  ABSL_DCHECK(IsMap()) << *this;
  if (auto* alternative = variant_.As<common_internal::LegacyMapValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = variant_.As<CustomMapValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = variant_.As<ParsedMapFieldValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = variant_.As<ParsedJsonMapValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  CEL_VALUE_THROW_BAD_VARIANT_ACCESS();
}

MessageValue Value::GetMessage() const& {
  ABSL_DCHECK(IsMessage()) << *this;
  return variant_.Get<ParsedMessageValue>();
}

MessageValue Value::GetMessage() && {
  ABSL_DCHECK(IsMessage()) << *this;
  return std::move(variant_).Get<ParsedMessageValue>();
}

NullValue Value::GetNull() const {
  ABSL_DCHECK(IsNull()) << *this;
  return variant_.Get<NullValue>();
}

const OpaqueValue& Value::GetOpaque() const& {
  ABSL_DCHECK(IsOpaque()) << *this;
  return variant_.Get<OpaqueValue>();
}

OpaqueValue Value::GetOpaque() && {
  ABSL_DCHECK(IsOpaque()) << *this;
  return std::move(variant_).Get<OpaqueValue>();
}

const OptionalValue& Value::GetOptional() const& {
  ABSL_DCHECK(IsOptional()) << *this;
  return static_cast<const OptionalValue&>(variant_.Get<OpaqueValue>());
}

OptionalValue Value::GetOptional() && {
  ABSL_DCHECK(IsOptional()) << *this;
  return static_cast<OptionalValue&&>(std::move(variant_).Get<OpaqueValue>());
}

const ParsedJsonListValue& Value::GetParsedJsonList() const& {
  ABSL_DCHECK(IsParsedJsonList()) << *this;
  return variant_.Get<ParsedJsonListValue>();
}

ParsedJsonListValue Value::GetParsedJsonList() && {
  ABSL_DCHECK(IsParsedJsonList()) << *this;
  return std::move(variant_).Get<ParsedJsonListValue>();
}

const ParsedJsonMapValue& Value::GetParsedJsonMap() const& {
  ABSL_DCHECK(IsParsedJsonMap()) << *this;
  return variant_.Get<ParsedJsonMapValue>();
}

ParsedJsonMapValue Value::GetParsedJsonMap() && {
  ABSL_DCHECK(IsParsedJsonMap()) << *this;
  return std::move(variant_).Get<ParsedJsonMapValue>();
}

const CustomListValue& Value::GetCustomList() const& {
  ABSL_DCHECK(IsCustomList()) << *this;
  return variant_.Get<CustomListValue>();
}

CustomListValue Value::GetCustomList() && {
  ABSL_DCHECK(IsCustomList()) << *this;
  return std::move(variant_).Get<CustomListValue>();
}

const CustomMapValue& Value::GetCustomMap() const& {
  ABSL_DCHECK(IsCustomMap()) << *this;
  return variant_.Get<CustomMapValue>();
}

CustomMapValue Value::GetCustomMap() && {
  ABSL_DCHECK(IsCustomMap()) << *this;
  return std::move(variant_).Get<CustomMapValue>();
}

const ParsedMapFieldValue& Value::GetParsedMapField() const& {
  ABSL_DCHECK(IsParsedMapField()) << *this;
  return variant_.Get<ParsedMapFieldValue>();
}

ParsedMapFieldValue Value::GetParsedMapField() && {
  ABSL_DCHECK(IsParsedMapField()) << *this;
  return std::move(variant_).Get<ParsedMapFieldValue>();
}

const ParsedMessageValue& Value::GetParsedMessage() const& {
  ABSL_DCHECK(IsParsedMessage()) << *this;
  return variant_.Get<ParsedMessageValue>();
}

ParsedMessageValue Value::GetParsedMessage() && {
  ABSL_DCHECK(IsParsedMessage()) << *this;
  return std::move(variant_).Get<ParsedMessageValue>();
}

const ParsedRepeatedFieldValue& Value::GetParsedRepeatedField() const& {
  ABSL_DCHECK(IsParsedRepeatedField()) << *this;
  return variant_.Get<ParsedRepeatedFieldValue>();
}

ParsedRepeatedFieldValue Value::GetParsedRepeatedField() && {
  ABSL_DCHECK(IsParsedRepeatedField()) << *this;
  return std::move(variant_).Get<ParsedRepeatedFieldValue>();
}

const CustomStructValue& Value::GetCustomStruct() const& {
  ABSL_DCHECK(IsCustomStruct()) << *this;
  return variant_.Get<CustomStructValue>();
}

CustomStructValue Value::GetCustomStruct() && {
  ABSL_DCHECK(IsCustomStruct()) << *this;
  return std::move(variant_).Get<CustomStructValue>();
}

const StringValue& Value::GetString() const& {
  ABSL_DCHECK(IsString()) << *this;
  return variant_.Get<StringValue>();
}

StringValue Value::GetString() && {
  ABSL_DCHECK(IsString()) << *this;
  return std::move(variant_).Get<StringValue>();
}

StructValue Value::GetStruct() const& {
  ABSL_DCHECK(IsStruct()) << *this;
  if (const auto* alternative =
          variant_.As<common_internal::LegacyStructValue>();
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = variant_.As<CustomStructValue>();
      alternative != nullptr) {
    return *alternative;
  }
  if (const auto* alternative = variant_.As<ParsedMessageValue>();
      alternative != nullptr) {
    return *alternative;
  }
  CEL_VALUE_THROW_BAD_VARIANT_ACCESS();
}

StructValue Value::GetStruct() && {
  ABSL_DCHECK(IsStruct()) << *this;
  if (auto* alternative = variant_.As<common_internal::LegacyStructValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = variant_.As<CustomStructValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  if (auto* alternative = variant_.As<ParsedMessageValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  CEL_VALUE_THROW_BAD_VARIANT_ACCESS();
}

TimestampValue Value::GetTimestamp() const {
  ABSL_DCHECK(IsTimestamp()) << *this;
  return variant_.Get<TimestampValue>();
}

const TypeValue& Value::GetType() const& {
  ABSL_DCHECK(IsType()) << *this;
  return variant_.Get<TypeValue>();
}

TypeValue Value::GetType() && {
  ABSL_DCHECK(IsType()) << *this;
  return std::move(variant_).Get<TypeValue>();
}

UintValue Value::GetUint() const {
  ABSL_DCHECK(IsUint()) << *this;
  return variant_.Get<UintValue>();
}

const UnknownValue& Value::GetUnknown() const& {
  ABSL_DCHECK(IsUnknown()) << *this;
  return variant_.Get<UnknownValue>();
}

UnknownValue Value::GetUnknown() && {
  ABSL_DCHECK(IsUnknown()) << *this;
  return std::move(variant_).Get<UnknownValue>();
}

namespace {

class EmptyValueIterator final : public ValueIterator {
 public:
  bool HasNext() override { return false; }

  absl::Status Next(const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                    google::protobuf::MessageFactory* absl_nonnull message_factory,
                    google::protobuf::Arena* absl_nonnull arena,
                    Value* absl_nonnull result) override {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(arena != nullptr);
    ABSL_DCHECK(result != nullptr);

    return absl::FailedPreconditionError(
        "`ValueIterator::Next` called after `ValueIterator::HasNext` returned "
        "false");
  }

  absl::StatusOr<bool> Next1(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena,
      Value* absl_nonnull key_or_value) override {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(arena != nullptr);
    ABSL_DCHECK(key_or_value != nullptr);

    return false;
  }

  absl::StatusOr<bool> Next2(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull key,
      Value* absl_nullable value) override {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(arena != nullptr);
    ABSL_DCHECK(key != nullptr);

    return false;
  }
};

}  // namespace

absl_nonnull std::unique_ptr<ValueIterator> NewEmptyValueIterator() {
  return std::make_unique<EmptyValueIterator>();
}

absl_nonnull ListValueBuilderPtr
NewListValueBuilder(google::protobuf::Arena* absl_nonnull arena) {
  ABSL_DCHECK(arena != nullptr);
  return common_internal::NewListValueBuilder(arena);
}

absl_nonnull MapValueBuilderPtr
NewMapValueBuilder(google::protobuf::Arena* absl_nonnull arena) {
  ABSL_DCHECK(arena != nullptr);
  return common_internal::NewMapValueBuilder(arena);
}

absl_nullable StructValueBuilderPtr NewStructValueBuilder(
    google::protobuf::Arena* absl_nonnull arena,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    absl::string_view name) {
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  return common_internal::NewStructValueBuilder(arena, descriptor_pool,
                                                message_factory, name);
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

absl::StatusOr<bool> ValueIterator::Next1(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull value) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(value != nullptr);

  if (HasNext()) {
    CEL_RETURN_IF_ERROR(Next(descriptor_pool, message_factory, arena, value));
    return true;
  }
  return false;
}

}  // namespace cel
