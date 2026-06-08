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
#include "common/allocator.h"
#include "common/any.h"
#include "common/memory.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "common/values/value_builder.h"
#include "extensions/protobuf/internal/map_reflection.h"
#include "internal/status_macros.h"
#include "internal/well_known_types.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "google/protobuf/message.h"

// TODO(uncreated-issue/82): Improve test coverage for struct value builder

// TODO(uncreated-issue/76): improve test coverage for JSON/Any

namespace cel::common_internal {

namespace {

absl::StatusOr<const google::protobuf::Descriptor* absl_nonnull> GetDescriptor(
    const google::protobuf::Message& message) {
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InvalidArgumentError(
        absl::StrCat(message.GetTypeName(), " is missing descriptor"));
  }
  return desc;
}

absl::StatusOr<absl::optional<ErrorValue>> ProtoMessageCopyUsingSerialization(
    google::protobuf::MessageLite* to, const google::protobuf::MessageLite* from) {
  ABSL_DCHECK_EQ(to->GetTypeName(), from->GetTypeName());
  absl::Cord serialized;
  if (!from->SerializePartialToString(&serialized)) {
    return absl::UnknownError(
        absl::StrCat("failed to serialize `", from->GetTypeName(), "`"));
  }
  if (!to->ParsePartialFromString(serialized)) {
    return absl::UnknownError(
        absl::StrCat("failed to parse `", to->GetTypeName(), "`"));
  }
  return absl::nullopt;
}

absl::StatusOr<absl::optional<ErrorValue>> ProtoMessageCopy(
    google::protobuf::Message* absl_nonnull to_message,
    const google::protobuf::Descriptor* absl_nonnull to_descriptor,
    const google::protobuf::Message* absl_nonnull from_message) {
  CEL_ASSIGN_OR_RETURN(const auto* from_descriptor,
                       GetDescriptor(*from_message));
  if (to_descriptor == from_descriptor) {
    // Same.
    to_message->CopyFrom(*from_message);
    return absl::nullopt;
  }
  if (to_descriptor->full_name() == from_descriptor->full_name()) {
    // Same type, different descriptors.
    return ProtoMessageCopyUsingSerialization(to_message, from_message);
  }
  return TypeConversionError(from_descriptor->full_name(),
                             to_descriptor->full_name());
}

absl::StatusOr<absl::optional<ErrorValue>> ProtoMessageFromValueImpl(
    const Value& value, const google::protobuf::DescriptorPool* absl_nonnull pool,
    google::protobuf::MessageFactory* absl_nonnull factory,
    well_known_types::Reflection* absl_nonnull well_known_types,
    google::protobuf::Message* absl_nonnull message) {
  CEL_ASSIGN_OR_RETURN(const auto* to_desc, GetDescriptor(*message));
  switch (to_desc->well_known_type()) {
    case google::protobuf::Descriptor::WELLKNOWNTYPE_FLOATVALUE: {
      if (auto double_value = value.AsDouble(); double_value) {
        CEL_RETURN_IF_ERROR(well_known_types->FloatValue().Initialize(
            message->GetDescriptor()));
        well_known_types->FloatValue().SetValue(
            message, static_cast<float>(double_value->NativeValue()));
        return absl::nullopt;
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name());
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_DOUBLEVALUE: {
      if (auto double_value = value.AsDouble(); double_value) {
        CEL_RETURN_IF_ERROR(well_known_types->DoubleValue().Initialize(
            message->GetDescriptor()));
        well_known_types->DoubleValue().SetValue(message,
                                                 double_value->NativeValue());
        return absl::nullopt;
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name());
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_INT32VALUE: {
      if (auto int_value = value.AsInt(); int_value) {
        if (int_value->NativeValue() < std::numeric_limits<int32_t>::min() ||
            int_value->NativeValue() > std::numeric_limits<int32_t>::max()) {
          return ErrorValue(absl::OutOfRangeError("int64 to int32 overflow"));
        }
        CEL_RETURN_IF_ERROR(well_known_types->Int32Value().Initialize(
            message->GetDescriptor()));
        well_known_types->Int32Value().SetValue(
            message, static_cast<int32_t>(int_value->NativeValue()));
        return absl::nullopt;
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name());
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_INT64VALUE: {
      if (auto int_value = value.AsInt(); int_value) {
        CEL_RETURN_IF_ERROR(well_known_types->Int64Value().Initialize(
            message->GetDescriptor()));
        well_known_types->Int64Value().SetValue(message,
                                                int_value->NativeValue());
        return absl::nullopt;
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name());
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT32VALUE: {
      if (auto uint_value = value.AsUint(); uint_value) {
        if (uint_value->NativeValue() > std::numeric_limits<uint32_t>::max()) {
          return ErrorValue(absl::OutOfRangeError("uint64 to uint32 overflow"));
        }
        CEL_RETURN_IF_ERROR(well_known_types->UInt32Value().Initialize(
            message->GetDescriptor()));
        well_known_types->UInt32Value().SetValue(
            message, static_cast<uint32_t>(uint_value->NativeValue()));
        return absl::nullopt;
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name());
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT64VALUE: {
      if (auto uint_value = value.AsUint(); uint_value) {
        CEL_RETURN_IF_ERROR(well_known_types->UInt64Value().Initialize(
            message->GetDescriptor()));
        well_known_types->UInt64Value().SetValue(message,
                                                 uint_value->NativeValue());
        return absl::nullopt;
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name());
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_STRINGVALUE: {
      if (auto string_value = value.AsString(); string_value) {
        CEL_RETURN_IF_ERROR(well_known_types->StringValue().Initialize(
            message->GetDescriptor()));
        well_known_types->StringValue().SetValue(message,
                                                 string_value->NativeCord());
        return absl::nullopt;
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name());
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_BYTESVALUE: {
      if (auto bytes_value = value.AsBytes(); bytes_value) {
        CEL_RETURN_IF_ERROR(well_known_types->BytesValue().Initialize(
            message->GetDescriptor()));
        well_known_types->BytesValue().SetValue(message,
                                                bytes_value->NativeCord());
        return absl::nullopt;
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name());
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_BOOLVALUE: {
      if (auto bool_value = value.AsBool(); bool_value) {
        CEL_RETURN_IF_ERROR(
            well_known_types->BoolValue().Initialize(message->GetDescriptor()));
        well_known_types->BoolValue().SetValue(message,
                                               bool_value->NativeValue());
        return absl::nullopt;
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name());
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_ANY: {
      google::protobuf::io::CordOutputStream serialized;
      CEL_RETURN_IF_ERROR(value.SerializeTo(pool, factory, &serialized));
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
      well_known_types->Any().SetValue(message,
                                       std::move(serialized).Consume());
      return absl::nullopt;
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_DURATION: {
      if (auto duration_value = value.AsDuration(); duration_value) {
        CEL_RETURN_IF_ERROR(
            well_known_types->Duration().Initialize(message->GetDescriptor()));
        CEL_RETURN_IF_ERROR(well_known_types->Duration().SetFromAbslDuration(
            message, duration_value->NativeValue()));
        return absl::nullopt;
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name());
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_TIMESTAMP: {
      if (auto timestamp_value = value.AsTimestamp(); timestamp_value) {
        CEL_RETURN_IF_ERROR(
            well_known_types->Timestamp().Initialize(message->GetDescriptor()));
        CEL_RETURN_IF_ERROR(well_known_types->Timestamp().SetFromAbslTime(
            message, timestamp_value->NativeValue()));
        return absl::nullopt;
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name());
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE: {
      CEL_RETURN_IF_ERROR(value.ConvertToJson(pool, factory, message));
      return absl::nullopt;
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE: {
      CEL_RETURN_IF_ERROR(value.ConvertToJsonArray(pool, factory, message));
      return absl::nullopt;
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT: {
      CEL_RETURN_IF_ERROR(value.ConvertToJsonObject(pool, factory, message));
      return absl::nullopt;
    }
    default:
      break;
  }

  // Not a well known type.

  // Deal with legacy values.
  if (auto legacy_value = common_internal::AsLegacyStructValue(value);
      legacy_value) {
    const auto* from_message = legacy_value->message_ptr();
    return ProtoMessageCopy(message, to_desc, from_message);
  }

  // Deal with modern values.
  if (auto parsed_message_value = value.AsParsedMessage();
      parsed_message_value) {
    return ProtoMessageCopy(message, to_desc,
                            cel::to_address(*parsed_message_value));
  }

  return TypeConversionError(value.GetTypeName(), message->GetTypeName());
}

// Converts a value to a specific protocol buffer map key.
using ProtoMapKeyFromValueConverter =
    absl::StatusOr<absl::optional<ErrorValue>> (*)(const Value&,
                                                   google::protobuf::MapKey&,
                                                   std::string&);

absl::StatusOr<absl::optional<ErrorValue>> ProtoBoolMapKeyFromValueConverter(
    const Value& value, google::protobuf::MapKey& key, std::string&) {
  if (auto bool_value = value.AsBool(); bool_value) {
    key.SetBoolValue(bool_value->NativeValue());
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "bool");
}

absl::StatusOr<absl::optional<ErrorValue>> ProtoInt32MapKeyFromValueConverter(
    const Value& value, google::protobuf::MapKey& key, std::string&) {
  if (auto int_value = value.AsInt(); int_value) {
    if (int_value->NativeValue() < std::numeric_limits<int32_t>::min() ||
        int_value->NativeValue() > std::numeric_limits<int32_t>::max()) {
      return ErrorValue(absl::OutOfRangeError("int64 to int32 overflow"));
    }
    key.SetInt32Value(static_cast<int32_t>(int_value->NativeValue()));
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "int");
}

absl::StatusOr<absl::optional<ErrorValue>> ProtoInt64MapKeyFromValueConverter(
    const Value& value, google::protobuf::MapKey& key, std::string&) {
  if (auto int_value = value.AsInt(); int_value) {
    key.SetInt64Value(int_value->NativeValue());
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "int");
}

absl::StatusOr<absl::optional<ErrorValue>> ProtoUInt32MapKeyFromValueConverter(
    const Value& value, google::protobuf::MapKey& key, std::string&) {
  if (auto uint_value = value.AsUint(); uint_value) {
    if (uint_value->NativeValue() > std::numeric_limits<uint32_t>::max()) {
      return ErrorValue(absl::OutOfRangeError("uint64 to uint32 overflow"));
    }
    key.SetUInt32Value(static_cast<uint32_t>(uint_value->NativeValue()));
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "uint");
}

absl::StatusOr<absl::optional<ErrorValue>> ProtoUInt64MapKeyFromValueConverter(
    const Value& value, google::protobuf::MapKey& key, std::string&) {
  if (auto uint_value = value.AsUint(); uint_value) {
    key.SetUInt64Value(uint_value->NativeValue());
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "uint");
}

absl::StatusOr<absl::optional<ErrorValue>> ProtoStringMapKeyFromValueConverter(
    const Value& value, google::protobuf::MapKey& key, std::string& key_string) {
  if (auto string_value = value.AsString(); string_value) {
    key_string = string_value->NativeString();
    key.SetStringValue(key_string);
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "string");
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
using ProtoMapValueFromValueConverter =
    absl::StatusOr<absl::optional<ErrorValue>> (*)(
        const Value&, const google::protobuf::FieldDescriptor* absl_nonnull,
        const google::protobuf::DescriptorPool* absl_nonnull,
        google::protobuf::MessageFactory* absl_nonnull,
        well_known_types::Reflection* absl_nonnull, google::protobuf::MapValueRef&);

absl::StatusOr<absl::optional<ErrorValue>> ProtoBoolMapValueFromValueConverter(
    const Value& value, const google::protobuf::FieldDescriptor* absl_nonnull,
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull,
    well_known_types::Reflection* absl_nonnull,
    google::protobuf::MapValueRef& value_ref) {
  if (auto bool_value = value.AsBool(); bool_value) {
    value_ref.SetBoolValue(bool_value->NativeValue());
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "bool");
}

absl::StatusOr<absl::optional<ErrorValue>> ProtoInt32MapValueFromValueConverter(
    const Value& value, const google::protobuf::FieldDescriptor* absl_nonnull,
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull,
    well_known_types::Reflection* absl_nonnull,
    google::protobuf::MapValueRef& value_ref) {
  if (auto int_value = value.AsInt(); int_value) {
    if (int_value->NativeValue() < std::numeric_limits<int32_t>::min() ||
        int_value->NativeValue() > std::numeric_limits<int32_t>::max()) {
      return ErrorValue(absl::OutOfRangeError("int64 to int32 overflow"));
    }
    value_ref.SetInt32Value(static_cast<int32_t>(int_value->NativeValue()));
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "int");
}

absl::StatusOr<absl::optional<ErrorValue>> ProtoInt64MapValueFromValueConverter(
    const Value& value, const google::protobuf::FieldDescriptor* absl_nonnull,
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull,
    well_known_types::Reflection* absl_nonnull,
    google::protobuf::MapValueRef& value_ref) {
  if (auto int_value = value.AsInt(); int_value) {
    value_ref.SetInt64Value(int_value->NativeValue());
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "int");
}

absl::StatusOr<absl::optional<ErrorValue>>
ProtoUInt32MapValueFromValueConverter(
    const Value& value, const google::protobuf::FieldDescriptor* absl_nonnull,
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull,
    well_known_types::Reflection* absl_nonnull,
    google::protobuf::MapValueRef& value_ref) {
  if (auto uint_value = value.AsUint(); uint_value) {
    if (uint_value->NativeValue() > std::numeric_limits<uint32_t>::max()) {
      return ErrorValue(absl::OutOfRangeError("uint64 to uint32 overflow"));
    }
    value_ref.SetUInt32Value(static_cast<uint32_t>(uint_value->NativeValue()));
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "uint");
}

absl::StatusOr<absl::optional<ErrorValue>>
ProtoUInt64MapValueFromValueConverter(
    const Value& value, const google::protobuf::FieldDescriptor* absl_nonnull,
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull,
    well_known_types::Reflection* absl_nonnull,
    google::protobuf::MapValueRef& value_ref) {
  if (auto uint_value = value.AsUint(); uint_value) {
    value_ref.SetUInt64Value(uint_value->NativeValue());
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "uint");
}

absl::StatusOr<absl::optional<ErrorValue>> ProtoFloatMapValueFromValueConverter(
    const Value& value, const google::protobuf::FieldDescriptor* absl_nonnull,
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull,
    well_known_types::Reflection* absl_nonnull,
    google::protobuf::MapValueRef& value_ref) {
  if (auto double_value = value.AsDouble(); double_value) {
    value_ref.SetFloatValue(double_value->NativeValue());
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "double");
}

absl::StatusOr<absl::optional<ErrorValue>>
ProtoDoubleMapValueFromValueConverter(
    const Value& value, const google::protobuf::FieldDescriptor* absl_nonnull,
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull,
    well_known_types::Reflection* absl_nonnull,
    google::protobuf::MapValueRef& value_ref) {
  if (auto double_value = value.AsDouble(); double_value) {
    value_ref.SetDoubleValue(double_value->NativeValue());
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "double");
}

absl::StatusOr<absl::optional<ErrorValue>> ProtoBytesMapValueFromValueConverter(
    const Value& value, const google::protobuf::FieldDescriptor* absl_nonnull,
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull,
    well_known_types::Reflection* absl_nonnull,
    google::protobuf::MapValueRef& value_ref) {
  if (auto bytes_value = value.AsBytes(); bytes_value) {
    value_ref.SetStringValue(bytes_value->NativeString());
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "bytes");
}

absl::StatusOr<absl::optional<ErrorValue>>
ProtoStringMapValueFromValueConverter(
    const Value& value, const google::protobuf::FieldDescriptor* absl_nonnull,
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull,
    well_known_types::Reflection* absl_nonnull,
    google::protobuf::MapValueRef& value_ref) {
  if (auto string_value = value.AsString(); string_value) {
    value_ref.SetStringValue(string_value->NativeString());
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "string");
}

absl::StatusOr<absl::optional<ErrorValue>> ProtoNullMapValueFromValueConverter(
    const Value& value, const google::protobuf::FieldDescriptor* absl_nonnull,
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull,
    well_known_types::Reflection* absl_nonnull,
    google::protobuf::MapValueRef& value_ref) {
  if (value.IsNull() || value.IsInt()) {
    value_ref.SetEnumValue(0);
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "google.protobuf.NullValue");
}

absl::StatusOr<absl::optional<ErrorValue>> ProtoEnumMapValueFromValueConverter(
    const Value& value, const google::protobuf::FieldDescriptor* absl_nonnull field,
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull,
    well_known_types::Reflection* absl_nonnull,
    google::protobuf::MapValueRef& value_ref) {
  if (auto int_value = value.AsInt(); int_value) {
    if (int_value->NativeValue() < std::numeric_limits<int32_t>::min() ||
        int_value->NativeValue() > std::numeric_limits<int32_t>::max()) {
      return ErrorValue(absl::OutOfRangeError("int64 to int32 overflow"));
    }
    value_ref.SetEnumValue(static_cast<int32_t>(int_value->NativeValue()));
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "enum");
}

absl::StatusOr<absl::optional<ErrorValue>>
ProtoMessageMapValueFromValueConverter(
    const Value& value, const google::protobuf::FieldDescriptor* absl_nonnull,
    const google::protobuf::DescriptorPool* absl_nonnull pool,
    google::protobuf::MessageFactory* absl_nonnull factory,
    well_known_types::Reflection* absl_nonnull well_known_types,
    google::protobuf::MapValueRef& value_ref) {
  return ProtoMessageFromValueImpl(value, pool, factory, well_known_types,
                                   value_ref.MutableMessageValue());
}

// Gets the converter for converting from values to protocol buffer map value.
absl::StatusOr<ProtoMapValueFromValueConverter>
GetProtoMapValueFromValueConverter(
    const google::protobuf::FieldDescriptor* absl_nonnull field) {
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

using ProtoRepeatedFieldFromValueMutator =
    absl::StatusOr<absl::optional<ErrorValue>> (*)(
        const google::protobuf::DescriptorPool* absl_nonnull,
        google::protobuf::MessageFactory* absl_nonnull,
        well_known_types::Reflection* absl_nonnull,
        const google::protobuf::Reflection* absl_nonnull, google::protobuf::Message* absl_nonnull,
        const google::protobuf::FieldDescriptor* absl_nonnull, const Value&);

absl::StatusOr<absl::optional<ErrorValue>>
ProtoBoolRepeatedFieldFromValueMutator(
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull,
    well_known_types::Reflection* absl_nonnull,
    const google::protobuf::Reflection* absl_nonnull reflection,
    google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field, const Value& value) {
  if (auto bool_value = value.AsBool(); bool_value) {
    reflection->AddBool(message, field, bool_value->NativeValue());
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "bool");
}

absl::StatusOr<absl::optional<ErrorValue>>
ProtoInt32RepeatedFieldFromValueMutator(
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull,
    well_known_types::Reflection* absl_nonnull,
    const google::protobuf::Reflection* absl_nonnull reflection,
    google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field, const Value& value) {
  if (auto int_value = value.AsInt(); int_value) {
    if (int_value->NativeValue() < std::numeric_limits<int32_t>::min() ||
        int_value->NativeValue() > std::numeric_limits<int32_t>::max()) {
      return ErrorValue(absl::OutOfRangeError("int64 to int32 overflow"));
    }
    reflection->AddInt32(message, field,
                         static_cast<int32_t>(int_value->NativeValue()));
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "int");
}

absl::StatusOr<absl::optional<ErrorValue>>
ProtoInt64RepeatedFieldFromValueMutator(
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull,
    well_known_types::Reflection* absl_nonnull,
    const google::protobuf::Reflection* absl_nonnull reflection,
    google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field, const Value& value) {
  if (auto int_value = value.AsInt(); int_value) {
    reflection->AddInt64(message, field, int_value->NativeValue());
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "int");
}

absl::StatusOr<absl::optional<ErrorValue>>
ProtoUInt32RepeatedFieldFromValueMutator(
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull,
    well_known_types::Reflection* absl_nonnull,
    const google::protobuf::Reflection* absl_nonnull reflection,
    google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field, const Value& value) {
  if (auto uint_value = value.AsUint(); uint_value) {
    if (uint_value->NativeValue() > std::numeric_limits<uint32_t>::max()) {
      return ErrorValue(absl::OutOfRangeError("uint64 to uint32 overflow"));
    }
    reflection->AddUInt32(message, field,
                          static_cast<uint32_t>(uint_value->NativeValue()));
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "uint");
}

absl::StatusOr<absl::optional<ErrorValue>>
ProtoUInt64RepeatedFieldFromValueMutator(
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull,
    well_known_types::Reflection* absl_nonnull,
    const google::protobuf::Reflection* absl_nonnull reflection,
    google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field, const Value& value) {
  if (auto uint_value = value.AsUint(); uint_value) {
    reflection->AddUInt64(message, field, uint_value->NativeValue());
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "uint");
}

absl::StatusOr<absl::optional<ErrorValue>>
ProtoFloatRepeatedFieldFromValueMutator(
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull,
    well_known_types::Reflection* absl_nonnull,
    const google::protobuf::Reflection* absl_nonnull reflection,
    google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field, const Value& value) {
  if (auto double_value = value.AsDouble(); double_value) {
    reflection->AddFloat(message, field,
                         static_cast<float>(double_value->NativeValue()));
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "double");
}

absl::StatusOr<absl::optional<ErrorValue>>
ProtoDoubleRepeatedFieldFromValueMutator(
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull,
    well_known_types::Reflection* absl_nonnull,
    const google::protobuf::Reflection* absl_nonnull reflection,
    google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field, const Value& value) {
  if (auto double_value = value.AsDouble(); double_value) {
    reflection->AddDouble(message, field, double_value->NativeValue());
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "double");
}

absl::StatusOr<absl::optional<ErrorValue>>
ProtoBytesRepeatedFieldFromValueMutator(
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull,
    well_known_types::Reflection* absl_nonnull,
    const google::protobuf::Reflection* absl_nonnull reflection,
    google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field, const Value& value) {
  if (auto bytes_value = value.AsBytes(); bytes_value) {
    reflection->AddString(message, field, bytes_value->NativeString());
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "bytes");
}

absl::StatusOr<absl::optional<ErrorValue>>
ProtoStringRepeatedFieldFromValueMutator(
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull,
    well_known_types::Reflection* absl_nonnull,
    const google::protobuf::Reflection* absl_nonnull reflection,
    google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field, const Value& value) {
  if (auto string_value = value.AsString(); string_value) {
    reflection->AddString(message, field, string_value->NativeString());
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "string");
}

absl::StatusOr<absl::optional<ErrorValue>>
ProtoNullRepeatedFieldFromValueMutator(
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull,
    well_known_types::Reflection* absl_nonnull,
    const google::protobuf::Reflection* absl_nonnull reflection,
    google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field, const Value& value) {
  if (value.IsNull() || value.IsInt()) {
    reflection->AddEnumValue(message, field, 0);
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), "null_type");
}

absl::StatusOr<absl::optional<ErrorValue>>
ProtoEnumRepeatedFieldFromValueMutator(
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull,
    well_known_types::Reflection* absl_nonnull,
    const google::protobuf::Reflection* absl_nonnull reflection,
    google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field, const Value& value) {
  const auto* enum_descriptor = field->enum_type();
  if (auto int_value = value.AsInt(); int_value) {
    if (int_value->NativeValue() < std::numeric_limits<int>::min() ||
        int_value->NativeValue() > std::numeric_limits<int>::max()) {
      return TypeConversionError(value.GetTypeName(),
                                 enum_descriptor->full_name());
    }
    reflection->AddEnumValue(message, field,
                             static_cast<int>(int_value->NativeValue()));
    return absl::nullopt;
  }
  return TypeConversionError(value.GetTypeName(), enum_descriptor->full_name());
}

absl::StatusOr<absl::optional<ErrorValue>>
ProtoMessageRepeatedFieldFromValueMutator(
    const google::protobuf::DescriptorPool* absl_nonnull pool,
    google::protobuf::MessageFactory* absl_nonnull factory,
    well_known_types::Reflection* absl_nonnull well_known_types,
    const google::protobuf::Reflection* absl_nonnull reflection,
    google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field, const Value& value) {
  auto* element = reflection->AddMessage(message, field, factory);
  auto result = ProtoMessageFromValueImpl(value, pool, factory,
                                          well_known_types, element);
  if (!result.ok() || result->has_value()) {
    reflection->RemoveLast(message, field);
  }
  return result;
}

absl::StatusOr<ProtoRepeatedFieldFromValueMutator>
GetProtoRepeatedFieldFromValueMutator(
    const google::protobuf::FieldDescriptor* absl_nonnull field) {
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

class MessageValueBuilderImpl {
 public:
  MessageValueBuilderImpl(
      google::protobuf::Arena* absl_nullable arena,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull message)
      : arena_(arena),
        descriptor_pool_(descriptor_pool),
        message_factory_(message_factory),
        message_(message),
        descriptor_(message_->GetDescriptor()),
        reflection_(message_->GetReflection()) {}

  ~MessageValueBuilderImpl() {
    if (arena_ == nullptr && message_ != nullptr) {
      delete message_;
    }
  }

  absl::StatusOr<absl::optional<ErrorValue>> SetFieldByName(
      absl::string_view name, Value value) {
    const auto* field = descriptor_->FindFieldByName(name);
    if (field == nullptr) {
      field = descriptor_pool_->FindExtensionByPrintableName(descriptor_, name);
      if (field == nullptr) {
        return NoSuchFieldError(name);
      }
    }
    return SetField(field, std::move(value));
  }

  absl::StatusOr<absl::optional<ErrorValue>> SetFieldByNumber(int64_t number,
                                                              Value value) {
    if (number < std::numeric_limits<int32_t>::min() ||
        number > std::numeric_limits<int32_t>::max()) {
      return NoSuchFieldError(absl::StrCat(number));
    }
    const auto* field =
        descriptor_->FindFieldByNumber(static_cast<int>(number));
    if (field == nullptr) {
      return NoSuchFieldError(absl::StrCat(number));
    }
    return SetField(field, std::move(value));
  }

  absl::StatusOr<Value> Build() && {
    return Value::WrapMessage(std::exchange(message_, nullptr),
                              descriptor_pool_, message_factory_, arena_);
  }

  absl::StatusOr<StructValue> BuildStruct() && {
    return ParsedMessageValue(std::exchange(message_, nullptr), arena_);
  }

 private:
  absl::StatusOr<absl::optional<ErrorValue>> SetMapField(
      const google::protobuf::FieldDescriptor* absl_nonnull field, Value value) {
    auto map_value = value.AsMap();
    if (!map_value) {
      return TypeConversionError(value.GetTypeName(), "map");
    }
    CEL_ASSIGN_OR_RETURN(auto key_converter,
                         GetProtoMapKeyFromValueConverter(
                             field->message_type()->map_key()->cpp_type()));
    CEL_ASSIGN_OR_RETURN(auto value_converter,
                         GetProtoMapValueFromValueConverter(field));
    reflection_->ClearField(message_, field);
    const auto* map_value_field = field->message_type()->map_value();
    absl::optional<ErrorValue> error_value;
    // Don't replace this pattern with a status macro; nested macro invocations
    // have the same __LINE__ on MSVC, causing CEL_ASSIGN_OR_RETURN invocations
    // to conflict with each-other.
    auto status = map_value->ForEach(
        [this, field, key_converter, map_value_field, value_converter,
         &error_value](const Value& entry_key,
                       const Value& entry_value) -> absl::StatusOr<bool> {
          std::string proto_key_string;
          google::protobuf::MapKey proto_key;
          CEL_ASSIGN_OR_RETURN(
              error_value,
              (*key_converter)(entry_key, proto_key, proto_key_string));
          if (error_value) {
            return false;
          }
          google::protobuf::MapValueRef proto_value;
          extensions::protobuf_internal::InsertOrLookupMapValue(
              *reflection_, message_, *field, proto_key, &proto_value);
          CEL_ASSIGN_OR_RETURN(
              error_value,
              (*value_converter)(entry_value, map_value_field, descriptor_pool_,
                                 message_factory_, &well_known_types_,
                                 proto_value));
          if (error_value) {
            return false;
          }
          return true;
        },
        descriptor_pool_, message_factory_, arena_);
    if (!status.ok()) {
      return status;
    }
    return error_value;
  }

  absl::StatusOr<absl::optional<ErrorValue>> SetRepeatedField(
      const google::protobuf::FieldDescriptor* absl_nonnull field, Value value) {
    auto list_value = value.AsList();
    if (!list_value) {
      return TypeConversionError(value.GetTypeName(), "list").NativeValue();
    }
    CEL_ASSIGN_OR_RETURN(auto accessor,
                         GetProtoRepeatedFieldFromValueMutator(field));
    reflection_->ClearField(message_, field);
    absl::optional<ErrorValue> error_value;
    CEL_RETURN_IF_ERROR(list_value->ForEach(
        [this, field, accessor,
         &error_value](const Value& element) -> absl::StatusOr<bool> {
          CEL_ASSIGN_OR_RETURN(error_value,
                               (*accessor)(descriptor_pool_, message_factory_,
                                           &well_known_types_, reflection_,
                                           message_, field, element));
          return !error_value;
        },
        descriptor_pool_, message_factory_, arena_));
    return error_value;
  }

  absl::StatusOr<absl::optional<ErrorValue>> SetSingularField(
      const google::protobuf::FieldDescriptor* absl_nonnull field, Value value) {
    switch (field->cpp_type()) {
      case google::protobuf::FieldDescriptor::CPPTYPE_BOOL: {
        if (auto bool_value = value.AsBool(); bool_value) {
          reflection_->SetBool(message_, field, bool_value->NativeValue());
          return absl::nullopt;
        }
        return TypeConversionError(value.GetTypeName(), "bool");
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_INT32: {
        if (auto int_value = value.AsInt(); int_value) {
          if (int_value->NativeValue() < std::numeric_limits<int32_t>::min() ||
              int_value->NativeValue() > std::numeric_limits<int32_t>::max()) {
            return ErrorValue(absl::OutOfRangeError("int64 to int32 overflow"));
          }
          reflection_->SetInt32(message_, field,
                                static_cast<int32_t>(int_value->NativeValue()));
          return absl::nullopt;
        }
        return TypeConversionError(value.GetTypeName(), "int");
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_INT64: {
        if (auto int_value = value.AsInt(); int_value) {
          reflection_->SetInt64(message_, field, int_value->NativeValue());
          return absl::nullopt;
        }
        return TypeConversionError(value.GetTypeName(), "int");
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_UINT32: {
        if (auto uint_value = value.AsUint(); uint_value) {
          if (uint_value->NativeValue() >
              std::numeric_limits<uint32_t>::max()) {
            return ErrorValue(
                absl::OutOfRangeError("uint64 to uint32 overflow"));
          }
          reflection_->SetUInt32(
              message_, field,
              static_cast<uint32_t>(uint_value->NativeValue()));
          return absl::nullopt;
        }
        return TypeConversionError(value.GetTypeName(), "uint");
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_UINT64: {
        if (auto uint_value = value.AsUint(); uint_value) {
          reflection_->SetUInt64(message_, field, uint_value->NativeValue());
          return absl::nullopt;
        }
        return TypeConversionError(value.GetTypeName(), "uint");
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT: {
        if (auto double_value = value.AsDouble(); double_value) {
          reflection_->SetFloat(message_, field, double_value->NativeValue());
          return absl::nullopt;
        }
        return TypeConversionError(value.GetTypeName(), "double");
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE: {
        if (auto double_value = value.AsDouble(); double_value) {
          reflection_->SetDouble(message_, field, double_value->NativeValue());
          return absl::nullopt;
        }
        return TypeConversionError(value.GetTypeName(), "double");
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
            return absl::nullopt;
          }
          return TypeConversionError(value.GetTypeName(), "bytes");
        }
        if (auto string_value = value.AsString(); string_value) {
          string_value->NativeValue(absl::Overload(
              [this, field](absl::string_view string) {
                reflection_->SetString(message_, field, std::string(string));
              },
              [this, field](const absl::Cord& cord) {
                reflection_->SetString(message_, field, cord);
              }));
          return absl::nullopt;
        }
        return TypeConversionError(value.GetTypeName(), "string");
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_ENUM: {
        if (field->enum_type()->full_name() == "google.protobuf.NullValue") {
          if (value.IsNull() || value.IsInt()) {
            reflection_->SetEnumValue(message_, field, 0);
            return absl::nullopt;
          }
          return TypeConversionError(value.GetTypeName(), "null_type");
        }
        if (auto int_value = value.AsInt(); int_value) {
          if (int_value->NativeValue() >= std::numeric_limits<int32_t>::min() &&
              int_value->NativeValue() <= std::numeric_limits<int32_t>::max()) {
            reflection_->SetEnumValue(
                message_, field, static_cast<int>(int_value->NativeValue()));
            return absl::nullopt;
          }
        }
        return TypeConversionError(value.GetTypeName(),
                                   field->enum_type()->full_name());
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
        switch (field->message_type()->well_known_type()) {
          case google::protobuf::Descriptor::WELLKNOWNTYPE_BOOLVALUE: {
            if (value.IsNull()) {
              // Allowing assigning `null` to message fields.
              return absl::nullopt;
            }
            if (auto bool_value = value.AsBool(); bool_value) {
              CEL_RETURN_IF_ERROR(well_known_types_.BoolValue().Initialize(
                  field->message_type()));
              well_known_types_.BoolValue().SetValue(
                  reflection_->MutableMessage(message_, field,
                                              message_factory_),
                  bool_value->NativeValue());
              return absl::nullopt;
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name());
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_INT32VALUE: {
            if (value.IsNull()) {
              // Allowing assigning `null` to message fields.
              return absl::nullopt;
            }
            if (auto int_value = value.AsInt(); int_value) {
              if (int_value->NativeValue() <
                      std::numeric_limits<int32_t>::min() ||
                  int_value->NativeValue() >
                      std::numeric_limits<int32_t>::max()) {
                return absl::OutOfRangeError("int64 to int32 overflow");
              }
              CEL_RETURN_IF_ERROR(well_known_types_.Int32Value().Initialize(
                  field->message_type()));
              well_known_types_.Int32Value().SetValue(
                  reflection_->MutableMessage(message_, field,
                                              message_factory_),
                  static_cast<int32_t>(int_value->NativeValue()));
              return absl::nullopt;
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name());
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_INT64VALUE: {
            if (value.IsNull()) {
              // Allowing assigning `null` to message fields.
              return absl::nullopt;
            }
            if (auto int_value = value.AsInt(); int_value) {
              CEL_RETURN_IF_ERROR(well_known_types_.Int64Value().Initialize(
                  field->message_type()));
              well_known_types_.Int64Value().SetValue(
                  reflection_->MutableMessage(message_, field,
                                              message_factory_),
                  int_value->NativeValue());
              return absl::nullopt;
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name());
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT32VALUE: {
            if (value.IsNull()) {
              // Allowing assigning `null` to message fields.
              return absl::nullopt;
            }
            if (auto uint_value = value.AsUint(); uint_value) {
              if (uint_value->NativeValue() >
                  std::numeric_limits<uint32_t>::max()) {
                return absl::OutOfRangeError("uint64 to uint32 overflow");
              }
              CEL_RETURN_IF_ERROR(well_known_types_.UInt32Value().Initialize(
                  field->message_type()));
              well_known_types_.UInt32Value().SetValue(
                  reflection_->MutableMessage(message_, field,
                                              message_factory_),
                  static_cast<uint32_t>(uint_value->NativeValue()));
              return absl::nullopt;
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name());
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT64VALUE: {
            if (value.IsNull()) {
              // Allowing assigning `null` to message fields.
              return absl::nullopt;
            }
            if (auto uint_value = value.AsUint(); uint_value) {
              CEL_RETURN_IF_ERROR(well_known_types_.UInt64Value().Initialize(
                  field->message_type()));
              well_known_types_.UInt64Value().SetValue(
                  reflection_->MutableMessage(message_, field,
                                              message_factory_),
                  uint_value->NativeValue());
              return absl::nullopt;
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name());
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_FLOATVALUE: {
            if (value.IsNull()) {
              // Allowing assigning `null` to message fields.
              return absl::nullopt;
            }
            if (auto double_value = value.AsDouble(); double_value) {
              CEL_RETURN_IF_ERROR(well_known_types_.FloatValue().Initialize(
                  field->message_type()));
              well_known_types_.FloatValue().SetValue(
                  reflection_->MutableMessage(message_, field,
                                              message_factory_),
                  static_cast<float>(double_value->NativeValue()));
              return absl::nullopt;
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name());
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_DOUBLEVALUE: {
            if (value.IsNull()) {
              // Allowing assigning `null` to message fields.
              return absl::nullopt;
            }
            if (auto double_value = value.AsDouble(); double_value) {
              CEL_RETURN_IF_ERROR(well_known_types_.DoubleValue().Initialize(
                  field->message_type()));
              well_known_types_.DoubleValue().SetValue(
                  reflection_->MutableMessage(message_, field,
                                              message_factory_),
                  double_value->NativeValue());
              return absl::nullopt;
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name());
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_BYTESVALUE: {
            if (value.IsNull()) {
              // Allowing assigning `null` to message fields.
              return absl::nullopt;
            }
            if (auto bytes_value = value.AsBytes(); bytes_value) {
              CEL_RETURN_IF_ERROR(well_known_types_.BytesValue().Initialize(
                  field->message_type()));
              well_known_types_.BytesValue().SetValue(
                  reflection_->MutableMessage(message_, field,
                                              message_factory_),
                  bytes_value->NativeCord());
              return absl::nullopt;
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name());
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_STRINGVALUE: {
            if (value.IsNull()) {
              // Allowing assigning `null` to message fields.
              return absl::nullopt;
            }
            if (auto string_value = value.AsString(); string_value) {
              CEL_RETURN_IF_ERROR(well_known_types_.StringValue().Initialize(
                  field->message_type()));
              well_known_types_.StringValue().SetValue(
                  reflection_->MutableMessage(message_, field,
                                              message_factory_),
                  string_value->NativeCord());
              return absl::nullopt;
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name());
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_DURATION: {
            if (value.IsNull()) {
              // Allowing assigning `null` to message fields.
              return absl::nullopt;
            }
            if (auto duration_value = value.AsDuration(); duration_value) {
              CEL_RETURN_IF_ERROR(well_known_types_.Duration().Initialize(
                  field->message_type()));
              CEL_RETURN_IF_ERROR(
                  well_known_types_.Duration().SetFromAbslDuration(
                      reflection_->MutableMessage(message_, field,
                                                  message_factory_),
                      duration_value->NativeValue()));
              return absl::nullopt;
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name());
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_TIMESTAMP: {
            if (value.IsNull()) {
              // Allowing assigning `null` to message fields.
              return absl::nullopt;
            }
            if (auto timestamp_value = value.AsTimestamp(); timestamp_value) {
              CEL_RETURN_IF_ERROR(well_known_types_.Timestamp().Initialize(
                  field->message_type()));
              CEL_RETURN_IF_ERROR(well_known_types_.Timestamp().SetFromAbslTime(
                  reflection_->MutableMessage(message_, field,
                                              message_factory_),
                  timestamp_value->NativeValue()));
              return absl::nullopt;
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name());
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE: {
            CEL_RETURN_IF_ERROR(
                value.ConvertToJson(descriptor_pool_, message_factory_,
                                    reflection_->MutableMessage(
                                        message_, field, message_factory_)));
            return absl::nullopt;
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE: {
            CEL_RETURN_IF_ERROR(value.ConvertToJsonArray(
                descriptor_pool_, message_factory_,
                reflection_->MutableMessage(message_, field,
                                            message_factory_)));
            return absl::nullopt;
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT: {
            CEL_RETURN_IF_ERROR(value.ConvertToJsonObject(
                descriptor_pool_, message_factory_,
                reflection_->MutableMessage(message_, field,
                                            message_factory_)));
            return absl::nullopt;
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_ANY: {
            // Probably not correct, need to use the parent/common one.
            google::protobuf::io::CordOutputStream serialized;
            CEL_RETURN_IF_ERROR(value.SerializeTo(
                descriptor_pool_, message_factory_, &serialized));
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
                std::move(serialized).Consume());
            return absl::nullopt;
          }
          default:
            if (value.IsNull()) {
              // Allowing assigning `null` to message fields.
              return absl::nullopt;
            }
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

  absl::StatusOr<absl::optional<ErrorValue>> SetField(
      const google::protobuf::FieldDescriptor* absl_nonnull field, Value value) {
    if (field->is_map()) {
      return SetMapField(field, std::move(value));
    }
    if (field->is_repeated()) {
      return SetRepeatedField(field, std::move(value));
    }
    return SetSingularField(field, std::move(value));
  }

  google::protobuf::Arena* absl_nullable const arena_;
  const google::protobuf::DescriptorPool* absl_nonnull const descriptor_pool_;
  google::protobuf::MessageFactory* absl_nonnull const message_factory_;
  google::protobuf::Message* absl_nullable message_;
  const google::protobuf::Descriptor* absl_nonnull const descriptor_;
  const google::protobuf::Reflection* absl_nonnull const reflection_;
  well_known_types::Reflection well_known_types_;
};

class ValueBuilderImpl final : public ValueBuilder {
 public:
  ValueBuilderImpl(google::protobuf::Arena* absl_nullable arena,
                   const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                   google::protobuf::MessageFactory* absl_nonnull message_factory,
                   google::protobuf::Message* absl_nonnull message)
      : builder_(arena, descriptor_pool, message_factory, message) {}

  absl::StatusOr<absl::optional<ErrorValue>> SetFieldByName(
      absl::string_view name, Value value) override {
    return builder_.SetFieldByName(name, std::move(value));
  }

  absl::StatusOr<absl::optional<ErrorValue>> SetFieldByNumber(
      int64_t number, Value value) override {
    return builder_.SetFieldByNumber(number, std::move(value));
  }

  absl::StatusOr<Value> Build() && override {
    return std::move(builder_).Build();
  }

 private:
  MessageValueBuilderImpl builder_;
};

class StructValueBuilderImpl final : public StructValueBuilder {
 public:
  StructValueBuilderImpl(
      google::protobuf::Arena* absl_nullable arena,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull message)
      : builder_(arena, descriptor_pool, message_factory, message) {}

  absl::StatusOr<absl::optional<ErrorValue>> SetFieldByName(
      absl::string_view name, Value value) override {
    return builder_.SetFieldByName(name, std::move(value));
  }

  absl::StatusOr<absl::optional<ErrorValue>> SetFieldByNumber(
      int64_t number, Value value) override {
    return builder_.SetFieldByNumber(number, std::move(value));
  }

  absl::StatusOr<StructValue> Build() && override {
    return std::move(builder_).BuildStruct();
  }

 private:
  MessageValueBuilderImpl builder_;
};

}  // namespace

absl_nullable cel::ValueBuilderPtr NewValueBuilder(
    Allocator<> allocator,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    absl::string_view name) {
  const google::protobuf::Descriptor* absl_nullable descriptor =
      descriptor_pool->FindMessageTypeByName(name);
  if (descriptor == nullptr) {
    return nullptr;
  }
  const google::protobuf::Message* absl_nullable prototype =
      message_factory->GetPrototype(descriptor);
  ABSL_DCHECK(prototype != nullptr)
      << "failed to get message prototype from factory, did you pass a dynamic "
         "descriptor to the generated message factory? we consider this to be "
         "a logic error and not a runtime error: "
      << descriptor->full_name();
  if (ABSL_PREDICT_FALSE(prototype == nullptr)) {
    return nullptr;
  }
  return std::make_unique<ValueBuilderImpl>(allocator.arena(), descriptor_pool,
                                            message_factory,
                                            prototype->New(allocator.arena()));
}

absl_nullable cel::StructValueBuilderPtr NewStructValueBuilder(
    Allocator<> allocator,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    absl::string_view name) {
  const google::protobuf::Descriptor* absl_nullable descriptor =
      descriptor_pool->FindMessageTypeByName(name);
  if (descriptor == nullptr) {
    return nullptr;
  }
  const google::protobuf::Message* absl_nullable prototype =
      message_factory->GetPrototype(descriptor);
  ABSL_DCHECK(prototype != nullptr)
      << "failed to get message prototype from factory, did you pass a dynamic "
         "descriptor to the generated message factory? we consider this to be "
         "a logic error and not a runtime error: "
      << descriptor->full_name();
  if (ABSL_PREDICT_FALSE(prototype == nullptr)) {
    return nullptr;
  }
  return std::make_unique<StructValueBuilderImpl>(
      allocator.arena(), descriptor_pool, message_factory,
      prototype->New(allocator.arena()));
}

}  // namespace cel::common_internal
