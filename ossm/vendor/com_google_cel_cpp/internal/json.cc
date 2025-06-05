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

#include "internal/json.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/functional/overload.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/cord.h"
#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "common/json.h"
#include "extensions/protobuf/internal/map_reflection.h"
#include "internal/status_macros.h"
#include "internal/strings.h"
#include "internal/well_known_types.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"
#include "google/protobuf/util/time_util.h"

namespace cel::internal {

namespace {

using ::cel::well_known_types::AsVariant;
using ::cel::well_known_types::GetListValueReflection;
using ::cel::well_known_types::GetListValueReflectionOrDie;
using ::cel::well_known_types::GetRepeatedBytesField;
using ::cel::well_known_types::GetRepeatedStringField;
using ::cel::well_known_types::GetStructReflection;
using ::cel::well_known_types::GetStructReflectionOrDie;
using ::cel::well_known_types::GetValueReflection;
using ::cel::well_known_types::GetValueReflectionOrDie;
using ::cel::well_known_types::ListValueReflection;
using ::cel::well_known_types::Reflection;
using ::cel::well_known_types::StructReflection;
using ::cel::well_known_types::ValueReflection;
using ::google::protobuf::Descriptor;
using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::util::TimeUtil;

// Yanked from the implementation `google::protobuf::util::TimeUtil`.
template <typename Chars>
absl::Status SnakeCaseToCamelCaseImpl(Chars input,
                                      absl::Nonnull<std::string*> output) {
  output->clear();
  bool after_underscore = false;
  for (char input_char : input) {
    if (absl::ascii_isupper(input_char)) {
      // The field name must not contain uppercase letters.
      return absl::InvalidArgumentError(
          "field mask path name contains uppercase letters");
    }
    if (after_underscore) {
      if (absl::ascii_islower(input_char)) {
        output->push_back(absl::ascii_toupper(input_char));
        after_underscore = false;
      } else {
        // The character after a "_" must be a lowercase letter.
        return absl::InvalidArgumentError(
            "field mask path contains '_' not followed by a lowercase letter");
      }
    } else if (input_char == '_') {
      after_underscore = true;
    } else {
      output->push_back(input_char);
    }
  }
  if (after_underscore) {
    // Trailing "_".
    return absl::InvalidArgumentError("field mask path contains trailing '_'");
  }
  return absl::OkStatus();
}

absl::Status SnakeCaseToCamelCase(const well_known_types::StringValue& input,
                                  absl::Nonnull<std::string*> output) {
  return absl::visit(absl::Overload(
                         [&](absl::string_view string) -> absl::Status {
                           return SnakeCaseToCamelCaseImpl(string, output);
                         },
                         [&](const absl::Cord& cord) -> absl::Status {
                           return SnakeCaseToCamelCaseImpl(cord.Chars(),
                                                           output);
                         }),
                     AsVariant(input));
}

class MessageToJsonState;

using MapFieldKeyToString = std::string (*)(const google::protobuf::MapKey&);

std::string BoolMapFieldKeyToString(const google::protobuf::MapKey& key) {
  return key.GetBoolValue() ? "true" : "false";
}

std::string Int32MapFieldKeyToString(const google::protobuf::MapKey& key) {
  return absl::StrCat(key.GetInt32Value());
}

std::string Int64MapFieldKeyToString(const google::protobuf::MapKey& key) {
  return absl::StrCat(key.GetInt64Value());
}

std::string UInt32MapFieldKeyToString(const google::protobuf::MapKey& key) {
  return absl::StrCat(key.GetUInt32Value());
}

std::string UInt64MapFieldKeyToString(const google::protobuf::MapKey& key) {
  return absl::StrCat(key.GetUInt64Value());
}

std::string StringMapFieldKeyToString(const google::protobuf::MapKey& key) {
  return std::string(key.GetStringValue());
}

MapFieldKeyToString GetMapFieldKeyToString(
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field) {
  switch (field->cpp_type()) {
    case FieldDescriptor::CPPTYPE_BOOL:
      return &BoolMapFieldKeyToString;
    case FieldDescriptor::CPPTYPE_INT32:
      return &Int32MapFieldKeyToString;
    case FieldDescriptor::CPPTYPE_INT64:
      return &Int64MapFieldKeyToString;
    case FieldDescriptor::CPPTYPE_UINT32:
      return &UInt32MapFieldKeyToString;
    case FieldDescriptor::CPPTYPE_UINT64:
      return &UInt64MapFieldKeyToString;
    case FieldDescriptor::CPPTYPE_STRING:
      return &StringMapFieldKeyToString;
    default:
      ABSL_UNREACHABLE();
  }
}

using MapFieldValueToValue = absl::Status (MessageToJsonState::*)(
    const google::protobuf::MapValueConstRef& value,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<google::protobuf::MessageLite*> result);

using RepeatedFieldToValue = absl::Status (MessageToJsonState::*)(
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    const google::protobuf::Message& message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
    absl::Nonnull<google::protobuf::MessageLite*> result);

class MessageToJsonState {
 public:
  MessageToJsonState(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory)
      : descriptor_pool_(descriptor_pool), message_factory_(message_factory) {}

  virtual ~MessageToJsonState() = default;

  absl::Status ToJson(const google::protobuf::Message& message,
                      absl::Nonnull<google::protobuf::MessageLite*> result) {
    const auto* descriptor = message.GetDescriptor();
    switch (descriptor->well_known_type()) {
      case Descriptor::WELLKNOWNTYPE_DOUBLEVALUE: {
        CEL_RETURN_IF_ERROR(reflection_.DoubleValue().Initialize(descriptor));
        SetNumberValue(result, reflection_.DoubleValue().GetValue(message));
      } break;
      case Descriptor::WELLKNOWNTYPE_FLOATVALUE: {
        CEL_RETURN_IF_ERROR(reflection_.FloatValue().Initialize(descriptor));
        SetNumberValue(result, reflection_.FloatValue().GetValue(message));
      } break;
      case Descriptor::WELLKNOWNTYPE_INT64VALUE: {
        CEL_RETURN_IF_ERROR(reflection_.Int64Value().Initialize(descriptor));
        SetNumberValue(result, reflection_.Int64Value().GetValue(message));
      } break;
      case Descriptor::WELLKNOWNTYPE_UINT64VALUE: {
        CEL_RETURN_IF_ERROR(reflection_.UInt64Value().Initialize(descriptor));
        SetNumberValue(result, reflection_.UInt64Value().GetValue(message));
      } break;
      case Descriptor::WELLKNOWNTYPE_INT32VALUE: {
        CEL_RETURN_IF_ERROR(reflection_.Int32Value().Initialize(descriptor));
        SetNumberValue(result, reflection_.Int32Value().GetValue(message));
      } break;
      case Descriptor::WELLKNOWNTYPE_UINT32VALUE: {
        CEL_RETURN_IF_ERROR(reflection_.UInt32Value().Initialize(descriptor));
        SetNumberValue(result, reflection_.UInt32Value().GetValue(message));
      } break;
      case Descriptor::WELLKNOWNTYPE_STRINGVALUE: {
        CEL_RETURN_IF_ERROR(reflection_.StringValue().Initialize(descriptor));
        StringValueToJson(reflection_.StringValue().GetValue(message, scratch_),
                          result);
      } break;
      case Descriptor::WELLKNOWNTYPE_BYTESVALUE: {
        CEL_RETURN_IF_ERROR(reflection_.BytesValue().Initialize(descriptor));
        BytesValueToJson(reflection_.BytesValue().GetValue(message, scratch_),
                         result);
      } break;
      case Descriptor::WELLKNOWNTYPE_BOOLVALUE: {
        CEL_RETURN_IF_ERROR(reflection_.BoolValue().Initialize(descriptor));
        SetBoolValue(result, reflection_.BoolValue().GetValue(message));
      } break;
      case Descriptor::WELLKNOWNTYPE_ANY: {
        CEL_ASSIGN_OR_RETURN(auto unpacked,
                             well_known_types::UnpackAnyFrom(
                                 result->GetArena(), reflection_.Any(), message,
                                 descriptor_pool_, message_factory_));
        auto* struct_result = MutableStructValue(result);
        const auto* unpacked_descriptor = unpacked->GetDescriptor();
        SetStringValue(InsertField(struct_result, "@type"),
                       absl::StrCat("type.googleapis.com/",
                                    unpacked_descriptor->full_name()));
        switch (unpacked_descriptor->well_known_type()) {
          case Descriptor::WELLKNOWNTYPE_DOUBLEVALUE:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_FLOATVALUE:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_INT64VALUE:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_UINT64VALUE:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_INT32VALUE:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_UINT32VALUE:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_STRINGVALUE:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_BYTESVALUE:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_BOOLVALUE:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_FIELDMASK:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_DURATION:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_TIMESTAMP:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_VALUE:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_LISTVALUE:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_STRUCT:
            return ToJson(*unpacked, InsertField(struct_result, "value"));
          default:
            if (unpacked_descriptor->full_name() == "google.protobuf.Empty") {
              MutableStructValue(InsertField(struct_result, "value"));
              return absl::OkStatus();
            } else {
              return MessageToJson(*unpacked, struct_result);
            }
        }
      }
      case Descriptor::WELLKNOWNTYPE_FIELDMASK: {
        CEL_RETURN_IF_ERROR(reflection_.FieldMask().Initialize(descriptor));
        std::vector<std::string> paths;
        const int paths_size = reflection_.FieldMask().PathsSize(message);
        for (int i = 0; i < paths_size; ++i) {
          CEL_RETURN_IF_ERROR(SnakeCaseToCamelCase(
              reflection_.FieldMask().Paths(message, i, scratch_),
              &paths.emplace_back()));
        }
        SetStringValue(result, absl::StrJoin(paths, ","));
      } break;
      case Descriptor::WELLKNOWNTYPE_DURATION: {
        CEL_RETURN_IF_ERROR(reflection_.Duration().Initialize(descriptor));
        google::protobuf::Duration duration;
        duration.set_seconds(reflection_.Duration().GetSeconds(message));
        duration.set_nanos(reflection_.Duration().GetNanos(message));
        SetStringValue(result, TimeUtil::ToString(duration));
      } break;
      case Descriptor::WELLKNOWNTYPE_TIMESTAMP: {
        CEL_RETURN_IF_ERROR(reflection_.Timestamp().Initialize(descriptor));
        google::protobuf::Timestamp timestamp;
        timestamp.set_seconds(reflection_.Timestamp().GetSeconds(message));
        timestamp.set_nanos(reflection_.Timestamp().GetNanos(message));
        SetStringValue(result, TimeUtil::ToString(timestamp));
      } break;
      case Descriptor::WELLKNOWNTYPE_VALUE: {
        absl::Cord serialized;
        if (!message.SerializePartialToCord(&serialized)) {
          return absl::UnknownError(
              "failed to serialize message google.protobuf.Value");
        }
        if (!result->ParsePartialFromCord(serialized)) {
          return absl::UnknownError(
              "failed to parsed message: google.protobuf.Value");
        }
      } break;
      case Descriptor::WELLKNOWNTYPE_LISTVALUE: {
        absl::Cord serialized;
        if (!message.SerializePartialToCord(&serialized)) {
          return absl::UnknownError(
              "failed to serialize message google.protobuf.ListValue");
        }
        if (!MutableListValue(result)->ParsePartialFromCord(serialized)) {
          return absl::UnknownError(
              "failed to parsed message: google.protobuf.ListValue");
        }
      } break;
      case Descriptor::WELLKNOWNTYPE_STRUCT: {
        absl::Cord serialized;
        if (!message.SerializePartialToCord(&serialized)) {
          return absl::UnknownError(
              "failed to serialize message google.protobuf.Struct");
        }
        if (!MutableStructValue(result)->ParsePartialFromCord(serialized)) {
          return absl::UnknownError(
              "failed to parsed message: google.protobuf.Struct");
        }
      } break;
      default:
        return MessageToJson(message, MutableStructValue(result));
    }
    return absl::OkStatus();
  }

  absl::Status FieldToJson(const google::protobuf::Message& message,
                           absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
                           absl::Nonnull<google::protobuf::MessageLite*> result) {
    return MessageFieldToJson(message, field, result);
  }

  virtual absl::Status Initialize(
      absl::Nonnull<google::protobuf::MessageLite*> message) = 0;

 private:
  absl::StatusOr<MapFieldValueToValue> GetMapFieldValueToValue(
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field) {
    switch (field->type()) {
      case FieldDescriptor::TYPE_DOUBLE:
        return &MessageToJsonState::MapDoubleFieldToValue;
      case FieldDescriptor::TYPE_FLOAT:
        return &MessageToJsonState::MapFloatFieldToValue;
      case FieldDescriptor::TYPE_FIXED64:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_UINT64:
        return &MessageToJsonState::MapUInt64FieldToValue;
      case FieldDescriptor::TYPE_BOOL:
        return &MessageToJsonState::MapBoolFieldToValue;
      case FieldDescriptor::TYPE_STRING:
        return &MessageToJsonState::MapStringFieldToValue;
      case FieldDescriptor::TYPE_GROUP:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_MESSAGE:
        return &MessageToJsonState::MapMessageFieldToValue;
      case FieldDescriptor::TYPE_BYTES:
        return &MessageToJsonState::MapBytesFieldToValue;
      case FieldDescriptor::TYPE_FIXED32:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_UINT32:
        return &MessageToJsonState::MapUInt32FieldToValue;
      case FieldDescriptor::TYPE_ENUM: {
        const auto* enum_descriptor = field->enum_type();
        if (enum_descriptor->full_name() == "google.protobuf.NullValue") {
          return &MessageToJsonState::MapNullFieldToValue;
        } else {
          return &MessageToJsonState::MapEnumFieldToValue;
        }
      }
      case FieldDescriptor::TYPE_SFIXED32:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_SINT32:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_INT32:
        return &MessageToJsonState::MapInt32FieldToValue;
      case FieldDescriptor::TYPE_SFIXED64:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_SINT64:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_INT64:
        return &MessageToJsonState::MapInt64FieldToValue;
      default:
        return absl::InvalidArgumentError(absl::StrCat(
            "unexpected message field type: ", field->type_name()));
    }
  }

  absl::Status MapBoolFieldToValue(
      const google::protobuf::MapValueConstRef& value,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(value.type(), field->cpp_type());
    ABSL_DCHECK(!field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_BOOL);
    SetBoolValue(result, value.GetBoolValue());
    return absl::OkStatus();
  }

  absl::Status MapInt32FieldToValue(
      const google::protobuf::MapValueConstRef& value,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(value.type(), field->cpp_type());
    ABSL_DCHECK(!field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_INT32);
    SetNumberValue(result, value.GetInt32Value());
    return absl::OkStatus();
  }

  absl::Status MapInt64FieldToValue(
      const google::protobuf::MapValueConstRef& value,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(value.type(), field->cpp_type());
    ABSL_DCHECK(!field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_INT64);
    SetNumberValue(result, value.GetInt64Value());
    return absl::OkStatus();
  }

  absl::Status MapUInt32FieldToValue(
      const google::protobuf::MapValueConstRef& value,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(value.type(), field->cpp_type());
    ABSL_DCHECK(!field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_UINT32);
    SetNumberValue(result, value.GetUInt32Value());
    return absl::OkStatus();
  }

  absl::Status MapUInt64FieldToValue(
      const google::protobuf::MapValueConstRef& value,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(value.type(), field->cpp_type());
    ABSL_DCHECK(!field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_UINT64);
    SetNumberValue(result, value.GetUInt64Value());
    return absl::OkStatus();
  }

  absl::Status MapFloatFieldToValue(
      const google::protobuf::MapValueConstRef& value,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(value.type(), field->cpp_type());
    ABSL_DCHECK(!field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_FLOAT);
    SetNumberValue(result, value.GetFloatValue());
    return absl::OkStatus();
  }

  absl::Status MapDoubleFieldToValue(
      const google::protobuf::MapValueConstRef& value,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(value.type(), field->cpp_type());
    ABSL_DCHECK(!field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_DOUBLE);
    SetNumberValue(result, value.GetDoubleValue());
    return absl::OkStatus();
  }

  absl::Status MapBytesFieldToValue(
      const google::protobuf::MapValueConstRef& value,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(value.type(), field->cpp_type());
    ABSL_DCHECK(!field->is_repeated());
    ABSL_DCHECK_EQ(field->type(), FieldDescriptor::TYPE_BYTES);
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_STRING);
    SetStringValueFromBytes(result, value.GetStringValue());
    return absl::OkStatus();
  }

  absl::Status MapStringFieldToValue(
      const google::protobuf::MapValueConstRef& value,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(value.type(), field->cpp_type());
    ABSL_DCHECK(!field->is_repeated());
    ABSL_DCHECK_EQ(field->type(), FieldDescriptor::TYPE_STRING);
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_STRING);
    SetStringValue(result, value.GetStringValue());
    return absl::OkStatus();
  }

  absl::Status MapMessageFieldToValue(
      const google::protobuf::MapValueConstRef& value,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(value.type(), field->cpp_type());
    ABSL_DCHECK(!field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_MESSAGE);
    return ToJson(value.GetMessageValue(), result);
  }

  absl::Status MapEnumFieldToValue(
      const google::protobuf::MapValueConstRef& value,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(value.type(), field->cpp_type());
    ABSL_DCHECK(!field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_ENUM);
    ABSL_DCHECK_NE(field->enum_type()->full_name(),
                   "google.protobuf.NullValue");
    if (const auto* value_descriptor =
            field->enum_type()->FindValueByNumber(value.GetEnumValue());
        value_descriptor != nullptr) {
      SetStringValue(result, value_descriptor->name());
    } else {
      SetNumberValue(result, value.GetEnumValue());
    }
    return absl::OkStatus();
  }

  absl::Status MapNullFieldToValue(
      const google::protobuf::MapValueConstRef& value,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(value.type(), field->cpp_type());
    ABSL_DCHECK(!field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_ENUM);
    ABSL_DCHECK_EQ(field->enum_type()->full_name(),
                   "google.protobuf.NullValue");
    SetNullValue(result);
    return absl::OkStatus();
  }

  absl::StatusOr<RepeatedFieldToValue> GetRepeatedFieldToValue(
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field) {
    switch (field->type()) {
      case FieldDescriptor::TYPE_DOUBLE:
        return &MessageToJsonState::RepeatedDoubleFieldToValue;
      case FieldDescriptor::TYPE_FLOAT:
        return &MessageToJsonState::RepeatedFloatFieldToValue;
      case FieldDescriptor::TYPE_FIXED64:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_UINT64:
        return &MessageToJsonState::RepeatedUInt64FieldToValue;
      case FieldDescriptor::TYPE_BOOL:
        return &MessageToJsonState::RepeatedBoolFieldToValue;
      case FieldDescriptor::TYPE_STRING:
        return &MessageToJsonState::RepeatedStringFieldToValue;
      case FieldDescriptor::TYPE_GROUP:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_MESSAGE:
        return &MessageToJsonState::RepeatedMessageFieldToValue;
      case FieldDescriptor::TYPE_BYTES:
        return &MessageToJsonState::RepeatedBytesFieldToValue;
      case FieldDescriptor::TYPE_FIXED32:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_UINT32:
        return &MessageToJsonState::RepeatedUInt32FieldToValue;
      case FieldDescriptor::TYPE_ENUM: {
        const auto* enum_descriptor = field->enum_type();
        if (enum_descriptor->full_name() == "google.protobuf.NullValue") {
          return &MessageToJsonState::RepeatedNullFieldToValue;
        } else {
          return &MessageToJsonState::RepeatedEnumFieldToValue;
        }
      }
      case FieldDescriptor::TYPE_SFIXED32:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_SINT32:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_INT32:
        return &MessageToJsonState::RepeatedInt32FieldToValue;
      case FieldDescriptor::TYPE_SFIXED64:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_SINT64:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_INT64:
        return &MessageToJsonState::RepeatedInt64FieldToValue;
      default:
        return absl::InvalidArgumentError(absl::StrCat(
            "unexpected message field type: ", field->type_name()));
    }
  }

  absl::Status RepeatedBoolFieldToValue(
      absl::Nonnull<const google::protobuf::Reflection*> reflection,
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(reflection, message.GetReflection());
    ABSL_DCHECK(!field->is_map() && field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_BOOL);
    SetBoolValue(result, reflection->GetRepeatedBool(message, field, index));
    return absl::OkStatus();
  }

  absl::Status RepeatedInt32FieldToValue(
      absl::Nonnull<const google::protobuf::Reflection*> reflection,
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(reflection, message.GetReflection());
    ABSL_DCHECK(!field->is_map() && field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_INT32);
    SetNumberValue(result, reflection->GetRepeatedInt32(message, field, index));
    return absl::OkStatus();
  }

  absl::Status RepeatedInt64FieldToValue(
      absl::Nonnull<const google::protobuf::Reflection*> reflection,
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(reflection, message.GetReflection());
    ABSL_DCHECK(!field->is_map() && field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_INT64);
    SetNumberValue(result, reflection->GetRepeatedInt64(message, field, index));
    return absl::OkStatus();
  }

  absl::Status RepeatedUInt32FieldToValue(
      absl::Nonnull<const google::protobuf::Reflection*> reflection,
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(reflection, message.GetReflection());
    ABSL_DCHECK(!field->is_map() && field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_UINT32);
    SetNumberValue(result,
                   reflection->GetRepeatedUInt32(message, field, index));
    return absl::OkStatus();
  }

  absl::Status RepeatedUInt64FieldToValue(
      absl::Nonnull<const google::protobuf::Reflection*> reflection,
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(reflection, message.GetReflection());
    ABSL_DCHECK(!field->is_map() && field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_UINT64);
    SetNumberValue(result,
                   reflection->GetRepeatedUInt64(message, field, index));
    return absl::OkStatus();
  }

  absl::Status RepeatedFloatFieldToValue(
      absl::Nonnull<const google::protobuf::Reflection*> reflection,
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(reflection, message.GetReflection());
    ABSL_DCHECK(!field->is_map() && field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_FLOAT);
    SetNumberValue(result, reflection->GetRepeatedFloat(message, field, index));
    return absl::OkStatus();
  }

  absl::Status RepeatedDoubleFieldToValue(
      absl::Nonnull<const google::protobuf::Reflection*> reflection,
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(reflection, message.GetReflection());
    ABSL_DCHECK(!field->is_map() && field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_DOUBLE);
    SetNumberValue(result,
                   reflection->GetRepeatedDouble(message, field, index));
    return absl::OkStatus();
  }

  absl::Status RepeatedBytesFieldToValue(
      absl::Nonnull<const google::protobuf::Reflection*> reflection,
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(reflection, message.GetReflection());
    ABSL_DCHECK(!field->is_map() && field->is_repeated());
    ABSL_DCHECK_EQ(field->type(), FieldDescriptor::TYPE_BYTES);
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_STRING);
    absl::visit(absl::Overload(
                    [&](absl::string_view string) -> void {
                      SetStringValueFromBytes(result, string);
                    },
                    [&](absl::Cord&& cord) -> void {
                      SetStringValueFromBytes(result, cord);
                    }),
                AsVariant(GetRepeatedBytesField(reflection, message, field,
                                                index, scratch_)));
    return absl::OkStatus();
  }

  absl::Status RepeatedStringFieldToValue(
      absl::Nonnull<const google::protobuf::Reflection*> reflection,
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(reflection, message.GetReflection());
    ABSL_DCHECK(!field->is_map() && field->is_repeated());
    ABSL_DCHECK_EQ(field->type(), FieldDescriptor::TYPE_STRING);
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_STRING);
    absl::visit(
        absl::Overload(
            [&](absl::string_view string) -> void {
              SetStringValue(result, string);
            },
            [&](absl::Cord&& cord) -> void { SetStringValue(result, cord); }),
        AsVariant(GetRepeatedStringField(reflection, message, field, index,
                                         scratch_)));
    return absl::OkStatus();
  }

  absl::Status RepeatedMessageFieldToValue(
      absl::Nonnull<const google::protobuf::Reflection*> reflection,
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(reflection, message.GetReflection());
    ABSL_DCHECK(!field->is_map() && field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_MESSAGE);
    return ToJson(reflection->GetRepeatedMessage(message, field, index),
                  result);
  }

  absl::Status RepeatedEnumFieldToValue(
      absl::Nonnull<const google::protobuf::Reflection*> reflection,
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(reflection, message.GetReflection());
    ABSL_DCHECK(!field->is_map() && field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_ENUM);
    ABSL_DCHECK_NE(field->enum_type()->full_name(),
                   "google.protobuf.NullValue");
    if (const auto* value = reflection->GetRepeatedEnum(message, field, index);
        value != nullptr) {
      SetStringValue(result, value->name());
    } else {
      SetNumberValue(result,
                     reflection->GetRepeatedEnumValue(message, field, index));
    }
    return absl::OkStatus();
  }

  absl::Status RepeatedNullFieldToValue(
      absl::Nonnull<const google::protobuf::Reflection*> reflection,
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(reflection, message.GetReflection());
    ABSL_DCHECK(!field->is_map() && field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_ENUM);
    ABSL_DCHECK_EQ(field->enum_type()->full_name(),
                   "google.protobuf.NullValue");
    SetNullValue(result);
    return absl::OkStatus();
  }

  absl::Status MessageMapFieldToJson(
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    const auto* reflection = message.GetReflection();
    if (reflection->FieldSize(message, field) == 0) {
      return absl::OkStatus();
    }
    const auto key_to_string =
        GetMapFieldKeyToString(field->message_type()->map_key());
    const auto* value_descriptor = field->message_type()->map_value();
    CEL_ASSIGN_OR_RETURN(const auto value_to_value,
                         GetMapFieldValueToValue(value_descriptor));
    auto begin =
        extensions::protobuf_internal::MapBegin(*reflection, message, *field);
    const auto end =
        extensions::protobuf_internal::MapEnd(*reflection, message, *field);
    for (; begin != end; ++begin) {
      auto key = (*key_to_string)(begin.GetKey());
      CEL_RETURN_IF_ERROR((this->*value_to_value)(
          begin.GetValueRef(), value_descriptor, InsertField(result, key)));
    }
    return absl::OkStatus();
  }

  absl::Status MessageRepeatedFieldToJson(
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    const auto* reflection = message.GetReflection();
    const int size = reflection->FieldSize(message, field);
    if (size == 0) {
      return absl::OkStatus();
    }
    ReserveValues(result, size);
    CEL_ASSIGN_OR_RETURN(const auto to_value, GetRepeatedFieldToValue(field));
    for (int index = 0; index < size; ++index) {
      CEL_RETURN_IF_ERROR((this->*to_value)(reflection, message, field, index,
                                            AddValues(result)));
    }
    return absl::OkStatus();
  }

  absl::Status MessageFieldToJson(
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    if (field->is_map()) {
      return MessageMapFieldToJson(message, field, MutableStructValue(result));
    }
    if (field->is_repeated()) {
      return MessageRepeatedFieldToJson(message, field,
                                        MutableListValue(result));
    }
    const auto* reflection = message.GetReflection();
    switch (field->type()) {
      case FieldDescriptor::TYPE_DOUBLE:
        SetNumberValue(result, reflection->GetDouble(message, field));
        break;
      case FieldDescriptor::TYPE_FLOAT:
        SetNumberValue(result, reflection->GetFloat(message, field));
        break;
      case FieldDescriptor::TYPE_FIXED64:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_UINT64:
        SetNumberValue(result, reflection->GetUInt64(message, field));
        break;
      case FieldDescriptor::TYPE_BOOL:
        SetBoolValue(result, reflection->GetBool(message, field));
        break;
      case FieldDescriptor::TYPE_STRING:
        StringValueToJson(
            well_known_types::GetStringField(message, field, scratch_), result);
        break;
      case FieldDescriptor::TYPE_GROUP:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_MESSAGE:
        return ToJson(reflection->GetMessage(message, field), result);
      case FieldDescriptor::TYPE_BYTES:
        BytesValueToJson(
            well_known_types::GetBytesField(message, field, scratch_), result);
        break;
      case FieldDescriptor::TYPE_FIXED32:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_UINT32:
        SetNumberValue(result, reflection->GetUInt32(message, field));
        break;
      case FieldDescriptor::TYPE_ENUM: {
        const auto* enum_descriptor = field->enum_type();
        if (enum_descriptor->full_name() == "google.protobuf.NullValue") {
          SetNullValue(result);
        } else {
          const auto* enum_value_descriptor =
              reflection->GetEnum(message, field);
          if (enum_value_descriptor != nullptr) {
            SetStringValue(result, enum_value_descriptor->name());
          } else {
            SetNumberValue(result, reflection->GetEnumValue(message, field));
          }
        }
      } break;
      case FieldDescriptor::TYPE_SFIXED32:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_SINT32:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_INT32:
        SetNumberValue(result, reflection->GetInt32(message, field));
        break;
      case FieldDescriptor::TYPE_SFIXED64:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_SINT64:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_INT64:
        SetNumberValue(result, reflection->GetInt64(message, field));
        break;
      default:
        return absl::InvalidArgumentError(absl::StrCat(
            "unexpected message field type: ", field->type_name()));
    }
    return absl::OkStatus();
  }

  absl::Status MessageToJson(const google::protobuf::Message& message,
                             absl::Nonnull<google::protobuf::MessageLite*> result) {
    std::vector<const google::protobuf::FieldDescriptor*> fields;
    const auto* reflection = message.GetReflection();
    reflection->ListFields(message, &fields);
    if (!fields.empty()) {
      for (const auto* field : fields) {
        CEL_RETURN_IF_ERROR(MessageFieldToJson(
            message, field, InsertField(result, field->json_name())));
      }
    }
    return absl::OkStatus();
  }

  void StringValueToJson(const well_known_types::StringValue& value,
                         absl::Nonnull<google::protobuf::MessageLite*> result) const {
    absl::visit(absl::Overload([&](absl::string_view string)
                                   -> void { SetStringValue(result, string); },
                               [&](const absl::Cord& cord) -> void {
                                 SetStringValue(result, cord);
                               }),
                AsVariant(value));
  }

  void BytesValueToJson(const well_known_types::BytesValue& value,
                        absl::Nonnull<google::protobuf::MessageLite*> result) const {
    absl::visit(absl::Overload(
                    [&](absl::string_view string) -> void {
                      SetStringValueFromBytes(result, string);
                    },
                    [&](const absl::Cord& cord) -> void {
                      SetStringValueFromBytes(result, cord);
                    }),
                AsVariant(value));
  }

  virtual void SetNullValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const = 0;

  virtual void SetBoolValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                            bool value) const = 0;

  virtual void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                              double value) const = 0;

  void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      float value) const {
    SetNumberValue(message, static_cast<double>(value));
  }

  virtual void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                              int64_t value) const = 0;

  void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      int32_t value) const {
    SetNumberValue(message, static_cast<double>(value));
  }

  virtual void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                              uint64_t value) const = 0;

  void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      uint32_t value) const {
    SetNumberValue(message, static_cast<double>(value));
  }

  virtual void SetStringValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                              absl::string_view value) const = 0;

  virtual void SetStringValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                              const absl::Cord& value) const = 0;

  void SetStringValueFromBytes(absl::Nonnull<google::protobuf::MessageLite*> message,
                               absl::string_view value) const {
    if (value.empty()) {
      SetStringValue(message, value);
      return;
    }
    SetStringValue(message, absl::Base64Escape(value));
  }

  void SetStringValueFromBytes(absl::Nonnull<google::protobuf::MessageLite*> message,
                               const absl::Cord& value) const {
    if (value.empty()) {
      SetStringValue(message, value);
      return;
    }
    if (auto flat = value.TryFlat(); flat) {
      SetStringValue(message, absl::Base64Escape(*flat));
      return;
    }
    SetStringValue(message,
                   absl::Base64Escape(static_cast<std::string>(value)));
  }

  virtual absl::Nonnull<google::protobuf::MessageLite*> MutableListValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const = 0;

  virtual absl::Nonnull<google::protobuf::MessageLite*> MutableStructValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const = 0;

  virtual void ReserveValues(absl::Nonnull<google::protobuf::MessageLite*> message,
                             int capacity) const = 0;

  virtual absl::Nonnull<google::protobuf::MessageLite*> AddValues(
      absl::Nonnull<google::protobuf::MessageLite*> message) const = 0;

  virtual absl::Nonnull<google::protobuf::MessageLite*> InsertField(
      absl::Nonnull<google::protobuf::MessageLite*> message,
      absl::string_view name) const = 0;

  absl::Nonnull<const google::protobuf::DescriptorPool*> const descriptor_pool_;
  absl::Nonnull<google::protobuf::MessageFactory*> const message_factory_;
  std::string scratch_;
  Reflection reflection_;
};

class GeneratedMessageToJsonState final : public MessageToJsonState {
 public:
  using MessageToJsonState::MessageToJsonState;

  absl::Status Initialize(
      absl::Nonnull<google::protobuf::MessageLite*> message) override {
    // Nothing to do.
    return absl::OkStatus();
  }

 private:
  void SetNullValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const override {
    ValueReflection::SetNullValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message));
  }

  void SetBoolValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                    bool value) const override {
    ValueReflection::SetBoolValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message), value);
  }

  void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      double value) const override {
    ValueReflection::SetNumberValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message), value);
  }

  void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      int64_t value) const override {
    ValueReflection::SetNumberValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message), value);
  }

  void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      uint64_t value) const override {
    ValueReflection::SetNumberValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message), value);
  }

  void SetStringValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      absl::string_view value) const override {
    ValueReflection::SetStringValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message), value);
  }

  void SetStringValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      const absl::Cord& value) const override {
    ValueReflection::SetStringValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message), value);
  }

  absl::Nonnull<google::protobuf::MessageLite*> MutableListValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const override {
    return ValueReflection::MutableListValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message));
  }

  absl::Nonnull<google::protobuf::MessageLite*> MutableStructValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const override {
    return ValueReflection::MutableStructValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message));
  }

  void ReserveValues(absl::Nonnull<google::protobuf::MessageLite*> message,
                     int capacity) const override {
    ListValueReflection::ReserveValues(
        google::protobuf::DownCastMessage<google::protobuf::ListValue>(message),
        capacity);
  }

  absl::Nonnull<google::protobuf::MessageLite*> AddValues(
      absl::Nonnull<google::protobuf::MessageLite*> message) const override {
    return ListValueReflection::AddValues(
        google::protobuf::DownCastMessage<google::protobuf::ListValue>(message));
  }

  absl::Nonnull<google::protobuf::MessageLite*> InsertField(
      absl::Nonnull<google::protobuf::MessageLite*> message,
      absl::string_view name) const override {
    return StructReflection::InsertField(
        google::protobuf::DownCastMessage<google::protobuf::Struct>(message), name);
  }
};

class DynamicMessageToJsonState final : public MessageToJsonState {
 public:
  using MessageToJsonState::MessageToJsonState;

  absl::Status Initialize(
      absl::Nonnull<google::protobuf::MessageLite*> message) override {
    CEL_RETURN_IF_ERROR(value_reflection_.Initialize(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message)->GetDescriptor()));
    CEL_RETURN_IF_ERROR(list_value_reflection_.Initialize(
        value_reflection_.GetListValueDescriptor()));
    CEL_RETURN_IF_ERROR(
        struct_reflection_.Initialize(value_reflection_.GetStructDescriptor()));
    return absl::OkStatus();
  }

 private:
  void SetNullValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const override {
    value_reflection_.SetNullValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message));
  }

  void SetBoolValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                    bool value) const override {
    value_reflection_.SetBoolValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message), value);
  }

  void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      double value) const override {
    value_reflection_.SetNumberValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message), value);
  }

  void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      int64_t value) const override {
    value_reflection_.SetNumberValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message), value);
  }

  void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      uint64_t value) const override {
    value_reflection_.SetNumberValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message), value);
  }

  void SetStringValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      absl::string_view value) const override {
    value_reflection_.SetStringValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message), value);
  }

  void SetStringValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      const absl::Cord& value) const override {
    value_reflection_.SetStringValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message), value);
  }

  absl::Nonnull<google::protobuf::MessageLite*> MutableListValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const override {
    return value_reflection_.MutableListValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message));
  }

  absl::Nonnull<google::protobuf::MessageLite*> MutableStructValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const override {
    return value_reflection_.MutableStructValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message));
  }

  void ReserveValues(absl::Nonnull<google::protobuf::MessageLite*> message,
                     int capacity) const override {
    list_value_reflection_.ReserveValues(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message), capacity);
  }

  absl::Nonnull<google::protobuf::MessageLite*> AddValues(
      absl::Nonnull<google::protobuf::MessageLite*> message) const override {
    return list_value_reflection_.AddValues(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message));
  }

  absl::Nonnull<google::protobuf::MessageLite*> InsertField(
      absl::Nonnull<google::protobuf::MessageLite*> message,
      absl::string_view name) const override {
    return struct_reflection_.InsertField(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message), name);
  }

  ValueReflection value_reflection_;
  ListValueReflection list_value_reflection_;
  StructReflection struct_reflection_;
};

}  // namespace

absl::Status MessageToJson(
    const google::protobuf::Message& message,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Value*> result) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(result != nullptr);
  auto state = std::make_unique<GeneratedMessageToJsonState>(descriptor_pool,
                                                             message_factory);
  CEL_RETURN_IF_ERROR(state->Initialize(result));
  return state->ToJson(message, result);
}

absl::Status MessageToJson(
    const google::protobuf::Message& message,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Message*> result) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(result != nullptr);
  auto state = std::make_unique<DynamicMessageToJsonState>(descriptor_pool,
                                                           message_factory);
  CEL_RETURN_IF_ERROR(state->Initialize(result));
  return state->ToJson(message, result);
}

absl::Status MessageFieldToJson(
    const google::protobuf::Message& message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Value*> result) {
  ABSL_DCHECK_EQ(field->containing_type(), message.GetDescriptor());
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(result != nullptr);
  auto state = std::make_unique<GeneratedMessageToJsonState>(descriptor_pool,
                                                             message_factory);
  CEL_RETURN_IF_ERROR(state->Initialize(result));
  return state->FieldToJson(message, field, result);
}

absl::Status MessageFieldToJson(
    const google::protobuf::Message& message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Message*> result) {
  ABSL_DCHECK_EQ(field->containing_type(), message.GetDescriptor());
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(result != nullptr);
  auto state = std::make_unique<DynamicMessageToJsonState>(descriptor_pool,
                                                           message_factory);
  CEL_RETURN_IF_ERROR(state->Initialize(result));
  return state->FieldToJson(message, field, result);
}

absl::Status CheckJson(const google::protobuf::MessageLite& message) {
  if (const auto* generated_message =
          google::protobuf::DynamicCastMessage<google::protobuf::Value>(&message);
      generated_message) {
    return absl::OkStatus();
  }
  if (const auto* dynamic_message =
          google::protobuf::DynamicCastMessage<google::protobuf::Message>(&message);
      dynamic_message) {
    CEL_ASSIGN_OR_RETURN(auto reflection,
                         GetValueReflection(dynamic_message->GetDescriptor()));
    CEL_RETURN_IF_ERROR(
        GetListValueReflection(reflection.GetListValueDescriptor()).status());
    CEL_RETURN_IF_ERROR(
        GetStructReflection(reflection.GetStructDescriptor()).status());
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(
      absl::StrCat("message must be an instance of `google.protobuf.Value`: ",
                   message.GetTypeName()));
}

absl::Status CheckJsonList(const google::protobuf::MessageLite& message) {
  if (const auto* generated_message =
          google::protobuf::DynamicCastMessage<google::protobuf::ListValue>(&message);
      generated_message) {
    return absl::OkStatus();
  }
  if (const auto* dynamic_message =
          google::protobuf::DynamicCastMessage<google::protobuf::Message>(&message);
      dynamic_message) {
    CEL_ASSIGN_OR_RETURN(
        auto reflection,
        GetListValueReflection(dynamic_message->GetDescriptor()));
    CEL_ASSIGN_OR_RETURN(auto value_reflection,
                         GetValueReflection(reflection.GetValueDescriptor()));
    CEL_RETURN_IF_ERROR(
        GetStructReflection(value_reflection.GetStructDescriptor()).status());
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(absl::StrCat(
      "message must be an instance of `google.protobuf.ListValue`: ",
      message.GetTypeName()));
}

absl::Status CheckJsonMap(const google::protobuf::MessageLite& message) {
  if (const auto* generated_message =
          google::protobuf::DynamicCastMessage<google::protobuf::Struct>(&message);
      generated_message) {
    return absl::OkStatus();
  }
  if (const auto* dynamic_message =
          google::protobuf::DynamicCastMessage<google::protobuf::Message>(&message);
      dynamic_message) {
    CEL_ASSIGN_OR_RETURN(auto reflection,
                         GetStructReflection(dynamic_message->GetDescriptor()));
    CEL_ASSIGN_OR_RETURN(auto value_reflection,
                         GetValueReflection(reflection.GetValueDescriptor()));
    CEL_RETURN_IF_ERROR(
        GetListValueReflection(value_reflection.GetListValueDescriptor())
            .status());
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(
      absl::StrCat("message must be an instance of `google.protobuf.Struct`: ",
                   message.GetTypeName()));
}

namespace {

class JsonMapIterator final {
 public:
  using Generated =
      typename google::protobuf::Map<std::string,
                           google::protobuf::Value>::const_iterator;
  using Dynamic = google::protobuf::MapIterator;
  using Value = std::pair<well_known_types::StringValue,
                          absl::Nonnull<const google::protobuf::MessageLite*>>;

  // NOLINTNEXTLINE(google-explicit-constructor)
  JsonMapIterator(Generated generated) : variant_(std::move(generated)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  JsonMapIterator(Dynamic dynamic) : variant_(std::move(dynamic)) {}

  JsonMapIterator(const JsonMapIterator&) = default;
  JsonMapIterator(JsonMapIterator&&) = default;
  JsonMapIterator& operator=(const JsonMapIterator&) = default;
  JsonMapIterator& operator=(JsonMapIterator&&) = default;

  Value Next(std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    Value result;
    absl::visit(absl::Overload(
                    [&](Generated& generated) -> void {
                      result = std::pair{absl::string_view(generated->first),
                                         &generated->second};
                      ++generated;
                    },
                    [&](Dynamic& dynamic) -> void {
                      const auto& key = dynamic.GetKey().GetStringValue();
                      scratch.assign(key.data(), key.size());
                      result =
                          std::pair{absl::string_view(scratch),
                                    &dynamic.GetValueRef().GetMessageValue()};
                      ++dynamic;
                    }),
                variant_);
    return result;
  }

 private:
  absl::variant<Generated, Dynamic> variant_;
};

class JsonAccessor {
 public:
  virtual ~JsonAccessor() = default;

  virtual google::protobuf::Value::KindCase GetKindCase(
      const google::protobuf::MessageLite& message) const = 0;

  virtual bool GetBoolValue(const google::protobuf::MessageLite& message) const = 0;

  virtual double GetNumberValue(const google::protobuf::MessageLite& message) const = 0;

  virtual well_known_types::StringValue GetStringValue(
      const google::protobuf::MessageLite& message,
      std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const = 0;

  virtual const google::protobuf::MessageLite& GetListValue(
      const google::protobuf::MessageLite& message) const = 0;

  virtual int ValuesSize(const google::protobuf::MessageLite& message) const = 0;

  virtual const google::protobuf::MessageLite& Values(const google::protobuf::MessageLite& message,
                                            int index) const = 0;

  virtual const google::protobuf::MessageLite& GetStructValue(
      const google::protobuf::MessageLite& message) const = 0;

  virtual int FieldsSize(const google::protobuf::MessageLite& message) const = 0;

  virtual absl::Nullable<const google::protobuf::MessageLite*> FindField(
      const google::protobuf::MessageLite& message, absl::string_view name) const = 0;

  virtual JsonMapIterator IterateFields(
      const google::protobuf::MessageLite& message) const = 0;
};

class GeneratedJsonAccessor final : public JsonAccessor {
 public:
  static absl::Nonnull<const GeneratedJsonAccessor*> Singleton() {
    static const absl::NoDestructor<GeneratedJsonAccessor> singleton;
    return &*singleton;
  }

  google::protobuf::Value::KindCase GetKindCase(
      const google::protobuf::MessageLite& message) const override {
    return ValueReflection::GetKindCase(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message));
  }

  bool GetBoolValue(const google::protobuf::MessageLite& message) const override {
    return ValueReflection::GetBoolValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message));
  }

  double GetNumberValue(const google::protobuf::MessageLite& message) const override {
    return ValueReflection::GetNumberValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message));
  }

  well_known_types::StringValue GetStringValue(
      const google::protobuf::MessageLite& message, std::string&) const override {
    return ValueReflection::GetStringValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message));
  }

  const google::protobuf::MessageLite& GetListValue(
      const google::protobuf::MessageLite& message) const override {
    return ValueReflection::GetListValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message));
  }

  int ValuesSize(const google::protobuf::MessageLite& message) const override {
    return ListValueReflection::ValuesSize(
        google::protobuf::DownCastMessage<google::protobuf::ListValue>(message));
  }

  const google::protobuf::MessageLite& Values(const google::protobuf::MessageLite& message,
                                    int index) const override {
    return ListValueReflection::Values(
        google::protobuf::DownCastMessage<google::protobuf::ListValue>(message), index);
  }

  const google::protobuf::MessageLite& GetStructValue(
      const google::protobuf::MessageLite& message) const override {
    return ValueReflection::GetStructValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message));
  }

  int FieldsSize(const google::protobuf::MessageLite& message) const override {
    return StructReflection::FieldsSize(
        google::protobuf::DownCastMessage<google::protobuf::Struct>(message));
  }

  absl::Nullable<const google::protobuf::MessageLite*> FindField(
      const google::protobuf::MessageLite& message,
      absl::string_view name) const override {
    return StructReflection::FindField(
        google::protobuf::DownCastMessage<google::protobuf::Struct>(message), name);
  }

  JsonMapIterator IterateFields(
      const google::protobuf::MessageLite& message) const override {
    return StructReflection::BeginFields(
        google::protobuf::DownCastMessage<google::protobuf::Struct>(message));
  }
};

class DynamicJsonAccessor final : public JsonAccessor {
 public:
  void InitializeValue(const google::protobuf::Message& message) {
    value_reflection_ = GetValueReflectionOrDie(message.GetDescriptor());
    list_value_reflection_ =
        GetListValueReflectionOrDie(value_reflection_.GetListValueDescriptor());
    struct_reflection_ =
        GetStructReflectionOrDie(value_reflection_.GetStructDescriptor());
  }

  void InitializeListValue(const google::protobuf::Message& message) {
    list_value_reflection_ =
        GetListValueReflectionOrDie(message.GetDescriptor());
    value_reflection_ =
        GetValueReflectionOrDie(list_value_reflection_.GetValueDescriptor());
    struct_reflection_ =
        GetStructReflectionOrDie(value_reflection_.GetStructDescriptor());
  }

  void InitializeStruct(const google::protobuf::Message& message) {
    struct_reflection_ = GetStructReflectionOrDie(message.GetDescriptor());
    value_reflection_ =
        GetValueReflectionOrDie(struct_reflection_.GetValueDescriptor());
    list_value_reflection_ =
        GetListValueReflectionOrDie(value_reflection_.GetListValueDescriptor());
  }

  google::protobuf::Value::KindCase GetKindCase(
      const google::protobuf::MessageLite& message) const override {
    return value_reflection_.GetKindCase(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message));
  }

  bool GetBoolValue(const google::protobuf::MessageLite& message) const override {
    return value_reflection_.GetBoolValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message));
  }

  double GetNumberValue(const google::protobuf::MessageLite& message) const override {
    return value_reflection_.GetNumberValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message));
  }

  well_known_types::StringValue GetStringValue(
      const google::protobuf::MessageLite& message, std::string& scratch) const override {
    return value_reflection_.GetStringValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message), scratch);
  }

  const google::protobuf::MessageLite& GetListValue(
      const google::protobuf::MessageLite& message) const override {
    return value_reflection_.GetListValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message));
  }

  int ValuesSize(const google::protobuf::MessageLite& message) const override {
    return list_value_reflection_.ValuesSize(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message));
  }

  const google::protobuf::MessageLite& Values(const google::protobuf::MessageLite& message,
                                    int index) const override {
    return list_value_reflection_.Values(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message), index);
  }

  const google::protobuf::MessageLite& GetStructValue(
      const google::protobuf::MessageLite& message) const override {
    return value_reflection_.GetStructValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message));
  }

  int FieldsSize(const google::protobuf::MessageLite& message) const override {
    return struct_reflection_.FieldsSize(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message));
  }

  absl::Nullable<const google::protobuf::MessageLite*> FindField(
      const google::protobuf::MessageLite& message,
      absl::string_view name) const override {
    return struct_reflection_.FindField(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message), name);
  }

  JsonMapIterator IterateFields(
      const google::protobuf::MessageLite& message) const override {
    return struct_reflection_.BeginFields(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message));
  }

 private:
  ValueReflection value_reflection_;
  ListValueReflection list_value_reflection_;
  StructReflection struct_reflection_;
};

std::string JsonStringDebugString(const well_known_types::StringValue& value) {
  return absl::visit(absl::Overload(
                         [&](absl::string_view string) -> std::string {
                           return FormatStringLiteral(string);
                         },
                         [&](const absl::Cord& cord) -> std::string {
                           return FormatStringLiteral(cord);
                         }),
                     well_known_types::AsVariant(value));
}

std::string JsonNumberDebugString(double value) {
  if (std::isfinite(value)) {
    if (std::floor(value) != value) {
      // The double is not representable as a whole number, so use
      // absl::StrCat which will add decimal places.
      return absl::StrCat(value);
    }
    // absl::StrCat historically would represent 0.0 as 0, and we want the
    // decimal places so ZetaSQL correctly assumes the type as double
    // instead of int64_t.
    std::string stringified = absl::StrCat(value);
    if (!absl::StrContains(stringified, '.')) {
      absl::StrAppend(&stringified, ".0");
    } else {
      // absl::StrCat has a decimal now? Use it directly.
    }
    return stringified;
  }
  if (std::isnan(value)) {
    return "nan";
  }
  if (std::signbit(value)) {
    return "-infinity";
  }
  return "+infinity";
}

class JsonDebugStringState final {
 public:
  JsonDebugStringState(absl::Nonnull<const JsonAccessor*> accessor,
                       absl::Nonnull<std::string*> output)
      : accessor_(accessor), output_(output) {}

  void ValueDebugString(const google::protobuf::MessageLite& message) {
    const auto kind_case = accessor_->GetKindCase(message);
    switch (kind_case) {
      case google::protobuf::Value::KIND_NOT_SET:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::Value::kNullValue:
        output_->append("null");
        break;
      case google::protobuf::Value::kBoolValue:
        if (accessor_->GetBoolValue(message)) {
          output_->append("true");
        } else {
          output_->append("false");
        }
        break;
      case google::protobuf::Value::kNumberValue:
        output_->append(
            JsonNumberDebugString(accessor_->GetNumberValue(message)));
        break;
      case google::protobuf::Value::kStringValue:
        output_->append(JsonStringDebugString(
            accessor_->GetStringValue(message, scratch_)));
        break;
      case google::protobuf::Value::kListValue:
        ListValueDebugString(accessor_->GetListValue(message));
        break;
      case google::protobuf::Value::kStructValue:
        StructDebugString(accessor_->GetStructValue(message));
        break;
      default:
        // Should not get here, but if for some terrible reason
        // `google.protobuf.Value` is expanded, just skip.
        break;
    }
  }

  void ListValueDebugString(const google::protobuf::MessageLite& message) {
    const int size = accessor_->ValuesSize(message);
    output_->push_back('[');
    for (int i = 0; i < size; ++i) {
      if (i > 0) {
        output_->append(", ");
      }
      ValueDebugString(accessor_->Values(message, i));
    }
    output_->push_back(']');
  }

  void StructDebugString(const google::protobuf::MessageLite& message) {
    const int size = accessor_->FieldsSize(message);
    std::string key_scratch;
    well_known_types::StringValue key;
    absl::Nonnull<const google::protobuf::MessageLite*> value;
    auto iterator = accessor_->IterateFields(message);
    output_->push_back('{');
    for (int i = 0; i < size; ++i) {
      if (i > 0) {
        output_->append(", ");
      }
      std::tie(key, value) = iterator.Next(key_scratch);
      output_->append(JsonStringDebugString(key));
      output_->append(": ");
      ValueDebugString(*value);
    }
    output_->push_back('}');
  }

 private:
  const absl::Nonnull<const JsonAccessor*> accessor_;
  const absl::Nonnull<std::string*> output_;
  std::string scratch_;
};

}  // namespace

std::string JsonDebugString(const google::protobuf::Value& message) {
  std::string output;
  JsonDebugStringState(GeneratedJsonAccessor::Singleton(), &output)
      .ValueDebugString(message);
  return output;
}

std::string JsonDebugString(const google::protobuf::Message& message) {
  DynamicJsonAccessor accessor;
  accessor.InitializeValue(message);
  std::string output;
  JsonDebugStringState(&accessor, &output).ValueDebugString(message);
  return output;
}

std::string JsonListDebugString(const google::protobuf::ListValue& message) {
  std::string output;
  JsonDebugStringState(GeneratedJsonAccessor::Singleton(), &output)
      .ListValueDebugString(message);
  return output;
}

std::string JsonListDebugString(const google::protobuf::Message& message) {
  DynamicJsonAccessor accessor;
  accessor.InitializeListValue(message);
  std::string output;
  JsonDebugStringState(&accessor, &output).ListValueDebugString(message);
  return output;
}

std::string JsonMapDebugString(const google::protobuf::Struct& message) {
  std::string output;
  JsonDebugStringState(GeneratedJsonAccessor::Singleton(), &output)
      .StructDebugString(message);
  return output;
}

std::string JsonMapDebugString(const google::protobuf::Message& message) {
  DynamicJsonAccessor accessor;
  accessor.InitializeStruct(message);
  std::string output;
  JsonDebugStringState(&accessor, &output).StructDebugString(message);
  return output;
}

namespace {

class JsonEqualsState final {
 public:
  explicit JsonEqualsState(absl::Nonnull<const JsonAccessor*> lhs_accessor,
                           absl::Nonnull<const JsonAccessor*> rhs_accessor)
      : lhs_accessor_(lhs_accessor), rhs_accessor_(rhs_accessor) {}

  bool ValueEqual(const google::protobuf::MessageLite& lhs,
                  const google::protobuf::MessageLite& rhs) {
    auto lhs_kind_case = lhs_accessor_->GetKindCase(lhs);
    if (lhs_kind_case == google::protobuf::Value::KIND_NOT_SET) {
      lhs_kind_case = google::protobuf::Value::kNullValue;
    }
    auto rhs_kind_case = rhs_accessor_->GetKindCase(rhs);
    if (rhs_kind_case == google::protobuf::Value::KIND_NOT_SET) {
      rhs_kind_case = google::protobuf::Value::kNullValue;
    }
    if (lhs_kind_case != rhs_kind_case) {
      return false;
    }
    switch (lhs_kind_case) {
      case google::protobuf::Value::KIND_NOT_SET:
        ABSL_UNREACHABLE();
      case google::protobuf::Value::kNullValue:
        return true;
      case google::protobuf::Value::kBoolValue:
        return lhs_accessor_->GetBoolValue(lhs) ==
               rhs_accessor_->GetBoolValue(rhs);
      case google::protobuf::Value::kNumberValue:
        return lhs_accessor_->GetNumberValue(lhs) ==
               rhs_accessor_->GetNumberValue(rhs);
      case google::protobuf::Value::kStringValue:
        return lhs_accessor_->GetStringValue(lhs, lhs_scratch_) ==
               rhs_accessor_->GetStringValue(rhs, rhs_scratch_);
      case google::protobuf::Value::kListValue:
        return ListValueEqual(lhs_accessor_->GetListValue(lhs),
                              rhs_accessor_->GetListValue(rhs));
      case google::protobuf::Value::kStructValue:
        return StructEqual(lhs_accessor_->GetStructValue(lhs),
                           rhs_accessor_->GetStructValue(rhs));
      default:
        // Should not get here, but if for some terrible reason
        // `google.protobuf.Value` is expanded, default to false.
        return false;
    }
  }

  bool ListValueEqual(const google::protobuf::MessageLite& lhs,
                      const google::protobuf::MessageLite& rhs) {
    const int lhs_size = lhs_accessor_->ValuesSize(lhs);
    const int rhs_size = rhs_accessor_->ValuesSize(rhs);
    if (lhs_size != rhs_size) {
      return false;
    }
    for (int i = 0; i < lhs_size; ++i) {
      if (!ValueEqual(lhs_accessor_->Values(lhs, i),
                      rhs_accessor_->Values(rhs, i))) {
        return false;
      }
    }
    return true;
  }

  bool StructEqual(const google::protobuf::MessageLite& lhs,
                   const google::protobuf::MessageLite& rhs) {
    const int lhs_size = lhs_accessor_->FieldsSize(lhs);
    const int rhs_size = rhs_accessor_->FieldsSize(rhs);
    if (lhs_size != rhs_size) {
      return false;
    }
    if (lhs_size == 0) {
      return true;
    }
    std::string lhs_key_scratch;
    well_known_types::StringValue lhs_key;
    absl::Nonnull<const google::protobuf::MessageLite*> lhs_value;
    auto lhs_iterator = lhs_accessor_->IterateFields(lhs);
    for (int i = 0; i < lhs_size; ++i) {
      std::tie(lhs_key, lhs_value) = lhs_iterator.Next(lhs_key_scratch);
      if (const auto* rhs_value = rhs_accessor_->FindField(
              rhs, absl::visit(
                       absl::Overload(
                           [](absl::string_view string) -> absl::string_view {
                             return string;
                           },
                           [&lhs_key_scratch](
                               const absl::Cord& cord) -> absl::string_view {
                             if (auto flat = cord.TryFlat(); flat) {
                               return *flat;
                             }
                             absl::CopyCordToString(cord, &lhs_key_scratch);
                             return absl::string_view(lhs_key_scratch);
                           }),
                       AsVariant(lhs_key)));
          rhs_value == nullptr || !ValueEqual(*lhs_value, *rhs_value)) {
        return false;
      }
    }
    return true;
  }

 private:
  const absl::Nonnull<const JsonAccessor*> lhs_accessor_;
  const absl::Nonnull<const JsonAccessor*> rhs_accessor_;
  std::string lhs_scratch_;
  std::string rhs_scratch_;
};

}  // namespace

bool JsonEquals(const google::protobuf::Value& lhs,
                const google::protobuf::Value& rhs) {
  return JsonEqualsState(GeneratedJsonAccessor::Singleton(),
                         GeneratedJsonAccessor::Singleton())
      .ValueEqual(lhs, rhs);
}

bool JsonEquals(const google::protobuf::Value& lhs,
                const google::protobuf::Message& rhs) {
  DynamicJsonAccessor rhs_accessor;
  rhs_accessor.InitializeValue(rhs);
  return JsonEqualsState(GeneratedJsonAccessor::Singleton(), &rhs_accessor)
      .ValueEqual(lhs, rhs);
}

bool JsonEquals(const google::protobuf::Message& lhs,
                const google::protobuf::Value& rhs) {
  DynamicJsonAccessor lhs_accessor;
  lhs_accessor.InitializeValue(lhs);
  return JsonEqualsState(&lhs_accessor, GeneratedJsonAccessor::Singleton())
      .ValueEqual(lhs, rhs);
}

bool JsonEquals(const google::protobuf::Message& lhs, const google::protobuf::Message& rhs) {
  DynamicJsonAccessor lhs_accessor;
  lhs_accessor.InitializeValue(lhs);
  DynamicJsonAccessor rhs_accessor;
  rhs_accessor.InitializeValue(rhs);
  return JsonEqualsState(&lhs_accessor, &rhs_accessor).ValueEqual(lhs, rhs);
}

bool JsonEquals(const google::protobuf::MessageLite& lhs,
                const google::protobuf::MessageLite& rhs) {
  const auto* lhs_generated =
      google::protobuf::DynamicCastMessage<google::protobuf::Value>(&lhs);
  const auto* rhs_generated =
      google::protobuf::DynamicCastMessage<google::protobuf::Value>(&rhs);
  if (lhs_generated && rhs_generated) {
    return JsonEquals(*lhs_generated, *rhs_generated);
  }
  if (lhs_generated) {
    return JsonEquals(*lhs_generated,
                      google::protobuf::DownCastMessage<google::protobuf::Message>(rhs));
  }
  if (rhs_generated) {
    return JsonEquals(google::protobuf::DownCastMessage<google::protobuf::Message>(lhs),
                      *rhs_generated);
  }
  return JsonEquals(google::protobuf::DownCastMessage<google::protobuf::Message>(lhs),
                    google::protobuf::DownCastMessage<google::protobuf::Message>(rhs));
}

bool JsonListEquals(const google::protobuf::ListValue& lhs,
                    const google::protobuf::ListValue& rhs) {
  return JsonEqualsState(GeneratedJsonAccessor::Singleton(),
                         GeneratedJsonAccessor::Singleton())
      .ListValueEqual(lhs, rhs);
}

bool JsonListEquals(const google::protobuf::ListValue& lhs,
                    const google::protobuf::Message& rhs) {
  DynamicJsonAccessor rhs_accessor;
  rhs_accessor.InitializeListValue(rhs);
  return JsonEqualsState(GeneratedJsonAccessor::Singleton(), &rhs_accessor)
      .ListValueEqual(lhs, rhs);
}

bool JsonListEquals(const google::protobuf::Message& lhs,
                    const google::protobuf::ListValue& rhs) {
  DynamicJsonAccessor lhs_accessor;
  lhs_accessor.InitializeListValue(lhs);
  return JsonEqualsState(&lhs_accessor, GeneratedJsonAccessor::Singleton())
      .ListValueEqual(lhs, rhs);
}

bool JsonListEquals(const google::protobuf::Message& lhs, const google::protobuf::Message& rhs) {
  DynamicJsonAccessor lhs_accessor;
  lhs_accessor.InitializeListValue(lhs);
  DynamicJsonAccessor rhs_accessor;
  rhs_accessor.InitializeListValue(rhs);
  return JsonEqualsState(&lhs_accessor, &rhs_accessor).ListValueEqual(lhs, rhs);
}

bool JsonListEquals(const google::protobuf::MessageLite& lhs,
                    const google::protobuf::MessageLite& rhs) {
  const auto* lhs_generated =
      google::protobuf::DynamicCastMessage<google::protobuf::ListValue>(&lhs);
  const auto* rhs_generated =
      google::protobuf::DynamicCastMessage<google::protobuf::ListValue>(&rhs);
  if (lhs_generated && rhs_generated) {
    return JsonListEquals(*lhs_generated, *rhs_generated);
  }
  if (lhs_generated) {
    return JsonListEquals(*lhs_generated,
                          google::protobuf::DownCastMessage<google::protobuf::Message>(rhs));
  }
  if (rhs_generated) {
    return JsonListEquals(google::protobuf::DownCastMessage<google::protobuf::Message>(lhs),
                          *rhs_generated);
  }
  return JsonListEquals(google::protobuf::DownCastMessage<google::protobuf::Message>(lhs),
                        google::protobuf::DownCastMessage<google::protobuf::Message>(rhs));
}

bool JsonMapEquals(const google::protobuf::Struct& lhs,
                   const google::protobuf::Struct& rhs) {
  return JsonEqualsState(GeneratedJsonAccessor::Singleton(),
                         GeneratedJsonAccessor::Singleton())
      .StructEqual(lhs, rhs);
}

bool JsonMapEquals(const google::protobuf::Struct& lhs,
                   const google::protobuf::Message& rhs) {
  DynamicJsonAccessor rhs_accessor;
  rhs_accessor.InitializeStruct(rhs);
  return JsonEqualsState(GeneratedJsonAccessor::Singleton(), &rhs_accessor)
      .StructEqual(lhs, rhs);
}

bool JsonMapEquals(const google::protobuf::Message& lhs,
                   const google::protobuf::Struct& rhs) {
  DynamicJsonAccessor lhs_accessor;
  lhs_accessor.InitializeStruct(lhs);
  return JsonEqualsState(&lhs_accessor, GeneratedJsonAccessor::Singleton())
      .StructEqual(lhs, rhs);
}

bool JsonMapEquals(const google::protobuf::Message& lhs, const google::protobuf::Message& rhs) {
  DynamicJsonAccessor lhs_accessor;
  lhs_accessor.InitializeStruct(lhs);
  DynamicJsonAccessor rhs_accessor;
  rhs_accessor.InitializeStruct(rhs);
  return JsonEqualsState(&lhs_accessor, &rhs_accessor).StructEqual(lhs, rhs);
}

bool JsonMapEquals(const google::protobuf::MessageLite& lhs,
                   const google::protobuf::MessageLite& rhs) {
  const auto* lhs_generated =
      google::protobuf::DynamicCastMessage<google::protobuf::Struct>(&lhs);
  const auto* rhs_generated =
      google::protobuf::DynamicCastMessage<google::protobuf::Struct>(&rhs);
  if (lhs_generated && rhs_generated) {
    return JsonMapEquals(*lhs_generated, *rhs_generated);
  }
  if (lhs_generated) {
    return JsonMapEquals(*lhs_generated,
                         google::protobuf::DownCastMessage<google::protobuf::Message>(rhs));
  }
  if (rhs_generated) {
    return JsonMapEquals(google::protobuf::DownCastMessage<google::protobuf::Message>(lhs),
                         *rhs_generated);
  }
  return JsonMapEquals(google::protobuf::DownCastMessage<google::protobuf::Message>(lhs),
                       google::protobuf::DownCastMessage<google::protobuf::Message>(rhs));
}

namespace {

struct DynamicProtoJsonToNativeJsonState {
  ValueReflection value_reflection;
  ListValueReflection list_value_reflection;
  StructReflection struct_reflection;
  std::string scratch;

  absl::Status Initialize(const google::protobuf::Message& proto) {
    CEL_RETURN_IF_ERROR(value_reflection.Initialize(proto.GetDescriptor()));
    CEL_RETURN_IF_ERROR(list_value_reflection.Initialize(
        value_reflection.GetListValueDescriptor()));
    CEL_RETURN_IF_ERROR(
        struct_reflection.Initialize(value_reflection.GetStructDescriptor()));
    return absl::OkStatus();
  }

  absl::Status InitializeListValue(const google::protobuf::Message& proto) {
    CEL_RETURN_IF_ERROR(
        list_value_reflection.Initialize(proto.GetDescriptor()));
    CEL_RETURN_IF_ERROR(value_reflection.Initialize(
        list_value_reflection.GetValueDescriptor()));
    CEL_RETURN_IF_ERROR(
        struct_reflection.Initialize(value_reflection.GetStructDescriptor()));
    return absl::OkStatus();
  }

  absl::Status InitializeStruct(const google::protobuf::Message& proto) {
    CEL_RETURN_IF_ERROR(struct_reflection.Initialize(proto.GetDescriptor()));
    CEL_RETURN_IF_ERROR(
        value_reflection.Initialize(struct_reflection.GetValueDescriptor()));
    CEL_RETURN_IF_ERROR(list_value_reflection.Initialize(
        value_reflection.GetListValueDescriptor()));
    return absl::OkStatus();
  }

  absl::StatusOr<Json> ToNativeJson(const google::protobuf::Message& proto) {
    const auto kind_case = value_reflection.GetKindCase(proto);
    switch (kind_case) {
      case google::protobuf::Value::KIND_NOT_SET:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::Value::kNullValue:
        return kJsonNull;
      case google::protobuf::Value::kBoolValue:
        return JsonBool(value_reflection.GetBoolValue(proto));
      case google::protobuf::Value::kNumberValue:
        return JsonNumber(value_reflection.GetNumberValue(proto));
      case google::protobuf::Value::kStringValue:
        return absl::visit(
            absl::Overload(
                [](absl::string_view string) -> JsonString {
                  return JsonString(string);
                },
                [](absl::Cord&& cord) -> JsonString { return cord; }),
            AsVariant(value_reflection.GetStringValue(proto, scratch)));
      case google::protobuf::Value::kListValue:
        return ToNativeJsonList(value_reflection.GetListValue(proto));
      case google::protobuf::Value::kStructValue:
        return ToNativeJsonMap(value_reflection.GetStructValue(proto));
      default:
        return absl::InvalidArgumentError(
            absl::StrCat("unexpected value kind case: ", kind_case));
    }
  }

  absl::StatusOr<JsonArray> ToNativeJsonList(const google::protobuf::Message& proto) {
    const int proto_size = list_value_reflection.ValuesSize(proto);
    JsonArrayBuilder builder;
    builder.reserve(static_cast<size_t>(proto_size));
    for (int i = 0; i < proto_size; ++i) {
      CEL_ASSIGN_OR_RETURN(
          auto value, ToNativeJson(list_value_reflection.Values(proto, i)));
      builder.push_back(std::move(value));
    }
    return std::move(builder).Build();
  }

  absl::StatusOr<JsonObject> ToNativeJsonMap(const google::protobuf::Message& proto) {
    const int proto_size = struct_reflection.FieldsSize(proto);
    JsonObjectBuilder builder;
    builder.reserve(static_cast<size_t>(proto_size));
    auto struct_proto_begin = struct_reflection.BeginFields(proto);
    auto struct_proto_end = struct_reflection.EndFields(proto);
    for (; struct_proto_begin != struct_proto_end; ++struct_proto_begin) {
      CEL_ASSIGN_OR_RETURN(
          auto value,
          ToNativeJson(struct_proto_begin.GetValueRef().GetMessageValue()));
      builder.insert_or_assign(
          JsonString(struct_proto_begin.GetKey().GetStringValue()),
          std::move(value));
    }
    return std::move(builder).Build();
  }
};

}  // namespace

absl::StatusOr<Json> ProtoJsonToNativeJson(const google::protobuf::Message& proto) {
  DynamicProtoJsonToNativeJsonState state;
  CEL_RETURN_IF_ERROR(state.Initialize(proto));
  return state.ToNativeJson(proto);
}

absl::StatusOr<Json> ProtoJsonToNativeJson(
    const google::protobuf::Value& proto) {
  const auto kind_case = ValueReflection::GetKindCase(proto);
  switch (kind_case) {
    case google::protobuf::Value::KIND_NOT_SET:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Value::kNullValue:
      return kJsonNull;
    case google::protobuf::Value::kBoolValue:
      return JsonBool(ValueReflection::GetBoolValue(proto));
    case google::protobuf::Value::kNumberValue:
      return JsonNumber(ValueReflection::GetNumberValue(proto));
    case google::protobuf::Value::kStringValue:
      return JsonString(ValueReflection::GetStringValue(proto));
    case google::protobuf::Value::kListValue:
      return ProtoJsonListToNativeJsonList(
          ValueReflection::GetListValue(proto));
    case google::protobuf::Value::kStructValue:
      return ProtoJsonMapToNativeJsonMap(
          ValueReflection::GetStructValue(proto));
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("unexpected value kind case: ", kind_case));
  }
}
absl::StatusOr<JsonArray> ProtoJsonListToNativeJsonList(
    const google::protobuf::Message& proto) {
  DynamicProtoJsonToNativeJsonState state;
  CEL_RETURN_IF_ERROR(state.InitializeListValue(proto));
  return state.ToNativeJsonList(proto);
}

absl::StatusOr<JsonArray> ProtoJsonListToNativeJsonList(
    const google::protobuf::ListValue& proto) {
  const int proto_size = ListValueReflection::ValuesSize(proto);
  JsonArrayBuilder builder;
  builder.reserve(static_cast<size_t>(proto_size));
  for (int i = 0; i < proto_size; ++i) {
    CEL_ASSIGN_OR_RETURN(
        auto value,
        ProtoJsonToNativeJson(ListValueReflection::Values(proto, i)));
    builder.push_back(std::move(value));
  }
  return std::move(builder).Build();
}

absl::StatusOr<JsonObject> ProtoJsonMapToNativeJsonMap(
    const google::protobuf::Message& proto) {
  DynamicProtoJsonToNativeJsonState state;
  CEL_RETURN_IF_ERROR(state.InitializeStruct(proto));
  return state.ToNativeJsonMap(proto);
}

absl::StatusOr<JsonObject> ProtoJsonMapToNativeJsonMap(
    const google::protobuf::Struct& proto) {
  const int proto_size = StructReflection::FieldsSize(proto);
  JsonObjectBuilder builder;
  builder.reserve(static_cast<size_t>(proto_size));
  auto struct_proto_begin = StructReflection::BeginFields(proto);
  auto struct_proto_end = StructReflection::EndFields(proto);
  for (; struct_proto_begin != struct_proto_end; ++struct_proto_begin) {
    CEL_ASSIGN_OR_RETURN(auto value,
                         ProtoJsonToNativeJson(struct_proto_begin->second));
    builder.insert_or_assign(JsonString(struct_proto_begin->first),
                             std::move(value));
  }
  return std::move(builder).Build();
}

namespace {

class JsonMutator {
 public:
  virtual ~JsonMutator() = default;

  virtual void SetNullValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const = 0;

  virtual void SetBoolValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                            bool value) const = 0;

  virtual void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                              double value) const = 0;

  virtual void SetStringValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                              const absl::Cord& value) const = 0;

  virtual absl::Nonnull<google::protobuf::MessageLite*> MutableListValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const = 0;

  virtual void ReserveValues(absl::Nonnull<google::protobuf::MessageLite*> message,
                             int capacity) const = 0;

  virtual absl::Nonnull<google::protobuf::MessageLite*> AddValues(
      absl::Nonnull<google::protobuf::MessageLite*> message) const = 0;

  virtual absl::Nonnull<google::protobuf::MessageLite*> MutableStructValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const = 0;

  virtual absl::Nonnull<google::protobuf::MessageLite*> InsertField(
      absl::Nonnull<google::protobuf::MessageLite*> message,
      absl::string_view name) const = 0;
};

class GeneratedJsonMutator final : public JsonMutator {
 public:
  static absl::Nonnull<const GeneratedJsonMutator*> Singleton() {
    static const absl::NoDestructor<GeneratedJsonMutator> instance;
    return &*instance;
  }

  void SetNullValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const override {
    ValueReflection::SetNullValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message));
  }

  void SetBoolValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                    bool value) const override {
    ValueReflection::SetBoolValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message), value);
  }

  void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      double value) const override {
    ValueReflection::SetNumberValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message), value);
  }

  void SetStringValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      const absl::Cord& value) const override {
    ValueReflection::SetStringValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message), value);
  }

  absl::Nonnull<google::protobuf::MessageLite*> MutableListValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const override {
    return ValueReflection::MutableListValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message));
  }

  void ReserveValues(absl::Nonnull<google::protobuf::MessageLite*> message,
                     int capacity) const override {
    ListValueReflection::ReserveValues(
        google::protobuf::DownCastMessage<google::protobuf::ListValue>(message),
        capacity);
  }

  absl::Nonnull<google::protobuf::MessageLite*> AddValues(
      absl::Nonnull<google::protobuf::MessageLite*> message) const override {
    return ListValueReflection::AddValues(
        google::protobuf::DownCastMessage<google::protobuf::ListValue>(message));
  }

  absl::Nonnull<google::protobuf::MessageLite*> MutableStructValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const override {
    return ValueReflection::MutableStructValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message));
  }

  absl::Nonnull<google::protobuf::MessageLite*> InsertField(
      absl::Nonnull<google::protobuf::MessageLite*> message,
      absl::string_view name) const override {
    return StructReflection::InsertField(
        google::protobuf::DownCastMessage<google::protobuf::Struct>(message), name);
  }
};

class DynamicJsonMutator final : public JsonMutator {
 public:
  absl::Status InitializeValue(
      absl::Nonnull<const google::protobuf::Descriptor*> descriptor) {
    CEL_RETURN_IF_ERROR(value_reflection_.Initialize(descriptor));
    CEL_RETURN_IF_ERROR(list_value_reflection_.Initialize(
        value_reflection_.GetListValueDescriptor()));
    CEL_RETURN_IF_ERROR(
        struct_reflection_.Initialize(value_reflection_.GetStructDescriptor()));
    return absl::OkStatus();
  }

  absl::Status InitializeListValue(
      absl::Nonnull<const google::protobuf::Descriptor*> descriptor) {
    CEL_RETURN_IF_ERROR(list_value_reflection_.Initialize(descriptor));
    CEL_RETURN_IF_ERROR(value_reflection_.Initialize(
        list_value_reflection_.GetValueDescriptor()));
    CEL_RETURN_IF_ERROR(
        struct_reflection_.Initialize(value_reflection_.GetStructDescriptor()));
    return absl::OkStatus();
  }

  absl::Status InitializeStruct(
      absl::Nonnull<const google::protobuf::Descriptor*> descriptor) {
    CEL_RETURN_IF_ERROR(struct_reflection_.Initialize(descriptor));
    CEL_RETURN_IF_ERROR(
        value_reflection_.Initialize(struct_reflection_.GetValueDescriptor()));
    CEL_RETURN_IF_ERROR(list_value_reflection_.Initialize(
        value_reflection_.GetListValueDescriptor()));
    return absl::OkStatus();
  }

  void SetNullValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const override {
    value_reflection_.SetNullValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message));
  }

  void SetBoolValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                    bool value) const override {
    value_reflection_.SetBoolValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message), value);
  }

  void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      double value) const override {
    value_reflection_.SetNumberValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message), value);
  }

  void SetStringValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      const absl::Cord& value) const override {
    value_reflection_.SetStringValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message), value);
  }

  absl::Nonnull<google::protobuf::MessageLite*> MutableListValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const override {
    return value_reflection_.MutableListValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message));
  }

  void ReserveValues(absl::Nonnull<google::protobuf::MessageLite*> message,
                     int capacity) const override {
    list_value_reflection_.ReserveValues(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message), capacity);
  }

  absl::Nonnull<google::protobuf::MessageLite*> AddValues(
      absl::Nonnull<google::protobuf::MessageLite*> message) const override {
    return list_value_reflection_.AddValues(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message));
  }

  absl::Nonnull<google::protobuf::MessageLite*> MutableStructValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const override {
    return value_reflection_.MutableStructValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message));
  }

  absl::Nonnull<google::protobuf::MessageLite*> InsertField(
      absl::Nonnull<google::protobuf::MessageLite*> message,
      absl::string_view name) const override {
    return struct_reflection_.InsertField(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message), name);
  }

 private:
  ValueReflection value_reflection_;
  ListValueReflection list_value_reflection_;
  StructReflection struct_reflection_;
};

class NativeJsonToProtoJsonState {
 public:
  explicit NativeJsonToProtoJsonState(absl::Nonnull<const JsonMutator*> mutator)
      : mutator_(mutator) {}

  absl::Status ToProtoJson(const Json& json,
                           absl::Nonnull<google::protobuf::MessageLite*> proto) {
    return absl::visit(
        absl::Overload(
            [&](JsonNull) -> absl::Status {
              mutator_->SetNullValue(proto);
              return absl::OkStatus();
            },
            [&](JsonBool value) -> absl::Status {
              mutator_->SetBoolValue(proto, value);
              return absl::OkStatus();
            },
            [&](JsonNumber value) -> absl::Status {
              mutator_->SetNumberValue(proto, value);
              return absl::OkStatus();
            },
            [&](const JsonString& value) -> absl::Status {
              mutator_->SetStringValue(proto, value);
              return absl::OkStatus();
            },
            [&](const JsonArray& value) -> absl::Status {
              return ToProtoJsonList(value, mutator_->MutableListValue(proto));
            },
            [&](const JsonObject& value) -> absl::Status {
              return ToProtoJsonMap(value, mutator_->MutableStructValue(proto));
            }),
        json);
  }

  absl::Status ToProtoJsonList(const JsonArray& json,
                               absl::Nonnull<google::protobuf::MessageLite*> proto) {
    mutator_->ReserveValues(proto, static_cast<int>(json.size()));
    for (const auto& element : json) {
      CEL_RETURN_IF_ERROR(ToProtoJson(element, mutator_->AddValues(proto)));
    }
    return absl::OkStatus();
  }

  absl::Status ToProtoJsonMap(const JsonObject& json,
                              absl::Nonnull<google::protobuf::MessageLite*> proto) {
    for (const auto& entry : json) {
      CEL_RETURN_IF_ERROR(ToProtoJson(
          entry.second,
          mutator_->InsertField(proto, static_cast<std::string>(entry.first))));
    }
    return absl::OkStatus();
  }

 private:
  absl::Nonnull<const JsonMutator*> const mutator_;
};

}  // namespace

absl::Status NativeJsonToProtoJson(const Json& json,
                                   absl::Nonnull<google::protobuf::Message*> proto) {
  DynamicJsonMutator mutator;
  CEL_RETURN_IF_ERROR(mutator.InitializeValue(proto->GetDescriptor()));
  return NativeJsonToProtoJsonState(&mutator).ToProtoJson(json, proto);
}

absl::Status NativeJsonToProtoJson(
    const Json& json, absl::Nonnull<google::protobuf::Value*> proto) {
  return NativeJsonToProtoJsonState(GeneratedJsonMutator::Singleton())
      .ToProtoJson(json, proto);
}

absl::Status NativeJsonListToProtoJsonList(
    const JsonArray& json, absl::Nonnull<google::protobuf::Message*> proto) {
  DynamicJsonMutator mutator;
  CEL_RETURN_IF_ERROR(mutator.InitializeListValue(proto->GetDescriptor()));
  return NativeJsonToProtoJsonState(&mutator).ToProtoJsonList(json, proto);
}

absl::Status NativeJsonListToProtoJsonList(
    const JsonArray& json, absl::Nonnull<google::protobuf::ListValue*> proto) {
  return NativeJsonToProtoJsonState(GeneratedJsonMutator::Singleton())
      .ToProtoJsonList(json, proto);
}

absl::Status NativeJsonMapToProtoJsonMap(
    const JsonObject& json, absl::Nonnull<google::protobuf::Message*> proto) {
  DynamicJsonMutator mutator;
  CEL_RETURN_IF_ERROR(mutator.InitializeStruct(proto->GetDescriptor()));
  return NativeJsonToProtoJsonState(&mutator).ToProtoJsonMap(json, proto);
}

absl::Status NativeJsonMapToProtoJsonMap(
    const JsonObject& json, absl::Nonnull<google::protobuf::Struct*> proto) {
  return NativeJsonToProtoJsonState(GeneratedJsonMutator::Singleton())
      .ToProtoJsonMap(json, proto);
}

}  // namespace cel::internal
