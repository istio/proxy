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

#include "common/values/struct_value_builder.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/functional/overload.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "base/internal/message_wrapper.h"
#include "common/allocator.h"
#include "common/any.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_introspector.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "common/value_manager.h"
#include "extensions/protobuf/internal/map_reflection.h"
#include "internal/json.h"
#include "internal/status_macros.h"
#include "internal/well_known_types.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

// TODO: Improve test coverage for struct value builder

namespace cel::common_internal {

namespace {

class CompatTypeReflector final : public TypeReflector {
 public:
  CompatTypeReflector(absl::Nonnull<const google::protobuf::DescriptorPool*> pool,
                      absl::Nonnull<google::protobuf::MessageFactory*> factory)
      : pool_(pool), factory_(factory) {}

  absl::Nullable<const google::protobuf::DescriptorPool*> descriptor_pool()
      const override {
    return pool_;
  }

  absl::Nullable<google::protobuf::MessageFactory*> message_factory() const override {
    return factory_;
  }

 protected:
  absl::StatusOr<absl::optional<Type>> FindTypeImpl(
      TypeFactory& type_factory, absl::string_view name) const final {
    // We do not have to worry about well known types here.
    // `TypeIntrospector::FindType` handles those directly.
    const auto* desc = descriptor_pool()->FindMessageTypeByName(name);
    if (desc == nullptr) {
      return absl::nullopt;
    }
    return MessageType(desc);
  }

  absl::StatusOr<absl::optional<TypeIntrospector::EnumConstant>>
  FindEnumConstantImpl(TypeFactory&, absl::string_view type,
                       absl::string_view value) const final {
    const google::protobuf::EnumDescriptor* enum_desc =
        descriptor_pool()->FindEnumTypeByName(type);
    // google.protobuf.NullValue is special cased in the base class.
    if (enum_desc == nullptr) {
      return absl::nullopt;
    }

    // Note: we don't support strong enum typing at this time so only the fully
    // qualified enum values are meaningful, so we don't provide any signal if
    // the enum type is found but can't match the value name.
    const google::protobuf::EnumValueDescriptor* value_desc =
        enum_desc->FindValueByName(value);
    if (value_desc == nullptr) {
      return absl::nullopt;
    }

    return TypeIntrospector::EnumConstant{
        EnumType(enum_desc), enum_desc->full_name(), value_desc->name(),
        value_desc->number()};
  }

  absl::StatusOr<absl::optional<StructTypeField>> FindStructTypeFieldByNameImpl(
      TypeFactory& type_factory, absl::string_view type,
      absl::string_view name) const final {
    // We do not have to worry about well known types here.
    // `TypeIntrospector::FindStructTypeFieldByName` handles those directly.
    const auto* desc = descriptor_pool()->FindMessageTypeByName(type);
    if (desc == nullptr) {
      return absl::nullopt;
    }
    const auto* field_desc = desc->FindFieldByName(name);
    if (field_desc == nullptr) {
      field_desc = descriptor_pool()->FindExtensionByPrintableName(desc, name);
      if (field_desc == nullptr) {
        return absl::nullopt;
      }
    }
    return MessageTypeField(field_desc);
  }

  absl::StatusOr<absl::optional<Value>> DeserializeValueImpl(
      ValueFactory& value_factory, absl::string_view type_url,
      const absl::Cord& value) const override {
    absl::string_view type_name;
    if (!ParseTypeUrl(type_url, &type_name)) {
      return absl::InvalidArgumentError("invalid type URL");
    }
    const auto* descriptor =
        descriptor_pool()->FindMessageTypeByName(type_name);
    if (descriptor == nullptr) {
      return absl::nullopt;
    }
    const auto* prototype = message_factory()->GetPrototype(descriptor);
    if (prototype == nullptr) {
      return absl::nullopt;
    }
    absl::Nullable<google::protobuf::Arena*> arena =
        value_factory.GetMemoryManager().arena();
    auto message = WrapShared(prototype->New(arena), arena);
    if (!message->ParsePartialFromCord(value)) {
      return absl::InvalidArgumentError(
          absl::StrCat("failed to parse `", type_url, "`"));
    }
    return Value::Message(WrapShared(prototype->New(arena), arena), pool_,
                          factory_);
  }

 private:
  const google::protobuf::DescriptorPool* const pool_;
  google::protobuf::MessageFactory* const factory_;
};

class CompatValueManager final : public ValueManager {
 public:
  CompatValueManager(absl::Nullable<google::protobuf::Arena*> arena,
                     absl::Nonnull<const google::protobuf::DescriptorPool*> pool,
                     absl::Nonnull<google::protobuf::MessageFactory*> factory)
      : arena_(arena), reflector_(pool, factory) {}

  MemoryManagerRef GetMemoryManager() const override {
    return arena_ != nullptr ? MemoryManager::Pooling(arena_)
                             : MemoryManager::ReferenceCounting();
  }

  const TypeIntrospector& GetTypeIntrospector() const override {
    return reflector_;
  }

  const TypeReflector& GetTypeReflector() const override { return reflector_; }

  absl::Nullable<const google::protobuf::DescriptorPool*> descriptor_pool()
      const override {
    return reflector_.descriptor_pool();
  }

  absl::Nullable<google::protobuf::MessageFactory*> message_factory() const override {
    return reflector_.message_factory();
  }

 private:
  absl::Nullable<google::protobuf::Arena*> const arena_;
  CompatTypeReflector reflector_;
};

absl::StatusOr<absl::Nonnull<const google::protobuf::Descriptor*>> GetDescriptor(
    const google::protobuf::Message& message) {
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InvalidArgumentError(
        absl::StrCat(message.GetTypeName(), " is missing descriptor"));
  }
  return desc;
}

absl::Status ProtoMessageCopyUsingSerialization(
    google::protobuf::MessageLite* to, const google::protobuf::MessageLite* from) {
  ABSL_DCHECK_EQ(to->GetTypeName(), from->GetTypeName());
  absl::Cord serialized;
  if (!from->SerializePartialToCord(&serialized)) {
    return absl::UnknownError(
        absl::StrCat("failed to serialize `", from->GetTypeName(), "`"));
  }
  if (!to->ParsePartialFromCord(serialized)) {
    return absl::UnknownError(
        absl::StrCat("failed to parse `", to->GetTypeName(), "`"));
  }
  return absl::OkStatus();
}

absl::Status ProtoMessageCopy(
    absl::Nonnull<google::protobuf::Message*> to_message,
    absl::Nonnull<const google::protobuf::Descriptor*> to_descriptor,
    absl::Nonnull<const google::protobuf::Message*> from_message) {
  CEL_ASSIGN_OR_RETURN(const auto* from_descriptor,
                       GetDescriptor(*from_message));
  if (to_descriptor == from_descriptor) {
    // Same.
    to_message->CopyFrom(*from_message);
    return absl::OkStatus();
  }
  if (to_descriptor->full_name() == from_descriptor->full_name()) {
    // Same type, different descriptors.
    return ProtoMessageCopyUsingSerialization(to_message, from_message);
  }
  return TypeConversionError(from_descriptor->full_name(),
                             to_descriptor->full_name())
      .NativeValue();
}

absl::Status ProtoMessageFromValueImpl(
    const Value& value, absl::Nonnull<const google::protobuf::DescriptorPool*> pool,
    absl::Nonnull<google::protobuf::MessageFactory*> factory,
    absl::Nonnull<well_known_types::Reflection*> well_known_types,
    absl::Nonnull<google::protobuf::Message*> message) {
  CEL_ASSIGN_OR_RETURN(const auto* to_desc, GetDescriptor(*message));
  switch (to_desc->well_known_type()) {
    case google::protobuf::Descriptor::WELLKNOWNTYPE_FLOATVALUE: {
      if (auto double_value = value.AsDouble(); double_value) {
        CEL_RETURN_IF_ERROR(well_known_types->FloatValue().Initialize(
            message->GetDescriptor()));
        well_known_types->FloatValue().SetValue(
            message, static_cast<float>(double_value->NativeValue()));
        return absl::OkStatus();
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_DOUBLEVALUE: {
      if (auto double_value = value.AsDouble(); double_value) {
        CEL_RETURN_IF_ERROR(well_known_types->DoubleValue().Initialize(
            message->GetDescriptor()));
        well_known_types->DoubleValue().SetValue(message,
                                                 double_value->NativeValue());
        return absl::OkStatus();
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_INT32VALUE: {
      if (auto int_value = value.AsInt(); int_value) {
        if (int_value->NativeValue() < std::numeric_limits<int32_t>::min() ||
            int_value->NativeValue() > std::numeric_limits<int32_t>::max()) {
          return absl::OutOfRangeError("int64 to int32_t overflow");
        }
        CEL_RETURN_IF_ERROR(well_known_types->Int32Value().Initialize(
            message->GetDescriptor()));
        well_known_types->Int32Value().SetValue(
            message, static_cast<int32_t>(int_value->NativeValue()));
        return absl::OkStatus();
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_INT64VALUE: {
      if (auto int_value = value.AsInt(); int_value) {
        CEL_RETURN_IF_ERROR(well_known_types->Int64Value().Initialize(
            message->GetDescriptor()));
        well_known_types->Int64Value().SetValue(message,
                                                int_value->NativeValue());
        return absl::OkStatus();
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT32VALUE: {
      if (auto uint_value = value.AsUint(); uint_value) {
        if (uint_value->NativeValue() > std::numeric_limits<uint32_t>::max()) {
          return absl::OutOfRangeError("uint64 to uint32_t overflow");
        }
        CEL_RETURN_IF_ERROR(well_known_types->UInt32Value().Initialize(
            message->GetDescriptor()));
        well_known_types->UInt32Value().SetValue(
            message, static_cast<uint32_t>(uint_value->NativeValue()));
        return absl::OkStatus();
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT64VALUE: {
      if (auto uint_value = value.AsUint(); uint_value) {
        CEL_RETURN_IF_ERROR(well_known_types->UInt64Value().Initialize(
            message->GetDescriptor()));
        well_known_types->UInt64Value().SetValue(message,
                                                 uint_value->NativeValue());
        return absl::OkStatus();
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_STRINGVALUE: {
      if (auto string_value = value.AsString(); string_value) {
        CEL_RETURN_IF_ERROR(well_known_types->StringValue().Initialize(
            message->GetDescriptor()));
        well_known_types->StringValue().SetValue(message,
                                                 string_value->NativeCord());
        return absl::OkStatus();
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_BYTESVALUE: {
      if (auto bytes_value = value.AsBytes(); bytes_value) {
        CEL_RETURN_IF_ERROR(well_known_types->BytesValue().Initialize(
            message->GetDescriptor()));
        well_known_types->BytesValue().SetValue(message,
                                                bytes_value->NativeCord());
        return absl::OkStatus();
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_BOOLVALUE: {
      if (auto bool_value = value.AsBool(); bool_value) {
        CEL_RETURN_IF_ERROR(
            well_known_types->BoolValue().Initialize(message->GetDescriptor()));
        well_known_types->BoolValue().SetValue(message,
                                               bool_value->NativeValue());
        return absl::OkStatus();
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_ANY: {
      CompatValueManager converter(message->GetArena(), pool, factory);
      absl::Cord serialized;
      CEL_RETURN_IF_ERROR(value.SerializeTo(converter, serialized));
      std::string type_url;
      switch (value.kind()) {
        case ValueKind::kNull:
          type_url = MakeTypeUrl("google.protobuf.Value");
          break;
        case ValueKind::kBool:
          type_url = MakeTypeUrl("google.protobuf.BoolValue");
          break;
        case ValueKind::kInt:
          type_url = MakeTypeUrl("google.protobuf.Int64Value");
          break;
        case ValueKind::kUint:
          type_url = MakeTypeUrl("google.protobuf.UInt64Value");
          break;
        case ValueKind::kDouble:
          type_url = MakeTypeUrl("google.protobuf.DoubleValue");
          break;
        case ValueKind::kBytes:
          type_url = MakeTypeUrl("google.protobuf.BytesValue");
          break;
        case ValueKind::kString:
          type_url = MakeTypeUrl("google.protobuf.StringValue");
          break;
        case ValueKind::kList:
          type_url = MakeTypeUrl("google.protobuf.ListValue");
          break;
        case ValueKind::kMap:
          type_url = MakeTypeUrl("google.protobuf.Struct");
          break;
        case ValueKind::kDuration:
          type_url = MakeTypeUrl("google.protobuf.Duration");
          break;
        case ValueKind::kTimestamp:
          type_url = MakeTypeUrl("google.protobuf.Timestamp");
          break;
        default:
          type_url = MakeTypeUrl(value.GetTypeName());
          break;
      }
      CEL_RETURN_IF_ERROR(
          well_known_types->Any().Initialize(message->GetDescriptor()));
      well_known_types->Any().SetTypeUrl(message, type_url);
      well_known_types->Any().SetValue(message, serialized);
      return absl::OkStatus();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_DURATION: {
      if (auto duration_value = value.AsDuration(); duration_value) {
        CEL_RETURN_IF_ERROR(
            well_known_types->Duration().Initialize(message->GetDescriptor()));
        return well_known_types->Duration().SetFromAbslDuration(
            message, duration_value->NativeValue());
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_TIMESTAMP: {
      if (auto timestamp_value = value.AsTimestamp(); timestamp_value) {
        CEL_RETURN_IF_ERROR(
            well_known_types->Timestamp().Initialize(message->GetDescriptor()));
        return well_known_types->Timestamp().SetFromAbslTime(
            message, timestamp_value->NativeValue());
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE: {
      CompatValueManager converter(message->GetArena(), pool, factory);
      CEL_ASSIGN_OR_RETURN(auto json, value.ConvertToJson(converter));
      return internal::NativeJsonToProtoJson(json, message);
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE: {
      CompatValueManager converter(message->GetArena(), pool, factory);
      CEL_ASSIGN_OR_RETURN(auto json, value.ConvertToJson(converter));
      if (absl::holds_alternative<JsonArray>(json)) {
        return internal::NativeJsonListToProtoJsonList(
            absl::get<JsonArray>(json), message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT: {
      CompatValueManager converter(message->GetArena(), pool, factory);
      CEL_ASSIGN_OR_RETURN(auto json, value.ConvertToJson(converter));
      if (absl::holds_alternative<JsonObject>(json)) {
        return internal::NativeJsonMapToProtoJsonMap(
            absl::get<JsonObject>(json), message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    default:
      break;
  }

  // Not a well known type.

  // Deal with legacy values.
  if (auto legacy_value = common_internal::AsLegacyStructValue(value);
      legacy_value) {
    const auto* from_message = reinterpret_cast<const google::protobuf::Message*>(
        legacy_value->message_ptr() & base_internal::kMessageWrapperPtrMask);
    return ProtoMessageCopy(message, to_desc, from_message);
  }

  // Deal with modern values.
  if (auto parsed_message_value = value.AsParsedMessage();
      parsed_message_value) {
    return ProtoMessageCopy(message, to_desc,
                            cel::to_address(*parsed_message_value));
  }

  return TypeConversionError(value.GetTypeName(), message->GetTypeName())
      .NativeValue();
}

// Converts a value to a specific protocol buffer map key.
using ProtoMapKeyFromValueConverter = absl::Status (*)(const Value&,
                                                       google::protobuf::MapKey&,
                                                       std::string&);

absl::Status ProtoBoolMapKeyFromValueConverter(const Value& value,
                                               google::protobuf::MapKey& key,
                                               std::string&) {
  if (auto bool_value = value.AsBool(); bool_value) {
    key.SetBoolValue(bool_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "bool").NativeValue();
}

absl::Status ProtoInt32MapKeyFromValueConverter(const Value& value,
                                                google::protobuf::MapKey& key,
                                                std::string&) {
  if (auto int_value = value.AsInt(); int_value) {
    if (int_value->NativeValue() < std::numeric_limits<int32_t>::min() ||
        int_value->NativeValue() > std::numeric_limits<int32_t>::max()) {
      return absl::OutOfRangeError("int64 to int32_t overflow");
    }
    key.SetInt32Value(static_cast<int32_t>(int_value->NativeValue()));
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "int").NativeValue();
}

absl::Status ProtoInt64MapKeyFromValueConverter(const Value& value,
                                                google::protobuf::MapKey& key,
                                                std::string&) {
  if (auto int_value = value.AsInt(); int_value) {
    key.SetInt64Value(int_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "int").NativeValue();
}

absl::Status ProtoUInt32MapKeyFromValueConverter(const Value& value,
                                                 google::protobuf::MapKey& key,
                                                 std::string&) {
  if (auto uint_value = value.AsUint(); uint_value) {
    if (uint_value->NativeValue() > std::numeric_limits<uint32_t>::max()) {
      return absl::OutOfRangeError("uint64 to uint32_t overflow");
    }
    key.SetUInt32Value(static_cast<uint32_t>(uint_value->NativeValue()));
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "uint").NativeValue();
}

absl::Status ProtoUInt64MapKeyFromValueConverter(const Value& value,
                                                 google::protobuf::MapKey& key,
                                                 std::string&) {
  if (auto uint_value = value.AsUint(); uint_value) {
    key.SetUInt64Value(uint_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "uint").NativeValue();
}

absl::Status ProtoStringMapKeyFromValueConverter(const Value& value,
                                                 google::protobuf::MapKey& key,
                                                 std::string& key_string) {
  if (auto string_value = value.AsString(); string_value) {
    key_string = string_value->NativeString();
    key.SetStringValue(key_string);
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "string").NativeValue();
}

// Gets the converter for converting from values to protocol buffer map key.
absl::StatusOr<ProtoMapKeyFromValueConverter> GetProtoMapKeyFromValueConverter(
    google::protobuf::FieldDescriptor::CppType cpp_type) {
  switch (cpp_type) {
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return ProtoBoolMapKeyFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      return ProtoInt32MapKeyFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      return ProtoInt64MapKeyFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      return ProtoUInt32MapKeyFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return ProtoUInt64MapKeyFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
      return ProtoStringMapKeyFromValueConverter;
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("unexpected protocol buffer map key type: ",
                       google::protobuf::FieldDescriptor::CppTypeName(cpp_type)));
  }
}

// Converts a value to a specific protocol buffer map value.
using ProtoMapValueFromValueConverter = absl::Status (*)(
    const Value&, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<well_known_types::Reflection*>, google::protobuf::MapValueRef&);

absl::Status ProtoBoolMapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<well_known_types::Reflection*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto bool_value = value.AsBool(); bool_value) {
    value_ref.SetBoolValue(bool_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "bool").NativeValue();
}

absl::Status ProtoInt32MapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<well_known_types::Reflection*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto int_value = value.AsInt(); int_value) {
    if (int_value->NativeValue() < std::numeric_limits<int32_t>::min() ||
        int_value->NativeValue() > std::numeric_limits<int32_t>::max()) {
      return absl::OutOfRangeError("int64 to int32_t overflow");
    }
    value_ref.SetInt32Value(static_cast<int32_t>(int_value->NativeValue()));
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "int").NativeValue();
}

absl::Status ProtoInt64MapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<well_known_types::Reflection*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto int_value = value.AsInt(); int_value) {
    value_ref.SetInt64Value(int_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "int").NativeValue();
}

absl::Status ProtoUInt32MapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<well_known_types::Reflection*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto uint_value = value.AsUint(); uint_value) {
    if (uint_value->NativeValue() > std::numeric_limits<uint32_t>::max()) {
      return absl::OutOfRangeError("uint64 to uint32_t overflow");
    }
    value_ref.SetUInt32Value(static_cast<uint32_t>(uint_value->NativeValue()));
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "uint").NativeValue();
}

absl::Status ProtoUInt64MapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<well_known_types::Reflection*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto uint_value = value.AsUint(); uint_value) {
    value_ref.SetUInt64Value(uint_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "uint").NativeValue();
}

absl::Status ProtoFloatMapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<well_known_types::Reflection*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto double_value = value.AsDouble(); double_value) {
    value_ref.SetFloatValue(double_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "double").NativeValue();
}

absl::Status ProtoDoubleMapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<well_known_types::Reflection*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto double_value = value.AsDouble(); double_value) {
    value_ref.SetDoubleValue(double_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "double").NativeValue();
}

absl::Status ProtoBytesMapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<well_known_types::Reflection*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto bytes_value = value.AsBytes(); bytes_value) {
    value_ref.SetStringValue(bytes_value->NativeString());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "bytes").NativeValue();
}

absl::Status ProtoStringMapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<well_known_types::Reflection*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto string_value = value.AsString(); string_value) {
    value_ref.SetStringValue(string_value->NativeString());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "string").NativeValue();
}

absl::Status ProtoNullMapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<well_known_types::Reflection*>,
    google::protobuf::MapValueRef& value_ref) {
  if (value.IsNull() || value.IsInt()) {
    value_ref.SetEnumValue(0);
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "google.protobuf.NullValue")
      .NativeValue();
}

absl::Status ProtoEnumMapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<well_known_types::Reflection*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto int_value = value.AsInt(); int_value) {
    if (int_value->NativeValue() < std::numeric_limits<int32_t>::min() ||
        int_value->NativeValue() > std::numeric_limits<int32_t>::max()) {
      return absl::OutOfRangeError("int64 to int32_t overflow");
    }
    value_ref.SetEnumValue(static_cast<int32_t>(int_value->NativeValue()));
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "enum").NativeValue();
}

absl::Status ProtoMessageMapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    absl::Nonnull<const google::protobuf::DescriptorPool*> pool,
    absl::Nonnull<google::protobuf::MessageFactory*> factory,
    absl::Nonnull<well_known_types::Reflection*> well_known_types,
    google::protobuf::MapValueRef& value_ref) {
  return ProtoMessageFromValueImpl(value, pool, factory, well_known_types,
                                   value_ref.MutableMessageValue());
}

// Gets the converter for converting from values to protocol buffer map value.
absl::StatusOr<ProtoMapValueFromValueConverter>
GetProtoMapValueFromValueConverter(
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field) {
  ABSL_DCHECK(field->is_map());
  const auto* value_field = field->message_type()->map_value();
  switch (value_field->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return ProtoBoolMapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      return ProtoInt32MapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      return ProtoInt64MapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      return ProtoUInt32MapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return ProtoUInt64MapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
      return ProtoFloatMapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
      return ProtoDoubleMapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
      if (value_field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES) {
        return ProtoBytesMapValueFromValueConverter;
      }
      return ProtoStringMapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
      if (value_field->enum_type()->full_name() ==
          "google.protobuf.NullValue") {
        return ProtoNullMapValueFromValueConverter;
      }
      return ProtoEnumMapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
      return ProtoMessageMapValueFromValueConverter;
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "unexpected protocol buffer map value type: ",
          google::protobuf::FieldDescriptor::CppTypeName(value_field->cpp_type())));
  }
}

using ProtoRepeatedFieldFromValueMutator = absl::Status (*)(
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<well_known_types::Reflection*>,
    absl::Nonnull<const google::protobuf::Reflection*>, absl::Nonnull<google::protobuf::Message*>,
    absl::Nonnull<const google::protobuf::FieldDescriptor*>, const Value&);

absl::Status ProtoBoolRepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<well_known_types::Reflection*>,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, const Value& value) {
  if (auto bool_value = value.AsBool(); bool_value) {
    reflection->AddBool(message, field, bool_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "bool").NativeValue();
}

absl::Status ProtoInt32RepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<well_known_types::Reflection*>,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, const Value& value) {
  if (auto int_value = value.AsInt(); int_value) {
    if (int_value->NativeValue() < std::numeric_limits<int32_t>::min() ||
        int_value->NativeValue() > std::numeric_limits<int32_t>::max()) {
      return absl::OutOfRangeError("int64 to int32_t overflow");
    }
    reflection->AddInt32(message, field,
                         static_cast<int32_t>(int_value->NativeValue()));
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "int").NativeValue();
}

absl::Status ProtoInt64RepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<well_known_types::Reflection*>,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, const Value& value) {
  if (auto int_value = value.AsInt(); int_value) {
    reflection->AddInt64(message, field, int_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "int").NativeValue();
}

absl::Status ProtoUInt32RepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<well_known_types::Reflection*>,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, const Value& value) {
  if (auto uint_value = value.AsUint(); uint_value) {
    if (uint_value->NativeValue() > std::numeric_limits<uint32_t>::max()) {
      return absl::OutOfRangeError("uint64 to uint32_t overflow");
    }
    reflection->AddUInt32(message, field,
                          static_cast<uint32_t>(uint_value->NativeValue()));
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "uint").NativeValue();
}

absl::Status ProtoUInt64RepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<well_known_types::Reflection*>,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, const Value& value) {
  if (auto uint_value = value.AsUint(); uint_value) {
    reflection->AddUInt64(message, field, uint_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "uint").NativeValue();
}

absl::Status ProtoFloatRepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<well_known_types::Reflection*>,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, const Value& value) {
  if (auto double_value = value.AsDouble(); double_value) {
    reflection->AddFloat(message, field,
                         static_cast<float>(double_value->NativeValue()));
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "double").NativeValue();
}

absl::Status ProtoDoubleRepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<well_known_types::Reflection*>,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, const Value& value) {
  if (auto double_value = value.AsDouble(); double_value) {
    reflection->AddDouble(message, field, double_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "double").NativeValue();
}

absl::Status ProtoBytesRepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<well_known_types::Reflection*>,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, const Value& value) {
  if (auto bytes_value = value.AsBytes(); bytes_value) {
    reflection->AddString(message, field, bytes_value->NativeString());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "bytes").NativeValue();
}

absl::Status ProtoStringRepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<well_known_types::Reflection*>,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, const Value& value) {
  if (auto string_value = value.AsString(); string_value) {
    reflection->AddString(message, field, string_value->NativeString());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "string").NativeValue();
}

absl::Status ProtoNullRepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<well_known_types::Reflection*>,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, const Value& value) {
  if (value.IsNull() || value.IsInt()) {
    reflection->AddEnumValue(message, field, 0);
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "null_type").NativeValue();
}

absl::Status ProtoEnumRepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<well_known_types::Reflection*>,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, const Value& value) {
  const auto* enum_descriptor = field->enum_type();
  if (auto int_value = value.AsInt(); int_value) {
    if (int_value->NativeValue() < std::numeric_limits<int>::min() ||
        int_value->NativeValue() > std::numeric_limits<int>::max()) {
      return TypeConversionError(value.GetTypeName(),
                                 enum_descriptor->full_name())
          .NativeValue();
    }
    reflection->AddEnumValue(message, field,
                             static_cast<int>(int_value->NativeValue()));
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), enum_descriptor->full_name())
      .NativeValue();
}

absl::Status ProtoMessageRepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::DescriptorPool*> pool,
    absl::Nonnull<google::protobuf::MessageFactory*> factory,
    absl::Nonnull<well_known_types::Reflection*> well_known_types,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, const Value& value) {
  auto* element = reflection->AddMessage(message, field, factory);
  auto status = ProtoMessageFromValueImpl(value, pool, factory,
                                          well_known_types, element);
  if (!status.ok()) {
    reflection->RemoveLast(message, field);
  }
  return status;
}

absl::StatusOr<ProtoRepeatedFieldFromValueMutator>
GetProtoRepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field) {
  ABSL_DCHECK(!field->is_map());
  ABSL_DCHECK(field->is_repeated());
  switch (field->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return ProtoBoolRepeatedFieldFromValueMutator;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      return ProtoInt32RepeatedFieldFromValueMutator;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      return ProtoInt64RepeatedFieldFromValueMutator;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      return ProtoUInt32RepeatedFieldFromValueMutator;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return ProtoUInt64RepeatedFieldFromValueMutator;
    case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
      return ProtoFloatRepeatedFieldFromValueMutator;
    case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
      return ProtoDoubleRepeatedFieldFromValueMutator;
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
      if (field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES) {
        return ProtoBytesRepeatedFieldFromValueMutator;
      }
      return ProtoStringRepeatedFieldFromValueMutator;
    case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
      if (field->enum_type()->full_name() == "google.protobuf.NullValue") {
        return ProtoNullRepeatedFieldFromValueMutator;
      }
      return ProtoEnumRepeatedFieldFromValueMutator;
    case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
      return ProtoMessageRepeatedFieldFromValueMutator;
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "unexpected protocol buffer repeated field type: ",
          google::protobuf::FieldDescriptor::CppTypeName(field->cpp_type())));
  }
}

class StructValueBuilderImpl final : public StructValueBuilder {
 public:
  StructValueBuilderImpl(
      absl::Nullable<google::protobuf::Arena*> arena,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Message*> message)
      : arena_(arena),
        descriptor_pool_(descriptor_pool),
        message_factory_(message_factory),
        message_(message),
        descriptor_(message_->GetDescriptor()),
        reflection_(message_->GetReflection()) {}

  ~StructValueBuilderImpl() override {
    if (arena_ == nullptr && message_ != nullptr) {
      delete message_;
    }
  }

  absl::Status SetFieldByName(absl::string_view name, Value value) override {
    const auto* field = descriptor_->FindFieldByName(name);
    if (field == nullptr) {
      field = descriptor_pool_->FindExtensionByPrintableName(descriptor_, name);
      if (field == nullptr) {
        return NoSuchFieldError(name).NativeValue();
      }
    }
    return SetField(field, std::move(value));
  }

  absl::Status SetFieldByNumber(int64_t number, Value value) override {
    if (number < std::numeric_limits<int32_t>::min() ||
        number > std::numeric_limits<int32_t>::max()) {
      return NoSuchFieldError(absl::StrCat(number)).NativeValue();
    }
    const auto* field =
        descriptor_->FindFieldByNumber(static_cast<int>(number));
    if (field == nullptr) {
      return NoSuchFieldError(absl::StrCat(number)).NativeValue();
    }
    return SetField(field, std::move(value));
  }

  absl::StatusOr<StructValue> Build() && override {
    return ParsedMessageValue(
        WrapShared(std::exchange(message_, nullptr), Allocator(arena_)));
  }

 private:
  absl::Status SetMapField(absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
                           Value value) {
    auto map_value = value.AsMap();
    if (!map_value) {
      return TypeConversionError(value.GetTypeName(), "map").NativeValue();
    }
    CEL_ASSIGN_OR_RETURN(auto key_converter,
                         GetProtoMapKeyFromValueConverter(
                             field->message_type()->map_key()->cpp_type()));
    CEL_ASSIGN_OR_RETURN(auto value_converter,
                         GetProtoMapValueFromValueConverter(field));
    reflection_->ClearField(message_, field);
    CompatValueManager value_manager(arena_, descriptor_pool_,
                                     message_factory_);
    const auto* map_value_field = field->message_type()->map_value();
    CEL_RETURN_IF_ERROR(map_value->ForEach(
        value_manager,
        [this, field, key_converter, map_value_field, value_converter](
            const Value& entry_key,
            const Value& entry_value) -> absl::StatusOr<bool> {
          std::string proto_key_string;
          google::protobuf::MapKey proto_key;
          CEL_RETURN_IF_ERROR(
              (*key_converter)(entry_key, proto_key, proto_key_string));
          google::protobuf::MapValueRef proto_value;
          extensions::protobuf_internal::InsertOrLookupMapValue(
              *reflection_, message_, *field, proto_key, &proto_value);
          CEL_RETURN_IF_ERROR((*value_converter)(
              entry_value, map_value_field, descriptor_pool_, message_factory_,
              &well_known_types_, proto_value));
          return true;
        }));
    return absl::OkStatus();
  }

  absl::Status SetRepeatedField(
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, Value value) {
    auto list_value = value.AsList();
    if (!list_value) {
      return TypeConversionError(value.GetTypeName(), "list").NativeValue();
    }
    CEL_ASSIGN_OR_RETURN(auto accessor,
                         GetProtoRepeatedFieldFromValueMutator(field));
    reflection_->ClearField(message_, field);
    CompatValueManager value_manager(arena_, descriptor_pool_,
                                     message_factory_);
    CEL_RETURN_IF_ERROR(list_value->ForEach(
        value_manager,
        [this, field, accessor](const Value& element) -> absl::StatusOr<bool> {
          CEL_RETURN_IF_ERROR((*accessor)(descriptor_pool_, message_factory_,
                                          &well_known_types_, reflection_,
                                          message_, field, element));
          return true;
        }));
    return absl::OkStatus();
  }

  absl::Status SetSingularField(
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, Value value) {
    switch (field->cpp_type()) {
      case google::protobuf::FieldDescriptor::CPPTYPE_BOOL: {
        if (auto bool_value = value.AsBool(); bool_value) {
          reflection_->SetBool(message_, field, bool_value->NativeValue());
          return absl::OkStatus();
        }
        return TypeConversionError(value.GetTypeName(), "bool").NativeValue();
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_INT32: {
        if (auto int_value = value.AsInt(); int_value) {
          if (int_value->NativeValue() < std::numeric_limits<int32_t>::min() ||
              int_value->NativeValue() > std::numeric_limits<int32_t>::max()) {
            return absl::OutOfRangeError("int64 to int32_t overflow");
          }
          reflection_->SetInt32(message_, field,
                                static_cast<int32_t>(int_value->NativeValue()));
          return absl::OkStatus();
        }
        return TypeConversionError(value.GetTypeName(), "int").NativeValue();
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_INT64: {
        if (auto int_value = value.AsInt(); int_value) {
          reflection_->SetInt64(message_, field, int_value->NativeValue());
          return absl::OkStatus();
        }
        return TypeConversionError(value.GetTypeName(), "int").NativeValue();
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_UINT32: {
        if (auto uint_value = value.AsUint(); uint_value) {
          if (uint_value->NativeValue() >
              std::numeric_limits<uint32_t>::max()) {
            return absl::OutOfRangeError("uint64 to uint32_t overflow");
          }
          reflection_->SetUInt32(
              message_, field,
              static_cast<uint32_t>(uint_value->NativeValue()));
          return absl::OkStatus();
        }
        return TypeConversionError(value.GetTypeName(), "uint").NativeValue();
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_UINT64: {
        if (auto uint_value = value.AsUint(); uint_value) {
          reflection_->SetUInt64(message_, field, uint_value->NativeValue());
          return absl::OkStatus();
        }
        return TypeConversionError(value.GetTypeName(), "uint").NativeValue();
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT: {
        if (auto double_value = value.AsDouble(); double_value) {
          reflection_->SetFloat(message_, field, double_value->NativeValue());
          return absl::OkStatus();
        }
        return TypeConversionError(value.GetTypeName(), "double").NativeValue();
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE: {
        if (auto double_value = value.AsDouble(); double_value) {
          reflection_->SetDouble(message_, field, double_value->NativeValue());
          return absl::OkStatus();
        }
        return TypeConversionError(value.GetTypeName(), "double").NativeValue();
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
        if (field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES) {
          if (auto bytes_value = value.AsBytes(); bytes_value) {
            bytes_value->NativeValue(absl::Overload(
                [this, field](absl::string_view string) {
                  reflection_->SetString(message_, field, std::string(string));
                },
                [this, field](const absl::Cord& cord) {
                  reflection_->SetString(message_, field, cord);
                }));
            return absl::OkStatus();
          }
          return TypeConversionError(value.GetTypeName(), "bytes")
              .NativeValue();
        }
        if (auto string_value = value.AsString(); string_value) {
          string_value->NativeValue(absl::Overload(
              [this, field](absl::string_view string) {
                reflection_->SetString(message_, field, std::string(string));
              },
              [this, field](const absl::Cord& cord) {
                reflection_->SetString(message_, field, cord);
              }));
          return absl::OkStatus();
        }
        return TypeConversionError(value.GetTypeName(), "string").NativeValue();
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_ENUM: {
        if (field->enum_type()->full_name() == "google.protobuf.NullValue") {
          if (value.IsNull() || value.IsInt()) {
            reflection_->SetEnumValue(message_, field, 0);
            return absl::OkStatus();
          }
          return TypeConversionError(value.GetTypeName(), "null_type")
              .NativeValue();
        }
        if (auto int_value = value.AsInt(); int_value) {
          if (int_value->NativeValue() >= std::numeric_limits<int32_t>::min() &&
              int_value->NativeValue() <= std::numeric_limits<int32_t>::max()) {
            reflection_->SetEnumValue(
                message_, field, static_cast<int>(int_value->NativeValue()));
            return absl::OkStatus();
          }
        }
        return TypeConversionError(value.GetTypeName(),
                                   field->enum_type()->full_name())
            .NativeValue();
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
        switch (field->message_type()->well_known_type()) {
          case google::protobuf::Descriptor::WELLKNOWNTYPE_BOOLVALUE: {
            if (auto bool_value = value.AsBool(); bool_value) {
              CEL_RETURN_IF_ERROR(well_known_types_.BoolValue().Initialize(
                  field->message_type()));
              well_known_types_.BoolValue().SetValue(
                  reflection_->MutableMessage(message_, field,
                                              message_factory_),
                  bool_value->NativeValue());
              return absl::OkStatus();
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name())
                .NativeValue();
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_INT32VALUE: {
            if (auto int_value = value.AsInt(); int_value) {
              if (int_value->NativeValue() <
                      std::numeric_limits<int32_t>::min() ||
                  int_value->NativeValue() >
                      std::numeric_limits<int32_t>::max()) {
                return absl::OutOfRangeError("int64 to int32_t overflow");
              }
              CEL_RETURN_IF_ERROR(well_known_types_.Int32Value().Initialize(
                  field->message_type()));
              well_known_types_.Int32Value().SetValue(
                  reflection_->MutableMessage(message_, field,
                                              message_factory_),
                  static_cast<int32_t>(int_value->NativeValue()));
              return absl::OkStatus();
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name())
                .NativeValue();
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_INT64VALUE: {
            if (auto int_value = value.AsInt(); int_value) {
              CEL_RETURN_IF_ERROR(well_known_types_.Int64Value().Initialize(
                  field->message_type()));
              well_known_types_.Int64Value().SetValue(
                  reflection_->MutableMessage(message_, field,
                                              message_factory_),
                  int_value->NativeValue());
              return absl::OkStatus();
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name())
                .NativeValue();
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT32VALUE: {
            if (auto uint_value = value.AsUint(); uint_value) {
              if (uint_value->NativeValue() >
                  std::numeric_limits<uint32_t>::max()) {
                return absl::OutOfRangeError("uint64 to uint32_t overflow");
              }
              CEL_RETURN_IF_ERROR(well_known_types_.UInt32Value().Initialize(
                  field->message_type()));
              well_known_types_.UInt32Value().SetValue(
                  reflection_->MutableMessage(message_, field,
                                              message_factory_),
                  static_cast<uint32_t>(uint_value->NativeValue()));
              return absl::OkStatus();
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name())
                .NativeValue();
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT64VALUE: {
            if (auto uint_value = value.AsUint(); uint_value) {
              CEL_RETURN_IF_ERROR(well_known_types_.UInt64Value().Initialize(
                  field->message_type()));
              well_known_types_.UInt64Value().SetValue(
                  reflection_->MutableMessage(message_, field,
                                              message_factory_),
                  uint_value->NativeValue());
              return absl::OkStatus();
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name())
                .NativeValue();
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_FLOATVALUE: {
            if (auto double_value = value.AsDouble(); double_value) {
              CEL_RETURN_IF_ERROR(well_known_types_.FloatValue().Initialize(
                  field->message_type()));
              well_known_types_.FloatValue().SetValue(
                  reflection_->MutableMessage(message_, field,
                                              message_factory_),
                  static_cast<float>(double_value->NativeValue()));
              return absl::OkStatus();
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name())
                .NativeValue();
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_DOUBLEVALUE: {
            if (auto double_value = value.AsDouble(); double_value) {
              CEL_RETURN_IF_ERROR(well_known_types_.DoubleValue().Initialize(
                  field->message_type()));
              well_known_types_.DoubleValue().SetValue(
                  reflection_->MutableMessage(message_, field,
                                              message_factory_),
                  double_value->NativeValue());
              return absl::OkStatus();
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name())
                .NativeValue();
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_BYTESVALUE: {
            if (auto bytes_value = value.AsBytes(); bytes_value) {
              CEL_RETURN_IF_ERROR(well_known_types_.BytesValue().Initialize(
                  field->message_type()));
              well_known_types_.BytesValue().SetValue(
                  reflection_->MutableMessage(message_, field,
                                              message_factory_),
                  bytes_value->NativeCord());
              return absl::OkStatus();
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name())
                .NativeValue();
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_STRINGVALUE: {
            if (auto string_value = value.AsString(); string_value) {
              CEL_RETURN_IF_ERROR(well_known_types_.StringValue().Initialize(
                  field->message_type()));
              well_known_types_.StringValue().SetValue(
                  reflection_->MutableMessage(message_, field,
                                              message_factory_),
                  string_value->NativeCord());
              return absl::OkStatus();
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name())
                .NativeValue();
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_DURATION: {
            if (auto duration_value = value.AsDuration(); duration_value) {
              CEL_RETURN_IF_ERROR(well_known_types_.Duration().Initialize(
                  field->message_type()));
              return well_known_types_.Duration().SetFromAbslDuration(
                  reflection_->MutableMessage(message_, field,
                                              message_factory_),
                  duration_value->NativeValue());
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name())
                .NativeValue();
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_TIMESTAMP: {
            if (auto timestamp_value = value.AsTimestamp(); timestamp_value) {
              CEL_RETURN_IF_ERROR(well_known_types_.Timestamp().Initialize(
                  field->message_type()));
              return well_known_types_.Timestamp().SetFromAbslTime(
                  reflection_->MutableMessage(message_, field,
                                              message_factory_),
                  timestamp_value->NativeValue());
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name())
                .NativeValue();
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE: {
            // Probably not correct, need to use the parent/common one.
            CompatValueManager value_manager(arena_, descriptor_pool_,
                                             message_factory_);
            CEL_ASSIGN_OR_RETURN(auto json, value.ConvertToJson(value_manager));
            return internal::NativeJsonToProtoJson(
                json,
                reflection_->MutableMessage(message_, field, message_factory_));
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE: {
            // Probably not correct, need to use the parent/common one.
            CompatValueManager value_manager(arena_, descriptor_pool_,
                                             message_factory_);
            CEL_ASSIGN_OR_RETURN(auto json, value.ConvertToJson(value_manager));
            if (!absl::holds_alternative<JsonArray>(json)) {
              return TypeConversionError(value.GetTypeName(),
                                         field->message_type()->full_name())
                  .NativeValue();
            }
            return internal::NativeJsonListToProtoJsonList(
                absl::get<JsonArray>(json),
                reflection_->MutableMessage(message_, field, message_factory_));
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT: {
            // Probably not correct, need to use the parent/common one.
            CompatValueManager value_manager(arena_, descriptor_pool_,
                                             message_factory_);
            CEL_ASSIGN_OR_RETURN(auto json, value.ConvertToJson(value_manager));
            if (!absl::holds_alternative<JsonObject>(json)) {
              return TypeConversionError(value.GetTypeName(),
                                         field->message_type()->full_name())
                  .NativeValue();
            }
            return internal::NativeJsonMapToProtoJsonMap(
                absl::get<JsonObject>(json),
                reflection_->MutableMessage(message_, field, message_factory_));
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_ANY: {
            // Probably not correct, need to use the parent/common one.
            CompatValueManager value_manager(arena_, descriptor_pool_,
                                             message_factory_);
            absl::Cord serialized;
            CEL_RETURN_IF_ERROR(value.SerializeTo(value_manager, serialized));
            std::string type_url;
            switch (value.kind()) {
              case ValueKind::kNull:
                type_url = MakeTypeUrl("google.protobuf.Value");
                break;
              case ValueKind::kBool:
                type_url = MakeTypeUrl("google.protobuf.BoolValue");
                break;
              case ValueKind::kInt:
                type_url = MakeTypeUrl("google.protobuf.Int64Value");
                break;
              case ValueKind::kUint:
                type_url = MakeTypeUrl("google.protobuf.UInt64Value");
                break;
              case ValueKind::kDouble:
                type_url = MakeTypeUrl("google.protobuf.DoubleValue");
                break;
              case ValueKind::kBytes:
                type_url = MakeTypeUrl("google.protobuf.BytesValue");
                break;
              case ValueKind::kString:
                type_url = MakeTypeUrl("google.protobuf.StringValue");
                break;
              case ValueKind::kList:
                type_url = MakeTypeUrl("google.protobuf.ListValue");
                break;
              case ValueKind::kMap:
                type_url = MakeTypeUrl("google.protobuf.Struct");
                break;
              case ValueKind::kDuration:
                type_url = MakeTypeUrl("google.protobuf.Duration");
                break;
              case ValueKind::kTimestamp:
                type_url = MakeTypeUrl("google.protobuf.Timestamp");
                break;
              default:
                type_url = MakeTypeUrl(value.GetTypeName());
                break;
            }
            CEL_RETURN_IF_ERROR(
                well_known_types_.Any().Initialize(field->message_type()));
            well_known_types_.Any().SetTypeUrl(
                reflection_->MutableMessage(message_, field, message_factory_),
                type_url);
            well_known_types_.Any().SetValue(
                reflection_->MutableMessage(message_, field, message_factory_),
                serialized);
            return absl::OkStatus();
          }
          default:
            break;
        }
        return ProtoMessageFromValueImpl(
            value, descriptor_pool_, message_factory_, &well_known_types_,
            reflection_->MutableMessage(message_, field, message_factory_));
      }
      default:
        return absl::InternalError(
            absl::StrCat("unexpected protocol buffer message field type: ",
                         field->cpp_type_name()));
    }
  }

  absl::Status SetField(absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
                        Value value) {
    if (field->is_map()) {
      return SetMapField(field, std::move(value));
    }
    if (field->is_repeated()) {
      return SetRepeatedField(field, std::move(value));
    }
    return SetSingularField(field, std::move(value));
  }

  absl::Nullable<google::protobuf::Arena*> const arena_;
  absl::Nonnull<const google::protobuf::DescriptorPool*> const descriptor_pool_;
  absl::Nonnull<google::protobuf::MessageFactory*> const message_factory_;
  absl::Nullable<google::protobuf::Message*> message_;
  absl::Nonnull<const google::protobuf::Descriptor*> const descriptor_;
  absl::Nonnull<const google::protobuf::Reflection*> const reflection_;
  well_known_types::Reflection well_known_types_;
};

}  // namespace

absl::StatusOr<absl::Nonnull<cel::StructValueBuilderPtr>> NewStructValueBuilder(
    Allocator<> allocator,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::string_view name) {
  const auto* descriptor = descriptor_pool->FindMessageTypeByName(name);
  if (descriptor == nullptr) {
    return absl::NotFoundError(
        absl::StrCat("unable to find descriptor for type: ", name));
  }
  const auto* prototype = message_factory->GetPrototype(descriptor);
  if (prototype == nullptr) {
    return absl::NotFoundError(absl::StrCat(
        "unable to get prototype for descriptor: ", descriptor->full_name()));
  }
  return std::make_unique<StructValueBuilderImpl>(
      allocator.arena(), descriptor_pool, message_factory,
      prototype->New(allocator.arena()));
}

}  // namespace cel::common_internal
