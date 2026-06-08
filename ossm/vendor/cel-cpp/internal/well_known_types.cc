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

#include "internal/well_known_types.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/field_mask.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "google/protobuf/descriptor.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/call_once.h"
#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/functional/overload.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/time/time.h"
#include "absl/types/variant.h"
#include "common/json.h"
#include "common/memory.h"
#include "extensions/protobuf/internal/map_reflection.h"
#include "internal/protobuf_runtime_version.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"
#include "google/protobuf/reflection.h"
#include "google/protobuf/util/time_util.h"

namespace cel::well_known_types {

namespace {

using ::google::protobuf::Descriptor;
using ::google::protobuf::DescriptorPool;
using ::google::protobuf::EnumDescriptor;
using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::OneofDescriptor;
using ::google::protobuf::util::TimeUtil;

using CppStringType = ::google::protobuf::FieldDescriptor::CppStringType;

absl::string_view FlatStringValue(
    const StringValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND,
    std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return absl::visit(
      absl::Overload(
          [](absl::string_view string) -> absl::string_view { return string; },
          [&](const absl::Cord& cord) -> absl::string_view {
            if (auto flat = cord.TryFlat(); flat) {
              return *flat;
            }
            scratch = static_cast<std::string>(cord);
            return scratch;
          }),
      AsVariant(value));
}

StringValue CopyStringValue(const StringValue& value,
                            std::string& scratch
                                ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return absl::visit(
      absl::Overload(
          [&](absl::string_view string) -> StringValue {
            if (string.data() != scratch.data()) {
              scratch.assign(string.data(), string.size());
              return scratch;
            }
            return string;
          },
          [](const absl::Cord& cord) -> StringValue { return cord; }),
      AsVariant(value));
}

BytesValue CopyBytesValue(const BytesValue& value,
                          std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return absl::visit(
      absl::Overload(
          [&](absl::string_view string) -> BytesValue {
            if (string.data() != scratch.data()) {
              scratch.assign(string.data(), string.size());
              return scratch;
            }
            return string;
          },
          [](const absl::Cord& cord) -> BytesValue { return cord; }),
      AsVariant(value));
}

google::protobuf::Reflection::ScratchSpace& GetScratchSpace() {
  static absl::NoDestructor<google::protobuf::Reflection::ScratchSpace> scratch_space;
  return *scratch_space;
}

template <typename Variant>
Variant GetStringField(const google::protobuf::Reflection* absl_nonnull reflection,
                       const google::protobuf::Message& message,
                       const FieldDescriptor* absl_nonnull field,
                       CppStringType string_type,
                       std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  ABSL_DCHECK(field->cpp_string_type() == string_type);
  switch (string_type) {
    case CppStringType::kCord:
      return reflection->GetCord(message, field);
    case CppStringType::kView:
      ABSL_FALLTHROUGH_INTENDED;
    case CppStringType::kString:
      // Message is guaranteed to be storing as some sort of contiguous array of
      // bytes, there is no need to copy. But unfortunately `GetStringView`
      // forces taking scratch space.
      return reflection->GetStringView(message, field, GetScratchSpace());
    default:
      return absl::string_view(
          reflection->GetStringReference(message, field, &scratch));
  }
}

template <typename Variant>
Variant GetStringField(const google::protobuf::Message& message,
                       const FieldDescriptor* absl_nonnull field,
                       CppStringType string_type,
                       std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return GetStringField<Variant>(message.GetReflection(), message, field,
                                 string_type, scratch);
}

template <typename Variant>
Variant GetRepeatedStringField(
    const google::protobuf::Reflection* absl_nonnull reflection,
    const google::protobuf::Message& message, const FieldDescriptor* absl_nonnull field,
    CppStringType string_type, int index,
    std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  ABSL_DCHECK(field->cpp_string_type() == string_type);
  switch (string_type) {
    case CppStringType::kView:
      ABSL_FALLTHROUGH_INTENDED;
    case CppStringType::kString:
      // Message is guaranteed to be storing as some sort of contiguous array of
      // bytes, there is no need to copy. But unfortunately `GetStringView`
      // forces taking scratch space.
      return reflection->GetRepeatedStringView(message, field, index,
                                               GetScratchSpace());
    default:
      return absl::string_view(reflection->GetRepeatedStringReference(
          message, field, index, &scratch));
  }
}

template <typename Variant>
Variant GetRepeatedStringField(
    const google::protobuf::Message& message, const FieldDescriptor* absl_nonnull field,
    CppStringType string_type, int index,
    std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return GetRepeatedStringField<Variant>(message.GetReflection(), message,
                                         field, string_type, index, scratch);
}

absl::StatusOr<const Descriptor* absl_nonnull> GetMessageTypeByName(
    const DescriptorPool* absl_nonnull pool, absl::string_view name) {
  const auto* descriptor = pool->FindMessageTypeByName(name);
  if (ABSL_PREDICT_FALSE(descriptor == nullptr)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "descriptor missing for protocol buffer message well known type: ",
        name));
  }
  return descriptor;
}

absl::StatusOr<const EnumDescriptor* absl_nonnull> GetEnumTypeByName(
    const DescriptorPool* absl_nonnull pool, absl::string_view name) {
  const auto* descriptor = pool->FindEnumTypeByName(name);
  if (ABSL_PREDICT_FALSE(descriptor == nullptr)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "descriptor missing for protocol buffer enum well known type: ", name));
  }
  return descriptor;
}

absl::StatusOr<const OneofDescriptor* absl_nonnull> GetOneofByName(
    const Descriptor* absl_nonnull descriptor, absl::string_view name) {
  const auto* oneof = descriptor->FindOneofByName(name);
  if (ABSL_PREDICT_FALSE(oneof == nullptr)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "oneof missing for protocol buffer message well known type: ",
        descriptor->full_name(), ".", name));
  }
  return oneof;
}

absl::StatusOr<const FieldDescriptor* absl_nonnull> GetFieldByNumber(
    const Descriptor* absl_nonnull descriptor, int32_t number) {
  const auto* field = descriptor->FindFieldByNumber(number);
  if (ABSL_PREDICT_FALSE(field == nullptr)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "field missing for protocol buffer message well known type: ",
        descriptor->full_name(), ".", number));
  }
  return field;
}

absl::Status CheckFieldType(const FieldDescriptor* absl_nonnull field,
                            FieldDescriptor::Type type) {
  if (ABSL_PREDICT_FALSE(field->type() != type)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "unexpected field type for protocol buffer message well known type: ",
        field->full_name(), " ", field->type_name()));
  }
  return absl::OkStatus();
}

absl::Status CheckFieldCppType(const FieldDescriptor* absl_nonnull field,
                               FieldDescriptor::CppType cpp_type) {
  if (ABSL_PREDICT_FALSE(field->cpp_type() != cpp_type)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "unexpected field type for protocol buffer message well known type: ",
        field->full_name(), " ", field->cpp_type_name()));
  }
  return absl::OkStatus();
}

absl::string_view LabelToString(FieldDescriptor::Label label) {
  switch (label) {
    case FieldDescriptor::LABEL_REPEATED:
      return "REPEATED";
    case FieldDescriptor::LABEL_REQUIRED:
      return "REQUIRED";
    case FieldDescriptor::LABEL_OPTIONAL:
      return "OPTIONAL";
    default:
      return "ERROR";
  }
}

absl::Status CheckFieldCardinality(const FieldDescriptor* absl_nonnull field,
                                   FieldDescriptor::Label label) {
  if (ABSL_PREDICT_FALSE(field->label() != label)) {
    return absl::InvalidArgumentError(
        absl::StrCat("unexpected field cardinality for protocol buffer message "
                     "well known type: ",
                     field->full_name(), " ", LabelToString(field->label())));
  }
  return absl::OkStatus();
}

absl::string_view WellKnownTypeToString(
    Descriptor::WellKnownType well_known_type) {
  switch (well_known_type) {
    case Descriptor::WELLKNOWNTYPE_BOOLVALUE:
      return "BOOLVALUE";
    case Descriptor::WELLKNOWNTYPE_INT32VALUE:
      return "INT32VALUE";
    case Descriptor::WELLKNOWNTYPE_INT64VALUE:
      return "INT64VALUE";
    case Descriptor::WELLKNOWNTYPE_UINT32VALUE:
      return "UINT32VALUE";
    case Descriptor::WELLKNOWNTYPE_UINT64VALUE:
      return "UINT64VALUE";
    case Descriptor::WELLKNOWNTYPE_FLOATVALUE:
      return "FLOATVALUE";
    case Descriptor::WELLKNOWNTYPE_DOUBLEVALUE:
      return "DOUBLEVALUE";
    case Descriptor::WELLKNOWNTYPE_BYTESVALUE:
      return "BYTESVALUE";
    case Descriptor::WELLKNOWNTYPE_STRINGVALUE:
      return "STRINGVALUE";
    case Descriptor::WELLKNOWNTYPE_ANY:
      return "ANY";
    case Descriptor::WELLKNOWNTYPE_DURATION:
      return "DURATION";
    case Descriptor::WELLKNOWNTYPE_TIMESTAMP:
      return "TIMESTAMP";
    case Descriptor::WELLKNOWNTYPE_VALUE:
      return "VALUE";
    case Descriptor::WELLKNOWNTYPE_LISTVALUE:
      return "LISTVALUE";
    case Descriptor::WELLKNOWNTYPE_STRUCT:
      return "STRUCT";
    case Descriptor::WELLKNOWNTYPE_FIELDMASK:
      return "FIELDMASK";
    default:
      return "ERROR";
  }
}

absl::Status CheckWellKnownType(const Descriptor* absl_nonnull descriptor,
                                Descriptor::WellKnownType well_known_type) {
  if (ABSL_PREDICT_FALSE(descriptor->well_known_type() != well_known_type)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "expected message to be well known type: ", descriptor->full_name(),
        " ", WellKnownTypeToString(descriptor->well_known_type())));
  }
  return absl::OkStatus();
}

absl::Status CheckFieldWellKnownType(
    const FieldDescriptor* absl_nonnull field,
    Descriptor::WellKnownType well_known_type) {
  ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_MESSAGE);
  if (ABSL_PREDICT_FALSE(field->message_type()->well_known_type() !=
                         well_known_type)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "expected message field to be well known type for protocol buffer "
        "message well known type: ",
        field->full_name(), " ",
        WellKnownTypeToString(field->message_type()->well_known_type())));
  }
  return absl::OkStatus();
}

absl::Status CheckFieldOneof(const FieldDescriptor* absl_nonnull field,
                             const OneofDescriptor* absl_nonnull oneof,
                             int index) {
  if (ABSL_PREDICT_FALSE(field->containing_oneof() != oneof)) {
    return absl::InvalidArgumentError(
        absl::StrCat("expected field to be member of oneof for protocol buffer "
                     "message well known type: ",
                     field->full_name()));
  }
  if (ABSL_PREDICT_FALSE(field->index_in_oneof() != index)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "expected field to have index in oneof of ", index,
        " for protocol buffer "
        "message well known type: ",
        field->full_name(), " oneof_index=", field->index_in_oneof()));
  }
  return absl::OkStatus();
}

absl::Status CheckMapField(const FieldDescriptor* absl_nonnull field) {
  if (ABSL_PREDICT_FALSE(!field->is_map())) {
    return absl::InvalidArgumentError(
        absl::StrCat("expected field to be map for protocol buffer "
                     "message well known type: ",
                     field->full_name()));
  }
  return absl::OkStatus();
}

}  // namespace

bool StringValue::ConsumePrefix(absl::string_view prefix) {
  return absl::visit(absl::Overload(
                         [&](absl::string_view& value) {
                           return absl::ConsumePrefix(&value, prefix);
                         },
                         [&](absl::Cord& cord) {
                           if (cord.StartsWith(prefix)) {
                             cord.RemovePrefix(prefix.size());
                             return true;
                           }
                           return false;
                         }),
                     AsVariant(*this));
}

StringValue GetStringField(const google::protobuf::Reflection* absl_nonnull reflection,
                           const google::protobuf::Message& message,
                           const FieldDescriptor* absl_nonnull field,
                           std::string& scratch) {
  ABSL_DCHECK_EQ(reflection, message.GetReflection());
  ABSL_DCHECK(!field->is_map() && !field->is_repeated());
  ABSL_DCHECK_EQ(field->type(), FieldDescriptor::TYPE_STRING);
  ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_STRING);
  return GetStringField<StringValue>(reflection, message, field,
                                     field->cpp_string_type(), scratch);
}

BytesValue GetBytesField(const google::protobuf::Reflection* absl_nonnull reflection,
                         const google::protobuf::Message& message,
                         const FieldDescriptor* absl_nonnull field,
                         std::string& scratch) {
  ABSL_DCHECK_EQ(reflection, message.GetReflection());
  ABSL_DCHECK(!field->is_map() && !field->is_repeated());
  ABSL_DCHECK_EQ(field->type(), FieldDescriptor::TYPE_BYTES);
  ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_STRING);
  return GetStringField<BytesValue>(reflection, message, field,
                                    field->cpp_string_type(), scratch);
}

StringValue GetRepeatedStringField(
    const google::protobuf::Reflection* absl_nonnull reflection,
    const google::protobuf::Message& message, const FieldDescriptor* absl_nonnull field,
    int index, std::string& scratch) {
  ABSL_DCHECK_EQ(reflection, message.GetReflection());
  ABSL_DCHECK(!field->is_map() && field->is_repeated());
  ABSL_DCHECK_EQ(field->type(), FieldDescriptor::TYPE_STRING);
  ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_STRING);
  return GetRepeatedStringField<StringValue>(
      reflection, message, field, field->cpp_string_type(), index, scratch);
}

BytesValue GetRepeatedBytesField(
    const google::protobuf::Reflection* absl_nonnull reflection,
    const google::protobuf::Message& message, const FieldDescriptor* absl_nonnull field,
    int index, std::string& scratch) {
  ABSL_DCHECK_EQ(reflection, message.GetReflection());
  ABSL_DCHECK(!field->is_map() && field->is_repeated());
  ABSL_DCHECK_EQ(field->type(), FieldDescriptor::TYPE_BYTES);
  ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_STRING);
  return GetRepeatedStringField<BytesValue>(
      reflection, message, field, field->cpp_string_type(), index, scratch);
}

absl::Status NullValueReflection::Initialize(
    const DescriptorPool* absl_nonnull pool) {
  CEL_ASSIGN_OR_RETURN(const auto* descriptor,
                       GetEnumTypeByName(pool, "google.protobuf.NullValue"));
  return Initialize(descriptor);
}

absl::Status NullValueReflection::Initialize(
    const EnumDescriptor* absl_nonnull descriptor) {
  if (descriptor_ != descriptor) {
    if (ABSL_PREDICT_FALSE(descriptor->full_name() !=
                           "google.protobuf.NullValue")) {
      return absl::InvalidArgumentError(absl::StrCat(
          "expected enum to be well known type: ", descriptor->full_name(),
          " google.protobuf.NullValue"));
    }
    descriptor_ = nullptr;
    value_ = descriptor->FindValueByNumber(0);
    if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
      return absl::InvalidArgumentError(
          "well known protocol buffer enum missing value: "
          "google.protobuf.NullValue.NULL_VALUE");
    }
    if (ABSL_PREDICT_FALSE(descriptor->value_count() != 1)) {
      std::vector<absl::string_view> values;
      values.reserve(static_cast<size_t>(descriptor->value_count()));
      for (int i = 0; i < descriptor->value_count(); ++i) {
        values.push_back(descriptor->value(i)->name());
      }
      return absl::InvalidArgumentError(
          absl::StrCat("well known protocol buffer enum has multiple values: [",
                       absl::StrJoin(values, ", "), "]"));
    }
    descriptor_ = descriptor;
  }
  return absl::OkStatus();
}

absl::Status BoolValueReflection::Initialize(
    const DescriptorPool* absl_nonnull pool) {
  CEL_ASSIGN_OR_RETURN(const auto* descriptor,
                       GetMessageTypeByName(pool, "google.protobuf.BoolValue"));
  return Initialize(descriptor);
}

absl::Status BoolValueReflection::Initialize(
    const Descriptor* absl_nonnull descriptor) {
  if (descriptor_ != descriptor) {
    CEL_RETURN_IF_ERROR(CheckWellKnownType(descriptor, kWellKnownType));
    descriptor_ = nullptr;
    CEL_ASSIGN_OR_RETURN(value_field_, GetFieldByNumber(descriptor, 1));
    CEL_RETURN_IF_ERROR(
        CheckFieldCppType(value_field_, FieldDescriptor::CPPTYPE_BOOL));
    CEL_RETURN_IF_ERROR(
        CheckFieldCardinality(value_field_, FieldDescriptor::LABEL_OPTIONAL));
    descriptor_ = descriptor;
  }
  return absl::OkStatus();
}

bool BoolValueReflection::GetValue(const google::protobuf::Message& message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  return message.GetReflection()->GetBool(message, value_field_);
}

void BoolValueReflection::SetValue(google::protobuf::Message* absl_nonnull message,
                                   bool value) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  message->GetReflection()->SetBool(message, value_field_, value);
}

absl::StatusOr<BoolValueReflection> GetBoolValueReflection(
    const Descriptor* absl_nonnull descriptor) {
  BoolValueReflection reflection;
  CEL_RETURN_IF_ERROR(reflection.Initialize(descriptor));
  return reflection;
}

absl::Status Int32ValueReflection::Initialize(
    const DescriptorPool* absl_nonnull pool) {
  CEL_ASSIGN_OR_RETURN(
      const auto* descriptor,
      GetMessageTypeByName(pool, "google.protobuf.Int32Value"));
  return Initialize(descriptor);
}

absl::Status Int32ValueReflection::Initialize(
    const Descriptor* absl_nonnull descriptor) {
  if (descriptor_ != descriptor) {
    CEL_RETURN_IF_ERROR(CheckWellKnownType(descriptor, kWellKnownType));
    descriptor_ = nullptr;
    CEL_ASSIGN_OR_RETURN(value_field_, GetFieldByNumber(descriptor, 1));
    CEL_RETURN_IF_ERROR(
        CheckFieldCppType(value_field_, FieldDescriptor::CPPTYPE_INT32));
    CEL_RETURN_IF_ERROR(
        CheckFieldCardinality(value_field_, FieldDescriptor::LABEL_OPTIONAL));
    descriptor_ = descriptor;
  }
  return absl::OkStatus();
}

int32_t Int32ValueReflection::GetValue(const google::protobuf::Message& message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  return message.GetReflection()->GetInt32(message, value_field_);
}

void Int32ValueReflection::SetValue(google::protobuf::Message* absl_nonnull message,
                                    int32_t value) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  message->GetReflection()->SetInt32(message, value_field_, value);
}

absl::StatusOr<Int32ValueReflection> GetInt32ValueReflection(
    const Descriptor* absl_nonnull descriptor) {
  Int32ValueReflection reflection;
  CEL_RETURN_IF_ERROR(reflection.Initialize(descriptor));
  return reflection;
}

absl::Status Int64ValueReflection::Initialize(
    const DescriptorPool* absl_nonnull pool) {
  CEL_ASSIGN_OR_RETURN(
      const auto* descriptor,
      GetMessageTypeByName(pool, "google.protobuf.Int64Value"));
  return Initialize(descriptor);
}

absl::Status Int64ValueReflection::Initialize(
    const Descriptor* absl_nonnull descriptor) {
  if (descriptor_ != descriptor) {
    CEL_RETURN_IF_ERROR(CheckWellKnownType(descriptor, kWellKnownType));
    descriptor_ = nullptr;
    CEL_ASSIGN_OR_RETURN(value_field_, GetFieldByNumber(descriptor, 1));
    CEL_RETURN_IF_ERROR(
        CheckFieldCppType(value_field_, FieldDescriptor::CPPTYPE_INT64));
    CEL_RETURN_IF_ERROR(
        CheckFieldCardinality(value_field_, FieldDescriptor::LABEL_OPTIONAL));
    descriptor_ = descriptor;
  }
  return absl::OkStatus();
}

int64_t Int64ValueReflection::GetValue(const google::protobuf::Message& message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  return message.GetReflection()->GetInt64(message, value_field_);
}

void Int64ValueReflection::SetValue(google::protobuf::Message* absl_nonnull message,
                                    int64_t value) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  message->GetReflection()->SetInt64(message, value_field_, value);
}

absl::StatusOr<Int64ValueReflection> GetInt64ValueReflection(
    const Descriptor* absl_nonnull descriptor) {
  Int64ValueReflection reflection;
  CEL_RETURN_IF_ERROR(reflection.Initialize(descriptor));
  return reflection;
}

absl::Status UInt32ValueReflection::Initialize(
    const DescriptorPool* absl_nonnull pool) {
  CEL_ASSIGN_OR_RETURN(
      const auto* descriptor,
      GetMessageTypeByName(pool, "google.protobuf.UInt32Value"));
  return Initialize(descriptor);
}

absl::Status UInt32ValueReflection::Initialize(
    const Descriptor* absl_nonnull descriptor) {
  if (descriptor_ != descriptor) {
    CEL_RETURN_IF_ERROR(CheckWellKnownType(descriptor, kWellKnownType));
    descriptor_ = nullptr;
    CEL_ASSIGN_OR_RETURN(value_field_, GetFieldByNumber(descriptor, 1));
    CEL_RETURN_IF_ERROR(
        CheckFieldCppType(value_field_, FieldDescriptor::CPPTYPE_UINT32));
    CEL_RETURN_IF_ERROR(
        CheckFieldCardinality(value_field_, FieldDescriptor::LABEL_OPTIONAL));
    descriptor_ = descriptor;
  }
  return absl::OkStatus();
}

uint32_t UInt32ValueReflection::GetValue(const google::protobuf::Message& message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  return message.GetReflection()->GetUInt32(message, value_field_);
}

void UInt32ValueReflection::SetValue(google::protobuf::Message* absl_nonnull message,
                                     uint32_t value) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  message->GetReflection()->SetUInt32(message, value_field_, value);
}

absl::StatusOr<UInt32ValueReflection> GetUInt32ValueReflection(
    const Descriptor* absl_nonnull descriptor) {
  UInt32ValueReflection reflection;
  CEL_RETURN_IF_ERROR(reflection.Initialize(descriptor));
  return reflection;
}

absl::Status UInt64ValueReflection::Initialize(
    const DescriptorPool* absl_nonnull pool) {
  CEL_ASSIGN_OR_RETURN(
      const auto* descriptor,
      GetMessageTypeByName(pool, "google.protobuf.UInt64Value"));
  return Initialize(descriptor);
}

absl::Status UInt64ValueReflection::Initialize(
    const Descriptor* absl_nonnull descriptor) {
  if (descriptor_ != descriptor) {
    CEL_RETURN_IF_ERROR(CheckWellKnownType(descriptor, kWellKnownType));
    descriptor_ = nullptr;
    CEL_ASSIGN_OR_RETURN(value_field_, GetFieldByNumber(descriptor, 1));
    CEL_RETURN_IF_ERROR(
        CheckFieldCppType(value_field_, FieldDescriptor::CPPTYPE_UINT64));
    CEL_RETURN_IF_ERROR(
        CheckFieldCardinality(value_field_, FieldDescriptor::LABEL_OPTIONAL));
    descriptor_ = descriptor;
  }
  return absl::OkStatus();
}

uint64_t UInt64ValueReflection::GetValue(const google::protobuf::Message& message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  return message.GetReflection()->GetUInt64(message, value_field_);
}

void UInt64ValueReflection::SetValue(google::protobuf::Message* absl_nonnull message,
                                     uint64_t value) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  message->GetReflection()->SetUInt64(message, value_field_, value);
}

absl::StatusOr<UInt64ValueReflection> GetUInt64ValueReflection(
    const Descriptor* absl_nonnull descriptor) {
  UInt64ValueReflection reflection;
  CEL_RETURN_IF_ERROR(reflection.Initialize(descriptor));
  return reflection;
}

absl::Status FloatValueReflection::Initialize(
    const DescriptorPool* absl_nonnull pool) {
  CEL_ASSIGN_OR_RETURN(
      const auto* descriptor,
      GetMessageTypeByName(pool, "google.protobuf.FloatValue"));
  return Initialize(descriptor);
}

absl::Status FloatValueReflection::Initialize(
    const Descriptor* absl_nonnull descriptor) {
  if (descriptor_ != descriptor) {
    CEL_RETURN_IF_ERROR(CheckWellKnownType(descriptor, kWellKnownType));
    descriptor_ = nullptr;
    CEL_ASSIGN_OR_RETURN(value_field_, GetFieldByNumber(descriptor, 1));
    CEL_RETURN_IF_ERROR(
        CheckFieldCppType(value_field_, FieldDescriptor::CPPTYPE_FLOAT));
    CEL_RETURN_IF_ERROR(
        CheckFieldCardinality(value_field_, FieldDescriptor::LABEL_OPTIONAL));
    descriptor_ = descriptor;
  }
  return absl::OkStatus();
}

float FloatValueReflection::GetValue(const google::protobuf::Message& message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  return message.GetReflection()->GetFloat(message, value_field_);
}

void FloatValueReflection::SetValue(google::protobuf::Message* absl_nonnull message,
                                    float value) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  message->GetReflection()->SetFloat(message, value_field_, value);
}

absl::StatusOr<FloatValueReflection> GetFloatValueReflection(
    const Descriptor* absl_nonnull descriptor) {
  FloatValueReflection reflection;
  CEL_RETURN_IF_ERROR(reflection.Initialize(descriptor));
  return reflection;
}

absl::Status DoubleValueReflection::Initialize(
    const DescriptorPool* absl_nonnull pool) {
  CEL_ASSIGN_OR_RETURN(
      const auto* descriptor,
      GetMessageTypeByName(pool, "google.protobuf.DoubleValue"));
  return Initialize(descriptor);
}

absl::Status DoubleValueReflection::Initialize(
    const Descriptor* absl_nonnull descriptor) {
  if (descriptor_ != descriptor) {
    CEL_RETURN_IF_ERROR(CheckWellKnownType(descriptor, kWellKnownType));
    descriptor_ = nullptr;
    CEL_ASSIGN_OR_RETURN(value_field_, GetFieldByNumber(descriptor, 1));
    CEL_RETURN_IF_ERROR(
        CheckFieldCppType(value_field_, FieldDescriptor::CPPTYPE_DOUBLE));
    CEL_RETURN_IF_ERROR(
        CheckFieldCardinality(value_field_, FieldDescriptor::LABEL_OPTIONAL));
    descriptor_ = descriptor;
  }
  return absl::OkStatus();
}

double DoubleValueReflection::GetValue(const google::protobuf::Message& message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  return message.GetReflection()->GetDouble(message, value_field_);
}

void DoubleValueReflection::SetValue(google::protobuf::Message* absl_nonnull message,
                                     double value) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  message->GetReflection()->SetDouble(message, value_field_, value);
}

absl::StatusOr<DoubleValueReflection> GetDoubleValueReflection(
    const Descriptor* absl_nonnull descriptor) {
  DoubleValueReflection reflection;
  CEL_RETURN_IF_ERROR(reflection.Initialize(descriptor));
  return reflection;
}

absl::Status BytesValueReflection::Initialize(
    const DescriptorPool* absl_nonnull pool) {
  CEL_ASSIGN_OR_RETURN(
      const auto* descriptor,
      GetMessageTypeByName(pool, "google.protobuf.BytesValue"));
  return Initialize(descriptor);
}

absl::Status BytesValueReflection::Initialize(
    const Descriptor* absl_nonnull descriptor) {
  if (descriptor_ != descriptor) {
    CEL_RETURN_IF_ERROR(CheckWellKnownType(descriptor, kWellKnownType));
    descriptor_ = nullptr;
    CEL_ASSIGN_OR_RETURN(value_field_, GetFieldByNumber(descriptor, 1));
    CEL_RETURN_IF_ERROR(
        CheckFieldType(value_field_, FieldDescriptor::TYPE_BYTES));
    CEL_RETURN_IF_ERROR(
        CheckFieldCardinality(value_field_, FieldDescriptor::LABEL_OPTIONAL));
    value_field_string_type_ = value_field_->cpp_string_type();
    descriptor_ = descriptor;
  }
  return absl::OkStatus();
}

BytesValue BytesValueReflection::GetValue(const google::protobuf::Message& message,
                                          std::string& scratch) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  return GetStringField<BytesValue>(message, value_field_,
                                    value_field_string_type_, scratch);
}

void BytesValueReflection::SetValue(google::protobuf::Message* absl_nonnull message,
                                    absl::string_view value) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  message->GetReflection()->SetString(message, value_field_,
                                      std::string(value));
}

void BytesValueReflection::SetValue(google::protobuf::Message* absl_nonnull message,
                                    const absl::Cord& value) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  message->GetReflection()->SetString(message, value_field_, value);
}

absl::StatusOr<BytesValueReflection> GetBytesValueReflection(
    const Descriptor* absl_nonnull descriptor) {
  BytesValueReflection reflection;
  CEL_RETURN_IF_ERROR(reflection.Initialize(descriptor));
  return reflection;
}

absl::Status StringValueReflection::Initialize(
    const DescriptorPool* absl_nonnull pool) {
  CEL_ASSIGN_OR_RETURN(
      const auto* descriptor,
      GetMessageTypeByName(pool, "google.protobuf.StringValue"));
  return Initialize(descriptor);
}

absl::Status StringValueReflection::Initialize(
    const Descriptor* absl_nonnull descriptor) {
  if (descriptor_ != descriptor) {
    CEL_RETURN_IF_ERROR(CheckWellKnownType(descriptor, kWellKnownType));
    descriptor_ = nullptr;
    CEL_ASSIGN_OR_RETURN(value_field_, GetFieldByNumber(descriptor, 1));
    CEL_RETURN_IF_ERROR(
        CheckFieldType(value_field_, FieldDescriptor::TYPE_STRING));
    CEL_RETURN_IF_ERROR(
        CheckFieldCardinality(value_field_, FieldDescriptor::LABEL_OPTIONAL));
    value_field_string_type_ = value_field_->cpp_string_type();
    descriptor_ = descriptor;
  }
  return absl::OkStatus();
}

StringValue StringValueReflection::GetValue(const google::protobuf::Message& message,
                                            std::string& scratch) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  return GetStringField<StringValue>(message, value_field_,
                                     value_field_string_type_, scratch);
}

void StringValueReflection::SetValue(google::protobuf::Message* absl_nonnull message,
                                     absl::string_view value) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  message->GetReflection()->SetString(message, value_field_,
                                      std::string(value));
}

void StringValueReflection::SetValue(google::protobuf::Message* absl_nonnull message,
                                     const absl::Cord& value) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  message->GetReflection()->SetString(message, value_field_, value);
}

absl::StatusOr<StringValueReflection> GetStringValueReflection(
    const Descriptor* absl_nonnull descriptor) {
  StringValueReflection reflection;
  CEL_RETURN_IF_ERROR(reflection.Initialize(descriptor));
  return reflection;
}

absl::Status AnyReflection::Initialize(
    const DescriptorPool* absl_nonnull pool) {
  CEL_ASSIGN_OR_RETURN(const auto* descriptor,
                       GetMessageTypeByName(pool, "google.protobuf.Any"));
  return Initialize(descriptor);
}

absl::Status AnyReflection::Initialize(
    const Descriptor* absl_nonnull descriptor) {
  if (descriptor_ != descriptor) {
    CEL_RETURN_IF_ERROR(CheckWellKnownType(descriptor, kWellKnownType));
    descriptor_ = nullptr;
    CEL_ASSIGN_OR_RETURN(type_url_field_, GetFieldByNumber(descriptor, 1));
    CEL_RETURN_IF_ERROR(
        CheckFieldType(type_url_field_, FieldDescriptor::TYPE_STRING));
    CEL_RETURN_IF_ERROR(CheckFieldCardinality(type_url_field_,
                                              FieldDescriptor::LABEL_OPTIONAL));
    type_url_field_string_type_ = type_url_field_->cpp_string_type();
    CEL_ASSIGN_OR_RETURN(value_field_, GetFieldByNumber(descriptor, 2));
    CEL_RETURN_IF_ERROR(
        CheckFieldType(value_field_, FieldDescriptor::TYPE_BYTES));
    CEL_RETURN_IF_ERROR(
        CheckFieldCardinality(value_field_, FieldDescriptor::LABEL_OPTIONAL));
    value_field_string_type_ = value_field_->cpp_string_type();
    descriptor_ = descriptor;
  }
  return absl::OkStatus();
}

void AnyReflection::SetTypeUrl(google::protobuf::Message* absl_nonnull message,
                               absl::string_view type_url) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  message->GetReflection()->SetString(message, type_url_field_,
                                      std::string(type_url));
}

void AnyReflection::SetValue(google::protobuf::Message* absl_nonnull message,
                             const absl::Cord& value) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  message->GetReflection()->SetString(message, value_field_, value);
}

StringValue AnyReflection::GetTypeUrl(const google::protobuf::Message& message,
                                      std::string& scratch) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  return GetStringField<StringValue>(message, type_url_field_,
                                     type_url_field_string_type_, scratch);
}

BytesValue AnyReflection::GetValue(const google::protobuf::Message& message,
                                   std::string& scratch) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  return GetStringField<BytesValue>(message, value_field_,
                                    value_field_string_type_, scratch);
}

absl::StatusOr<AnyReflection> GetAnyReflection(
    const Descriptor* absl_nonnull descriptor) {
  AnyReflection reflection;
  CEL_RETURN_IF_ERROR(reflection.Initialize(descriptor));
  return reflection;
}

AnyReflection GetAnyReflectionOrDie(
    const google::protobuf::Descriptor* absl_nonnull descriptor) {
  AnyReflection reflection;
  ABSL_CHECK_OK(reflection.Initialize(descriptor));  // Crash OK
  return reflection;
}

absl::Status DurationReflection::Initialize(
    const DescriptorPool* absl_nonnull pool) {
  CEL_ASSIGN_OR_RETURN(const auto* descriptor,
                       GetMessageTypeByName(pool, "google.protobuf.Duration"));
  return Initialize(descriptor);
}

absl::Status DurationReflection::Initialize(
    const Descriptor* absl_nonnull descriptor) {
  if (descriptor_ != descriptor) {
    CEL_RETURN_IF_ERROR(CheckWellKnownType(descriptor, kWellKnownType));
    descriptor_ = nullptr;
    CEL_ASSIGN_OR_RETURN(seconds_field_, GetFieldByNumber(descriptor, 1));
    CEL_RETURN_IF_ERROR(
        CheckFieldCppType(seconds_field_, FieldDescriptor::CPPTYPE_INT64));
    CEL_RETURN_IF_ERROR(
        CheckFieldCardinality(seconds_field_, FieldDescriptor::LABEL_OPTIONAL));
    CEL_ASSIGN_OR_RETURN(nanos_field_, GetFieldByNumber(descriptor, 2));
    CEL_RETURN_IF_ERROR(
        CheckFieldCppType(nanos_field_, FieldDescriptor::CPPTYPE_INT32));
    CEL_RETURN_IF_ERROR(
        CheckFieldCardinality(nanos_field_, FieldDescriptor::LABEL_OPTIONAL));
    descriptor_ = descriptor;
  }
  return absl::OkStatus();
}

int64_t DurationReflection::GetSeconds(const google::protobuf::Message& message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  return message.GetReflection()->GetInt64(message, seconds_field_);
}

int32_t DurationReflection::GetNanos(const google::protobuf::Message& message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  return message.GetReflection()->GetInt32(message, nanos_field_);
}

void DurationReflection::SetSeconds(google::protobuf::Message* absl_nonnull message,
                                    int64_t value) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  message->GetReflection()->SetInt64(message, seconds_field_, value);
}

void DurationReflection::SetNanos(google::protobuf::Message* absl_nonnull message,
                                  int32_t value) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  message->GetReflection()->SetInt32(message, nanos_field_, value);
}

absl::Status DurationReflection::SetFromAbslDuration(
    google::protobuf::Message* absl_nonnull message, absl::Duration duration) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  int64_t seconds = absl::IDivDuration(duration, absl::Seconds(1), &duration);
  if (ABSL_PREDICT_FALSE(seconds < TimeUtil::kDurationMinSeconds ||
                         seconds > TimeUtil::kDurationMaxSeconds)) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid duration seconds: ", seconds));
  }
  int32_t nanos = static_cast<int32_t>(
      absl::IDivDuration(duration, absl::Nanoseconds(1), &duration));
  if (ABSL_PREDICT_FALSE(nanos < TimeUtil::kDurationMinNanoseconds ||
                         nanos > TimeUtil::kDurationMaxNanoseconds)) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid duration nanoseconds: ", nanos));
  }
  if ((seconds < 0 && nanos > 0) || (seconds > 0 && nanos < 0)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "duration sign mismatch: seconds=", seconds, ", nanoseconds=", nanos));
  }
  SetSeconds(message, seconds);
  SetNanos(message, nanos);
  return absl::OkStatus();
}

absl::Status DurationReflection::SetFromAbslDuration(
    GeneratedMessageType* absl_nonnull message, absl::Duration duration) {
  int64_t seconds = absl::IDivDuration(duration, absl::Seconds(1), &duration);
  if (ABSL_PREDICT_FALSE(seconds < TimeUtil::kDurationMinSeconds ||
                         seconds > TimeUtil::kDurationMaxSeconds)) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid duration seconds: ", seconds));
  }
  int32_t nanos = static_cast<int32_t>(
      absl::IDivDuration(duration, absl::Nanoseconds(1), &duration));
  if (ABSL_PREDICT_FALSE(nanos < TimeUtil::kDurationMinNanoseconds ||
                         nanos > TimeUtil::kDurationMaxNanoseconds)) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid duration nanoseconds: ", nanos));
  }
  if ((seconds < 0 && nanos > 0) || (seconds > 0 && nanos < 0)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "duration sign mismatch: seconds=", seconds, ", nanoseconds=", nanos));
  }
  SetSeconds(message, seconds);
  SetNanos(message, nanos);
  return absl::OkStatus();
}

void DurationReflection::UnsafeSetFromAbslDuration(
    google::protobuf::Message* absl_nonnull message, absl::Duration duration) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  int64_t seconds = absl::IDivDuration(duration, absl::Seconds(1), &duration);
  int32_t nanos = static_cast<int32_t>(
      absl::IDivDuration(duration, absl::Nanoseconds(1), &duration));
  SetSeconds(message, seconds);
  SetNanos(message, nanos);
}

absl::StatusOr<absl::Duration> DurationReflection::ToAbslDuration(
    const google::protobuf::Message& message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  int64_t seconds = GetSeconds(message);
  if (ABSL_PREDICT_FALSE(seconds < TimeUtil::kDurationMinSeconds ||
                         seconds > TimeUtil::kDurationMaxSeconds)) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid duration seconds: ", seconds));
  }
  int32_t nanos = GetNanos(message);
  if (ABSL_PREDICT_FALSE(nanos < TimeUtil::kDurationMinNanoseconds ||
                         nanos > TimeUtil::kDurationMaxNanoseconds)) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid duration nanoseconds: ", nanos));
  }
  if ((seconds < 0 && nanos > 0) || (seconds > 0 && nanos < 0)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "duration sign mismatch: seconds=", seconds, ", nanoseconds=", nanos));
  }
  return absl::Seconds(seconds) + absl::Nanoseconds(nanos);
}

absl::Duration DurationReflection::UnsafeToAbslDuration(
    const google::protobuf::Message& message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  int64_t seconds = GetSeconds(message);
  int32_t nanos = GetNanos(message);
  return absl::Seconds(seconds) + absl::Nanoseconds(nanos);
}

absl::StatusOr<DurationReflection> GetDurationReflection(
    const Descriptor* absl_nonnull descriptor) {
  DurationReflection reflection;
  CEL_RETURN_IF_ERROR(reflection.Initialize(descriptor));
  return reflection;
}

absl::Status TimestampReflection::Initialize(
    const DescriptorPool* absl_nonnull pool) {
  CEL_ASSIGN_OR_RETURN(const auto* descriptor,
                       GetMessageTypeByName(pool, "google.protobuf.Timestamp"));
  return Initialize(descriptor);
}

absl::Status TimestampReflection::Initialize(
    const Descriptor* absl_nonnull descriptor) {
  if (descriptor_ != descriptor) {
    CEL_RETURN_IF_ERROR(CheckWellKnownType(descriptor, kWellKnownType));
    descriptor_ = nullptr;
    CEL_ASSIGN_OR_RETURN(seconds_field_, GetFieldByNumber(descriptor, 1));
    CEL_RETURN_IF_ERROR(
        CheckFieldCppType(seconds_field_, FieldDescriptor::CPPTYPE_INT64));
    CEL_RETURN_IF_ERROR(
        CheckFieldCardinality(seconds_field_, FieldDescriptor::LABEL_OPTIONAL));
    CEL_ASSIGN_OR_RETURN(nanos_field_, GetFieldByNumber(descriptor, 2));
    CEL_RETURN_IF_ERROR(
        CheckFieldCppType(nanos_field_, FieldDescriptor::CPPTYPE_INT32));
    CEL_RETURN_IF_ERROR(
        CheckFieldCardinality(nanos_field_, FieldDescriptor::LABEL_OPTIONAL));
    descriptor_ = descriptor;
  }
  return absl::OkStatus();
}

int64_t TimestampReflection::GetSeconds(const google::protobuf::Message& message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  return message.GetReflection()->GetInt64(message, seconds_field_);
}

int32_t TimestampReflection::GetNanos(const google::protobuf::Message& message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  return message.GetReflection()->GetInt32(message, nanos_field_);
}

void TimestampReflection::SetSeconds(google::protobuf::Message* absl_nonnull message,
                                     int64_t value) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  message->GetReflection()->SetInt64(message, seconds_field_, value);
}

void TimestampReflection::SetNanos(google::protobuf::Message* absl_nonnull message,
                                   int32_t value) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  message->GetReflection()->SetInt32(message, nanos_field_, value);
}

absl::Status TimestampReflection::SetFromAbslTime(
    google::protobuf::Message* absl_nonnull message, absl::Time time) const {
  int64_t seconds = absl::ToUnixSeconds(time);
  if (ABSL_PREDICT_FALSE(seconds < TimeUtil::kTimestampMinSeconds ||
                         seconds > TimeUtil::kTimestampMaxSeconds)) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid timestamp seconds: ", seconds));
  }
  int64_t nanos = static_cast<int64_t>((time - absl::FromUnixSeconds(seconds)) /
                                       absl::Nanoseconds(1));
  if (ABSL_PREDICT_FALSE(nanos < TimeUtil::kTimestampMinNanoseconds ||
                         nanos > TimeUtil::kTimestampMaxNanoseconds)) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid timestamp nanoseconds: ", nanos));
  }
  SetSeconds(message, seconds);
  SetNanos(message, static_cast<int32_t>(nanos));
  return absl::OkStatus();
}

absl::Status TimestampReflection::SetFromAbslTime(
    GeneratedMessageType* absl_nonnull message, absl::Time time) {
  int64_t seconds = absl::ToUnixSeconds(time);
  if (ABSL_PREDICT_FALSE(seconds < TimeUtil::kTimestampMinSeconds ||
                         seconds > TimeUtil::kTimestampMaxSeconds)) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid timestamp seconds: ", seconds));
  }
  int64_t nanos = static_cast<int64_t>((time - absl::FromUnixSeconds(seconds)) /
                                       absl::Nanoseconds(1));
  if (ABSL_PREDICT_FALSE(nanos < TimeUtil::kTimestampMinNanoseconds ||
                         nanos > TimeUtil::kTimestampMaxNanoseconds)) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid timestamp nanoseconds: ", nanos));
  }
  SetSeconds(message, seconds);
  SetNanos(message, static_cast<int32_t>(nanos));
  return absl::OkStatus();
}

void TimestampReflection::UnsafeSetFromAbslTime(
    google::protobuf::Message* absl_nonnull message, absl::Time time) const {
  int64_t seconds = absl::ToUnixSeconds(time);
  int32_t nanos = static_cast<int32_t>((time - absl::FromUnixSeconds(seconds)) /
                                       absl::Nanoseconds(1));
  SetSeconds(message, seconds);
  SetNanos(message, nanos);
}

absl::StatusOr<absl::Time> TimestampReflection::ToAbslTime(
    const google::protobuf::Message& message) const {
  int64_t seconds = GetSeconds(message);
  if (ABSL_PREDICT_FALSE(seconds < TimeUtil::kTimestampMinSeconds ||
                         seconds > TimeUtil::kTimestampMaxSeconds)) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid timestamp seconds: ", seconds));
  }
  int32_t nanos = GetNanos(message);
  if (ABSL_PREDICT_FALSE(nanos < TimeUtil::kTimestampMinNanoseconds ||
                         nanos > TimeUtil::kTimestampMaxNanoseconds)) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid timestamp nanoseconds: ", nanos));
  }
  return absl::UnixEpoch() + absl::Seconds(seconds) + absl::Nanoseconds(nanos);
}

absl::Time TimestampReflection::UnsafeToAbslTime(
    const google::protobuf::Message& message) const {
  int64_t seconds = GetSeconds(message);
  int32_t nanos = GetNanos(message);
  return absl::UnixEpoch() + absl::Seconds(seconds) + absl::Nanoseconds(nanos);
}

absl::StatusOr<TimestampReflection> GetTimestampReflection(
    const Descriptor* absl_nonnull descriptor) {
  TimestampReflection reflection;
  CEL_RETURN_IF_ERROR(reflection.Initialize(descriptor));
  return reflection;
}

void ValueReflection::SetNumberValue(
    google::protobuf::Value* absl_nonnull message, int64_t value) {
  if (value < kJsonMinInt || value > kJsonMaxInt) {
    SetStringValue(message, absl::StrCat(value));
    return;
  }
  SetNumberValue(message, static_cast<double>(value));
}

void ValueReflection::SetNumberValue(
    google::protobuf::Value* absl_nonnull message, uint64_t value) {
  if (value > kJsonMaxUint) {
    SetStringValue(message, absl::StrCat(value));
    return;
  }
  SetNumberValue(message, static_cast<double>(value));
}

absl::Status ValueReflection::Initialize(
    const DescriptorPool* absl_nonnull pool) {
  CEL_ASSIGN_OR_RETURN(const auto* descriptor,
                       GetMessageTypeByName(pool, "google.protobuf.Value"));
  return Initialize(descriptor);
}

absl::Status ValueReflection::Initialize(
    const Descriptor* absl_nonnull descriptor) {
  if (descriptor_ != descriptor) {
    CEL_RETURN_IF_ERROR(CheckWellKnownType(descriptor, kWellKnownType));
    descriptor_ = nullptr;
    CEL_ASSIGN_OR_RETURN(kind_field_, GetOneofByName(descriptor, "kind"));
    CEL_ASSIGN_OR_RETURN(null_value_field_, GetFieldByNumber(descriptor, 1));
    CEL_RETURN_IF_ERROR(
        CheckFieldCppType(null_value_field_, FieldDescriptor::CPPTYPE_ENUM));
    CEL_RETURN_IF_ERROR(CheckFieldCardinality(null_value_field_,
                                              FieldDescriptor::LABEL_OPTIONAL));
    CEL_RETURN_IF_ERROR(CheckFieldOneof(null_value_field_, kind_field_, 0));
    CEL_ASSIGN_OR_RETURN(bool_value_field_, GetFieldByNumber(descriptor, 4));
    CEL_RETURN_IF_ERROR(
        CheckFieldCppType(bool_value_field_, FieldDescriptor::CPPTYPE_BOOL));
    CEL_RETURN_IF_ERROR(CheckFieldCardinality(bool_value_field_,
                                              FieldDescriptor::LABEL_OPTIONAL));
    CEL_RETURN_IF_ERROR(CheckFieldOneof(bool_value_field_, kind_field_, 3));
    CEL_ASSIGN_OR_RETURN(number_value_field_, GetFieldByNumber(descriptor, 2));
    CEL_RETURN_IF_ERROR(CheckFieldCppType(number_value_field_,
                                          FieldDescriptor::CPPTYPE_DOUBLE));
    CEL_RETURN_IF_ERROR(CheckFieldCardinality(number_value_field_,
                                              FieldDescriptor::LABEL_OPTIONAL));
    CEL_RETURN_IF_ERROR(CheckFieldOneof(number_value_field_, kind_field_, 1));
    CEL_ASSIGN_OR_RETURN(string_value_field_, GetFieldByNumber(descriptor, 3));
    CEL_RETURN_IF_ERROR(CheckFieldCppType(string_value_field_,
                                          FieldDescriptor::CPPTYPE_STRING));
    CEL_RETURN_IF_ERROR(CheckFieldCardinality(string_value_field_,
                                              FieldDescriptor::LABEL_OPTIONAL));
    CEL_RETURN_IF_ERROR(CheckFieldOneof(string_value_field_, kind_field_, 2));
    string_value_field_string_type_ = string_value_field_->cpp_string_type();
    CEL_ASSIGN_OR_RETURN(list_value_field_, GetFieldByNumber(descriptor, 6));
    CEL_RETURN_IF_ERROR(
        CheckFieldCppType(list_value_field_, FieldDescriptor::CPPTYPE_MESSAGE));
    CEL_RETURN_IF_ERROR(CheckFieldCardinality(list_value_field_,
                                              FieldDescriptor::LABEL_OPTIONAL));
    CEL_RETURN_IF_ERROR(CheckFieldOneof(list_value_field_, kind_field_, 5));
    CEL_RETURN_IF_ERROR(CheckFieldWellKnownType(
        list_value_field_, Descriptor::WELLKNOWNTYPE_LISTVALUE));
    CEL_ASSIGN_OR_RETURN(struct_value_field_, GetFieldByNumber(descriptor, 5));
    CEL_RETURN_IF_ERROR(CheckFieldCppType(struct_value_field_,
                                          FieldDescriptor::CPPTYPE_MESSAGE));
    CEL_RETURN_IF_ERROR(CheckFieldCardinality(struct_value_field_,
                                              FieldDescriptor::LABEL_OPTIONAL));
    CEL_RETURN_IF_ERROR(CheckFieldOneof(struct_value_field_, kind_field_, 4));
    CEL_RETURN_IF_ERROR(CheckFieldWellKnownType(
        struct_value_field_, Descriptor::WELLKNOWNTYPE_STRUCT));
    descriptor_ = descriptor;
  }
  return absl::OkStatus();
}

google::protobuf::Value::KindCase ValueReflection::GetKindCase(
    const google::protobuf::Message& message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  const auto* field =
      message.GetReflection()->GetOneofFieldDescriptor(message, kind_field_);
  return field != nullptr ? static_cast<google::protobuf::Value::KindCase>(
                                field->index_in_oneof() + 1)
                          : google::protobuf::Value::KIND_NOT_SET;
}

bool ValueReflection::GetBoolValue(const google::protobuf::Message& message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  return message.GetReflection()->GetBool(message, bool_value_field_);
}

double ValueReflection::GetNumberValue(const google::protobuf::Message& message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  return message.GetReflection()->GetDouble(message, number_value_field_);
}

StringValue ValueReflection::GetStringValue(const google::protobuf::Message& message,
                                            std::string& scratch) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  return GetStringField<StringValue>(message, string_value_field_,
                                     string_value_field_string_type_, scratch);
}

const google::protobuf::Message& ValueReflection::GetListValue(
    const google::protobuf::Message& message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
#undef GetMessage
  return message.GetReflection()->GetMessage(message, list_value_field_);
}

const google::protobuf::Message& ValueReflection::GetStructValue(
    const google::protobuf::Message& message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
#undef GetMessage
  return message.GetReflection()->GetMessage(message, struct_value_field_);
}

void ValueReflection::SetNullValue(
    google::protobuf::Message* absl_nonnull message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  message->GetReflection()->SetEnumValue(message, null_value_field_, 0);
}

void ValueReflection::SetBoolValue(google::protobuf::Message* absl_nonnull message,
                                   bool value) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  message->GetReflection()->SetBool(message, bool_value_field_, value);
}

void ValueReflection::SetNumberValue(google::protobuf::Message* absl_nonnull message,
                                     int64_t value) const {
  if (value < kJsonMinInt || value > kJsonMaxInt) {
    SetStringValue(message, absl::StrCat(value));
    return;
  }
  SetNumberValue(message, static_cast<double>(value));
}

void ValueReflection::SetNumberValue(google::protobuf::Message* absl_nonnull message,
                                     uint64_t value) const {
  if (value > kJsonMaxUint) {
    SetStringValue(message, absl::StrCat(value));
    return;
  }
  SetNumberValue(message, static_cast<double>(value));
}

void ValueReflection::SetNumberValue(google::protobuf::Message* absl_nonnull message,
                                     double value) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  message->GetReflection()->SetDouble(message, number_value_field_, value);
}

void ValueReflection::SetStringValue(google::protobuf::Message* absl_nonnull message,
                                     absl::string_view value) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  message->GetReflection()->SetString(message, string_value_field_,
                                      std::string(value));
}

void ValueReflection::SetStringValue(google::protobuf::Message* absl_nonnull message,
                                     const absl::Cord& value) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  message->GetReflection()->SetString(message, string_value_field_, value);
}

void ValueReflection::SetStringValueFromBytes(
    google::protobuf::Message* absl_nonnull message, absl::string_view value) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  if (value.empty()) {
    SetStringValue(message, value);
    return;
  }
  SetStringValue(message, absl::Base64Escape(value));
}

void ValueReflection::SetStringValueFromBytes(
    google::protobuf::Message* absl_nonnull message, const absl::Cord& value) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  if (value.empty()) {
    SetStringValue(message, value);
    return;
  }
  if (auto flat = value.TryFlat(); flat) {
    SetStringValue(message, absl::Base64Escape(*flat));
    return;
  }
  std::string flat;
  absl::CopyCordToString(value, &flat);
  SetStringValue(message, absl::Base64Escape(flat));
}

void ValueReflection::SetStringValueFromDuration(
    google::protobuf::Message* absl_nonnull message, absl::Duration duration) const {
  google::protobuf::Duration proto;
  proto.set_seconds(absl::IDivDuration(duration, absl::Seconds(1), &duration));
  proto.set_nanos(static_cast<int32_t>(
      absl::IDivDuration(duration, absl::Nanoseconds(1), &duration)));
  ABSL_DCHECK(TimeUtil::IsDurationValid(proto));
  SetStringValue(message, TimeUtil::ToString(proto));
}

void ValueReflection::SetStringValueFromTimestamp(
    google::protobuf::Message* absl_nonnull message, absl::Time time) const {
  google::protobuf::Timestamp proto;
  proto.set_seconds(absl::ToUnixSeconds(time));
  proto.set_nanos((time - absl::FromUnixSeconds(proto.seconds())) /
                  absl::Nanoseconds(1));
  ABSL_DCHECK(TimeUtil::IsTimestampValid(proto));
  SetStringValue(message, TimeUtil::ToString(proto));
}

google::protobuf::Message* absl_nonnull ValueReflection::MutableListValue(
    google::protobuf::Message* absl_nonnull message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  return message->GetReflection()->MutableMessage(message, list_value_field_);
}

google::protobuf::Message* absl_nonnull ValueReflection::MutableStructValue(
    google::protobuf::Message* absl_nonnull message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  return message->GetReflection()->MutableMessage(message, struct_value_field_);
}

Unique<google::protobuf::Message> ValueReflection::ReleaseListValue(
    google::protobuf::Message* absl_nonnull message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  const auto* reflection = message->GetReflection();
  if (!reflection->HasField(*message, list_value_field_)) {
    reflection->MutableMessage(message, list_value_field_);
  }
  return WrapUnique(
      reflection->UnsafeArenaReleaseMessage(message, list_value_field_),
      message->GetArena());
}

Unique<google::protobuf::Message> ValueReflection::ReleaseStructValue(
    google::protobuf::Message* absl_nonnull message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  const auto* reflection = message->GetReflection();
  if (!reflection->HasField(*message, struct_value_field_)) {
    reflection->MutableMessage(message, struct_value_field_);
  }
  return WrapUnique(
      reflection->UnsafeArenaReleaseMessage(message, struct_value_field_),
      message->GetArena());
}

absl::StatusOr<ValueReflection> GetValueReflection(
    const Descriptor* absl_nonnull descriptor) {
  ValueReflection reflection;
  CEL_RETURN_IF_ERROR(reflection.Initialize(descriptor));
  return reflection;
}
ValueReflection GetValueReflectionOrDie(
    const google::protobuf::Descriptor* absl_nonnull descriptor) {
  ValueReflection reflection;
  ABSL_CHECK_OK(reflection.Initialize(descriptor));  // Crash OK;
  return reflection;
}

absl::Status ListValueReflection::Initialize(
    const DescriptorPool* absl_nonnull pool) {
  CEL_ASSIGN_OR_RETURN(const auto* descriptor,
                       GetMessageTypeByName(pool, "google.protobuf.ListValue"));
  return Initialize(descriptor);
}

absl::Status ListValueReflection::Initialize(
    const Descriptor* absl_nonnull descriptor) {
  if (descriptor_ != descriptor) {
    CEL_RETURN_IF_ERROR(CheckWellKnownType(descriptor, kWellKnownType));
    descriptor_ = nullptr;
    CEL_ASSIGN_OR_RETURN(values_field_, GetFieldByNumber(descriptor, 1));
    CEL_RETURN_IF_ERROR(
        CheckFieldCppType(values_field_, FieldDescriptor::CPPTYPE_MESSAGE));
    CEL_RETURN_IF_ERROR(
        CheckFieldCardinality(values_field_, FieldDescriptor::LABEL_REPEATED));
    CEL_RETURN_IF_ERROR(CheckFieldWellKnownType(
        values_field_, Descriptor::WELLKNOWNTYPE_VALUE));
    descriptor_ = descriptor;
  }
  return absl::OkStatus();
}

int ListValueReflection::ValuesSize(const google::protobuf::Message& message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  return message.GetReflection()->FieldSize(message, values_field_);
}

google::protobuf::RepeatedFieldRef<google::protobuf::Message> ListValueReflection::Values(
    const google::protobuf::Message& message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  return message.GetReflection()->GetRepeatedFieldRef<google::protobuf::Message>(
      message, values_field_);
}

const google::protobuf::Message& ListValueReflection::Values(
    const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    int index) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  return message.GetReflection()->GetRepeatedMessage(message, values_field_,
                                                     index);
}

google::protobuf::MutableRepeatedFieldRef<google::protobuf::Message>
ListValueReflection::MutableValues(
    google::protobuf::Message* absl_nonnull message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  return message->GetReflection()->GetMutableRepeatedFieldRef<google::protobuf::Message>(
      message, values_field_);
}

google::protobuf::Message* absl_nonnull ListValueReflection::AddValues(
    google::protobuf::Message* absl_nonnull message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
  return message->GetReflection()->AddMessage(message, values_field_);
}

absl::StatusOr<ListValueReflection> GetListValueReflection(
    const Descriptor* absl_nonnull descriptor) {
  ListValueReflection reflection;
  CEL_RETURN_IF_ERROR(reflection.Initialize(descriptor));
  return reflection;
}

ListValueReflection GetListValueReflectionOrDie(
    const google::protobuf::Descriptor* absl_nonnull descriptor) {
  ListValueReflection reflection;
  ABSL_CHECK_OK(reflection.Initialize(descriptor));  // Crash OK
  return reflection;
}

absl::Status StructReflection::Initialize(
    const DescriptorPool* absl_nonnull pool) {
  CEL_ASSIGN_OR_RETURN(const auto* descriptor,
                       GetMessageTypeByName(pool, "google.protobuf.Struct"));
  return Initialize(descriptor);
}

absl::Status StructReflection::Initialize(
    const Descriptor* absl_nonnull descriptor) {
  if (descriptor_ != descriptor) {
    CEL_RETURN_IF_ERROR(CheckWellKnownType(descriptor, kWellKnownType));
    descriptor_ = nullptr;
    CEL_ASSIGN_OR_RETURN(fields_field_, GetFieldByNumber(descriptor, 1));
    CEL_RETURN_IF_ERROR(CheckMapField(fields_field_));
    fields_key_field_ = fields_field_->message_type()->map_key();
    CEL_RETURN_IF_ERROR(
        CheckFieldCppType(fields_key_field_, FieldDescriptor::CPPTYPE_STRING));
    CEL_RETURN_IF_ERROR(CheckFieldCardinality(fields_key_field_,
                                              FieldDescriptor::LABEL_OPTIONAL));
    fields_value_field_ = fields_field_->message_type()->map_value();
    CEL_RETURN_IF_ERROR(CheckFieldCppType(fields_value_field_,
                                          FieldDescriptor::CPPTYPE_MESSAGE));
    CEL_RETURN_IF_ERROR(CheckFieldCardinality(fields_value_field_,
                                              FieldDescriptor::LABEL_OPTIONAL));
    CEL_RETURN_IF_ERROR(CheckFieldWellKnownType(
        fields_value_field_, Descriptor::WELLKNOWNTYPE_VALUE));
    descriptor_ = descriptor;
  }
  return absl::OkStatus();
}

int StructReflection::FieldsSize(const google::protobuf::Message& message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  return cel::extensions::protobuf_internal::MapSize(*message.GetReflection(),
                                                     message, *fields_field_);
}

google::protobuf::MapIterator StructReflection::BeginFields(
    const google::protobuf::Message& message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  return cel::extensions::protobuf_internal::MapBegin(*message.GetReflection(),
                                                      message, *fields_field_);
}

google::protobuf::MapIterator StructReflection::EndFields(
    const google::protobuf::Message& message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  return cel::extensions::protobuf_internal::MapEnd(*message.GetReflection(),
                                                    message, *fields_field_);
}

bool StructReflection::ContainsField(const google::protobuf::Message& message,
                                     absl::string_view name) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
#if CEL_INTERNAL_PROTOBUF_OSS_VERSION_PREREQ(5, 30, 0)
  google::protobuf::MapKey key;
  key.SetStringValue(name);
#else
  std::string key_scratch(name);
  google::protobuf::MapKey key;
  key.SetStringValue(key_scratch);
#endif
  return cel::extensions::protobuf_internal::ContainsMapKey(
      *message.GetReflection(), message, *fields_field_, key);
}

const google::protobuf::Message* absl_nullable StructReflection::FindField(
    const google::protobuf::Message& message, absl::string_view name) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
#if CEL_INTERNAL_PROTOBUF_OSS_VERSION_PREREQ(5, 30, 0)
  google::protobuf::MapKey key;
  key.SetStringValue(name);
#else
  std::string key_scratch(name);
  google::protobuf::MapKey key;
  key.SetStringValue(key_scratch);
#endif
  google::protobuf::MapValueConstRef value;
  if (cel::extensions::protobuf_internal::LookupMapValue(
          *message.GetReflection(), message, *fields_field_, key, &value)) {
    return &value.GetMessageValue();
  }
  return nullptr;
}

google::protobuf::Message* absl_nonnull StructReflection::InsertField(
    google::protobuf::Message* absl_nonnull message, absl::string_view name) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
#if CEL_INTERNAL_PROTOBUF_OSS_VERSION_PREREQ(5, 30, 0)
  google::protobuf::MapKey key;
  key.SetStringValue(name);
#else
  std::string key_scratch(name);
  google::protobuf::MapKey key;
  key.SetStringValue(key_scratch);
#endif
  google::protobuf::MapValueRef value;
  cel::extensions::protobuf_internal::InsertOrLookupMapValue(
      *message->GetReflection(), message, *fields_field_, key, &value);
  return value.MutableMessageValue();
}

bool StructReflection::DeleteField(google::protobuf::Message* absl_nonnull message,
                                   absl::string_view name) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message->GetDescriptor(), descriptor_);
#if CEL_INTERNAL_PROTOBUF_OSS_VERSION_PREREQ(5, 30, 0)
  google::protobuf::MapKey key;
  key.SetStringValue(name);
#else
  std::string key_scratch(name);
  google::protobuf::MapKey key;
  key.SetStringValue(key_scratch);
#endif
  return cel::extensions::protobuf_internal::DeleteMapValue(
      message->GetReflection(), message, fields_field_, key);
}

absl::StatusOr<StructReflection> GetStructReflection(
    const Descriptor* absl_nonnull descriptor) {
  StructReflection reflection;
  CEL_RETURN_IF_ERROR(reflection.Initialize(descriptor));
  return reflection;
}

StructReflection GetStructReflectionOrDie(
    const google::protobuf::Descriptor* absl_nonnull descriptor) {
  StructReflection reflection;
  ABSL_CHECK_OK(reflection.Initialize(descriptor));  // Crash OK
  return reflection;
}

absl::Status FieldMaskReflection::Initialize(
    const google::protobuf::DescriptorPool* absl_nonnull pool) {
  CEL_ASSIGN_OR_RETURN(const auto* descriptor,
                       GetMessageTypeByName(pool, "google.protobuf.FieldMask"));
  return Initialize(descriptor);
}

absl::Status FieldMaskReflection::Initialize(
    const google::protobuf::Descriptor* absl_nonnull descriptor) {
  if (descriptor_ != descriptor) {
    CEL_RETURN_IF_ERROR(CheckWellKnownType(descriptor, kWellKnownType));
    descriptor_ = nullptr;
    CEL_ASSIGN_OR_RETURN(paths_field_, GetFieldByNumber(descriptor, 1));
    CEL_RETURN_IF_ERROR(
        CheckFieldCppType(paths_field_, FieldDescriptor::CPPTYPE_STRING));
    CEL_RETURN_IF_ERROR(
        CheckFieldCardinality(paths_field_, FieldDescriptor::LABEL_REPEATED));
    paths_field_string_type_ = paths_field_->cpp_string_type();
    descriptor_ = descriptor;
  }
  return absl::OkStatus();
}

int FieldMaskReflection::PathsSize(const google::protobuf::Message& message) const {
  ABSL_DCHECK(IsInitialized());
  ABSL_DCHECK_EQ(message.GetDescriptor(), descriptor_);
  return message.GetReflection()->FieldSize(message, paths_field_);
}

StringValue FieldMaskReflection::Paths(const google::protobuf::Message& message,
                                       int index, std::string& scratch) const {
  return GetRepeatedStringField<StringValue>(
      message, paths_field_, paths_field_string_type_, index, scratch);
}

absl::StatusOr<FieldMaskReflection> GetFieldMaskReflection(
    const google::protobuf::Descriptor* absl_nonnull descriptor) {
  FieldMaskReflection reflection;
  CEL_RETURN_IF_ERROR(reflection.Initialize(descriptor));
  return reflection;
}

absl::Status JsonReflection::Initialize(
    const google::protobuf::DescriptorPool* absl_nonnull pool) {
  CEL_RETURN_IF_ERROR(Value().Initialize(pool));
  CEL_RETURN_IF_ERROR(ListValue().Initialize(pool));
  CEL_RETURN_IF_ERROR(Struct().Initialize(pool));
  return absl::OkStatus();
}

absl::Status JsonReflection::Initialize(
    const google::protobuf::Descriptor* absl_nonnull descriptor) {
  switch (descriptor->well_known_type()) {
    case google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE:
      CEL_RETURN_IF_ERROR(Value().Initialize(descriptor));
      CEL_RETURN_IF_ERROR(
          ListValue().Initialize(Value().GetListValueDescriptor()));
      CEL_RETURN_IF_ERROR(Struct().Initialize(Value().GetStructDescriptor()));
      return absl::OkStatus();
    case google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE:
      CEL_RETURN_IF_ERROR(ListValue().Initialize(descriptor));
      CEL_RETURN_IF_ERROR(Value().Initialize(ListValue().GetValueDescriptor()));
      CEL_RETURN_IF_ERROR(Struct().Initialize(Value().GetStructDescriptor()));
      return absl::OkStatus();
    case google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT:
      CEL_RETURN_IF_ERROR(Struct().Initialize(descriptor));
      CEL_RETURN_IF_ERROR(Value().Initialize(Struct().GetValueDescriptor()));
      CEL_RETURN_IF_ERROR(
          ListValue().Initialize(Value().GetListValueDescriptor()));
      return absl::OkStatus();
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("expected message to be JSON-like well known type: ",
                       descriptor->full_name(), " ",
                       WellKnownTypeToString(descriptor->well_known_type())));
  }
}

bool JsonReflection::IsInitialized() const {
  return Value().IsInitialized() && ListValue().IsInitialized() &&
         Struct().IsInitialized();
}

namespace {

[[maybe_unused]] ABSL_CONST_INIT absl::once_flag
    link_well_known_message_reflection;

void LinkWellKnownMessageReflection() {
  google::protobuf::LinkMessageReflection<google::protobuf::BoolValue>();
  google::protobuf::LinkMessageReflection<google::protobuf::Int32Value>();
  google::protobuf::LinkMessageReflection<google::protobuf::Int64Value>();
  google::protobuf::LinkMessageReflection<google::protobuf::UInt32Value>();
  google::protobuf::LinkMessageReflection<google::protobuf::UInt64Value>();
  google::protobuf::LinkMessageReflection<google::protobuf::FloatValue>();
  google::protobuf::LinkMessageReflection<google::protobuf::DoubleValue>();
  google::protobuf::LinkMessageReflection<google::protobuf::BytesValue>();
  google::protobuf::LinkMessageReflection<google::protobuf::StringValue>();
  google::protobuf::LinkMessageReflection<google::protobuf::Any>();
  google::protobuf::LinkMessageReflection<google::protobuf::Duration>();
  google::protobuf::LinkMessageReflection<google::protobuf::Timestamp>();
  google::protobuf::LinkMessageReflection<google::protobuf::Value>();
  google::protobuf::LinkMessageReflection<google::protobuf::ListValue>();
  google::protobuf::LinkMessageReflection<google::protobuf::Struct>();
  google::protobuf::LinkMessageReflection<google::protobuf::FieldMask>();
}

}  // namespace

absl::Status Reflection::Initialize(const DescriptorPool* absl_nonnull pool) {
  if (pool == DescriptorPool::generated_pool()) {
    absl::call_once(link_well_known_message_reflection,
                    &LinkWellKnownMessageReflection);
  }
  CEL_RETURN_IF_ERROR(NullValue().Initialize(pool));
  CEL_RETURN_IF_ERROR(BoolValue().Initialize(pool));
  CEL_RETURN_IF_ERROR(Int32Value().Initialize(pool));
  CEL_RETURN_IF_ERROR(Int64Value().Initialize(pool));
  CEL_RETURN_IF_ERROR(UInt32Value().Initialize(pool));
  CEL_RETURN_IF_ERROR(UInt64Value().Initialize(pool));
  CEL_RETURN_IF_ERROR(FloatValue().Initialize(pool));
  CEL_RETURN_IF_ERROR(DoubleValue().Initialize(pool));
  CEL_RETURN_IF_ERROR(BytesValue().Initialize(pool));
  CEL_RETURN_IF_ERROR(StringValue().Initialize(pool));
  CEL_RETURN_IF_ERROR(Any().Initialize(pool));
  CEL_RETURN_IF_ERROR(Duration().Initialize(pool));
  CEL_RETURN_IF_ERROR(Timestamp().Initialize(pool));
  CEL_RETURN_IF_ERROR(Json().Initialize(pool));
  // google.protobuf.FieldMask is not strictly mandatory, but we do have to
  // treat it specifically for JSON. So use it if we have it.
  if (const auto* descriptor =
          pool->FindMessageTypeByName("google.protobuf.FieldMask");
      descriptor != nullptr) {
    CEL_RETURN_IF_ERROR(FieldMask().Initialize(descriptor));
  }
  return absl::OkStatus();
}

bool Reflection::IsInitialized() const {
  // Check that everything is initialized except field mask, which is optional.
  return NullValue().IsInitialized() && BoolValue().IsInitialized() &&
         Int32Value().IsInitialized() && Int64Value().IsInitialized() &&
         UInt32Value().IsInitialized() && UInt64Value().IsInitialized() &&
         FloatValue().IsInitialized() && DoubleValue().IsInitialized() &&
         BytesValue().IsInitialized() && StringValue().IsInitialized() &&
         Any().IsInitialized() && Duration().IsInitialized() &&
         Timestamp().IsInitialized() && Json().IsInitialized();
}

namespace {

// AdaptListValue verifies the message is the well known type
// `google.protobuf.ListValue` and performs the complicated logic of reimaging
// it as `ListValue`. If adapted is empty, we return as a reference. If adapted
// is present, message must be a reference to the value held in adapted and it
// will be returned by value.
absl::StatusOr<ListValue> AdaptListValue(google::protobuf::Arena* absl_nullable arena,
                                         const google::protobuf::Message& message,
                                         Unique<google::protobuf::Message> adapted) {
  ABSL_DCHECK(!adapted || &message == cel::to_address(adapted));
  const auto* descriptor = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(descriptor == nullptr)) {
    return absl::InvalidArgumentError(
        absl::StrCat("missing descriptor for protocol buffer message: ",
                     message.GetTypeName()));
  }
  // Not much to do. Just verify the well known type is well-formed.
  CEL_RETURN_IF_ERROR(GetListValueReflection(descriptor).status());
  if (adapted) {
    return ListValue(std::move(adapted));
  }
  return ListValue(std::cref(message));
}

// AdaptStruct verifies the message is the well known type
// `google.protobuf.Struct` and performs the complicated logic of reimaging it
// as `Struct`. If adapted is empty, we return as a reference. If adapted is
// present, message must be a reference to the value held in adapted and it will
// be returned by value.
absl::StatusOr<Struct> AdaptStruct(google::protobuf::Arena* absl_nullable arena,
                                   const google::protobuf::Message& message,
                                   Unique<google::protobuf::Message> adapted) {
  ABSL_DCHECK(!adapted || &message == cel::to_address(adapted));
  const auto* descriptor = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(descriptor == nullptr)) {
    return absl::InvalidArgumentError(
        absl::StrCat("missing descriptor for protocol buffer message: ",
                     message.GetTypeName()));
  }
  // Not much to do. Just verify the well known type is well-formed.
  CEL_RETURN_IF_ERROR(GetStructReflection(descriptor).status());
  if (adapted) {
    return Struct(std::move(adapted));
  }
  return Struct(std::cref(message));
}

// AdaptAny recursively unpacks a protocol buffer message which is an instance
// of `google.protobuf.Any`.
absl::StatusOr<Unique<google::protobuf::Message>> AdaptAny(
    google::protobuf::Arena* absl_nullable arena, AnyReflection& reflection,
    const google::protobuf::Message& message, const Descriptor* absl_nonnull descriptor,
    const DescriptorPool* absl_nonnull pool,
    google::protobuf::MessageFactory* absl_nonnull factory, bool error_if_unresolveable) {
  ABSL_DCHECK_EQ(descriptor->well_known_type(), Descriptor::WELLKNOWNTYPE_ANY);
  const google::protobuf::Message* absl_nonnull to_unwrap = &message;
  Unique<google::protobuf::Message> unwrapped;
  std::string type_url_scratch;
  std::string value_scratch;
  do {
    CEL_RETURN_IF_ERROR(reflection.Initialize(descriptor));
    StringValue type_url = reflection.GetTypeUrl(*to_unwrap, type_url_scratch);
    absl::string_view type_url_view =
        FlatStringValue(type_url, type_url_scratch);
    if (!absl::ConsumePrefix(&type_url_view, "type.googleapis.com/") &&
        !absl::ConsumePrefix(&type_url_view, "type.googleprod.com/")) {
      if (!error_if_unresolveable) {
        break;
      }
      return absl::InvalidArgumentError(absl::StrCat(
          "unable to find descriptor for type URL: ", type_url_view));
    }
    const auto* packed_descriptor = pool->FindMessageTypeByName(type_url_view);
    if (packed_descriptor == nullptr) {
      if (!error_if_unresolveable) {
        break;
      }
      return absl::InvalidArgumentError(absl::StrCat(
          "unable to find descriptor for type name: ", type_url_view));
    }
    const auto* prototype = factory->GetPrototype(packed_descriptor);
    if (prototype == nullptr) {
      return absl::InvalidArgumentError(absl::StrCat(
          "unable to build prototype for type name: ", type_url_view));
    }
    BytesValue value = reflection.GetValue(*to_unwrap, value_scratch);
    Unique<google::protobuf::Message> unpacked = WrapUnique(prototype->New(arena), arena);
    const bool ok = absl::visit(absl::Overload(
                                    [&](absl::string_view string) -> bool {
                                      return unpacked->ParseFromString(string);
                                    },
                                    [&](const absl::Cord& cord) -> bool {
                                      return unpacked->ParseFromString(cord);
                                    }),
                                AsVariant(value));
    if (!ok) {
      return absl::InvalidArgumentError(absl::StrCat(
          "failed to unpack protocol buffer message: ", type_url_view));
    }
    // We can only update unwrapped at this point, not before. This is because
    // we could have been unpacking from unwrapped itself.
    unwrapped = std::move(unpacked);
    to_unwrap = cel::to_address(unwrapped);
    descriptor = to_unwrap->GetDescriptor();
    if (descriptor == nullptr) {
      return absl::InvalidArgumentError(
          absl::StrCat("missing descriptor for protocol buffer message: ",
                       to_unwrap->GetTypeName()));
    }
  } while (descriptor->well_known_type() == Descriptor::WELLKNOWNTYPE_ANY);
  return unwrapped;
}

}  // namespace

absl::StatusOr<Unique<google::protobuf::Message>> UnpackAnyFrom(
    google::protobuf::Arena* absl_nullable arena, AnyReflection& reflection,
    const google::protobuf::Message& message,
    const google::protobuf::DescriptorPool* absl_nonnull pool,
    google::protobuf::MessageFactory* absl_nonnull factory) {
  ABSL_DCHECK_EQ(message.GetDescriptor()->well_known_type(),
                 Descriptor::WELLKNOWNTYPE_ANY);
  return AdaptAny(arena, reflection, message, message.GetDescriptor(), pool,
                  factory, /*error_if_unresolveable=*/true);
}

absl::StatusOr<Unique<google::protobuf::Message>> UnpackAnyIfResolveable(
    google::protobuf::Arena* absl_nullable arena, AnyReflection& reflection,
    const google::protobuf::Message& message,
    const google::protobuf::DescriptorPool* absl_nonnull pool,
    google::protobuf::MessageFactory* absl_nonnull factory) {
  ABSL_DCHECK_EQ(message.GetDescriptor()->well_known_type(),
                 Descriptor::WELLKNOWNTYPE_ANY);
  return AdaptAny(arena, reflection, message, message.GetDescriptor(), pool,
                  factory, /*error_if_unresolveable=*/false);
}

absl::StatusOr<well_known_types::Value> AdaptFromMessage(
    google::protobuf::Arena* absl_nullable arena, const google::protobuf::Message& message,
    const DescriptorPool* absl_nonnull pool,
    google::protobuf::MessageFactory* absl_nonnull factory, std::string& scratch) {
  const auto* descriptor = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(descriptor == nullptr)) {
    return absl::InvalidArgumentError(
        absl::StrCat("missing descriptor for protocol buffer message: ",
                     message.GetTypeName()));
  }
  const google::protobuf::Message* absl_nonnull to_adapt;
  Unique<google::protobuf::Message> adapted;
  Descriptor::WellKnownType well_known_type = descriptor->well_known_type();
  if (well_known_type == Descriptor::WELLKNOWNTYPE_ANY) {
    AnyReflection reflection;
    CEL_ASSIGN_OR_RETURN(
        adapted, UnpackAnyFrom(arena, reflection, message, pool, factory));
    to_adapt = cel::to_address(adapted);
    // GetDescriptor() is guaranteed to be nonnull by AdaptAny().
    descriptor = to_adapt->GetDescriptor();
    well_known_type = descriptor->well_known_type();
  } else {
    to_adapt = &message;
  }
  switch (descriptor->well_known_type()) {
    case Descriptor::WELLKNOWNTYPE_DOUBLEVALUE: {
      CEL_ASSIGN_OR_RETURN(auto reflection,
                           GetDoubleValueReflection(descriptor));
      return reflection.GetValue(*to_adapt);
    }
    case Descriptor::WELLKNOWNTYPE_FLOATVALUE: {
      CEL_ASSIGN_OR_RETURN(auto reflection,
                           GetFloatValueReflection(descriptor));
      return reflection.GetValue(*to_adapt);
    }
    case Descriptor::WELLKNOWNTYPE_INT64VALUE: {
      CEL_ASSIGN_OR_RETURN(auto reflection,
                           GetInt64ValueReflection(descriptor));
      return reflection.GetValue(*to_adapt);
    }
    case Descriptor::WELLKNOWNTYPE_UINT64VALUE: {
      CEL_ASSIGN_OR_RETURN(auto reflection,
                           GetUInt64ValueReflection(descriptor));
      return reflection.GetValue(*to_adapt);
    }
    case Descriptor::WELLKNOWNTYPE_INT32VALUE: {
      CEL_ASSIGN_OR_RETURN(auto reflection,
                           GetInt32ValueReflection(descriptor));
      return reflection.GetValue(*to_adapt);
    }
    case Descriptor::WELLKNOWNTYPE_UINT32VALUE: {
      CEL_ASSIGN_OR_RETURN(auto reflection,
                           GetUInt32ValueReflection(descriptor));
      return reflection.GetValue(*to_adapt);
    }
    case Descriptor::WELLKNOWNTYPE_STRINGVALUE: {
      CEL_ASSIGN_OR_RETURN(auto reflection,
                           GetStringValueReflection(descriptor));
      auto value = reflection.GetValue(*to_adapt, scratch);
      if (adapted) {
        // value might actually be a view of data owned by adapted, force a copy
        // to scratch if that is the case.
        value = CopyStringValue(value, scratch);
      }
      return value;
    }
    case Descriptor::WELLKNOWNTYPE_BYTESVALUE: {
      CEL_ASSIGN_OR_RETURN(auto reflection,
                           GetBytesValueReflection(descriptor));
      auto value = reflection.GetValue(*to_adapt, scratch);
      if (adapted) {
        // value might actually be a view of data owned by adapted, force a copy
        // to scratch if that is the case.
        value = CopyBytesValue(value, scratch);
      }
      return value;
    }
    case Descriptor::WELLKNOWNTYPE_BOOLVALUE: {
      CEL_ASSIGN_OR_RETURN(auto reflection, GetBoolValueReflection(descriptor));
      return reflection.GetValue(*to_adapt);
    }
    case Descriptor::WELLKNOWNTYPE_ANY:
      // This is unreachable, as AdaptAny() above recursively unpacks.
      ABSL_UNREACHABLE();
    case Descriptor::WELLKNOWNTYPE_DURATION: {
      CEL_ASSIGN_OR_RETURN(auto reflection, GetDurationReflection(descriptor));
      return reflection.ToAbslDuration(*to_adapt);
    }
    case Descriptor::WELLKNOWNTYPE_TIMESTAMP: {
      CEL_ASSIGN_OR_RETURN(auto reflection, GetTimestampReflection(descriptor));
      return reflection.ToAbslTime(*to_adapt);
    }
    case Descriptor::WELLKNOWNTYPE_VALUE: {
      CEL_ASSIGN_OR_RETURN(auto reflection, GetValueReflection(descriptor));
      const auto kind_case = reflection.GetKindCase(*to_adapt);
      switch (kind_case) {
        case google::protobuf::Value::KIND_NOT_SET:
          ABSL_FALLTHROUGH_INTENDED;
        case google::protobuf::Value::kNullValue:
          return nullptr;
        case google::protobuf::Value::kNumberValue:
          return reflection.GetNumberValue(*to_adapt);
        case google::protobuf::Value::kStringValue: {
          auto value = reflection.GetStringValue(*to_adapt, scratch);
          if (adapted) {
            value = CopyStringValue(value, scratch);
          }
          return value;
        }
        case google::protobuf::Value::kBoolValue:
          return reflection.GetBoolValue(*to_adapt);
        case google::protobuf::Value::kStructValue: {
          if (adapted) {
            // We can release.
            adapted = reflection.ReleaseStructValue(cel::to_address(adapted));
            to_adapt = cel::to_address(adapted);
          } else {
            to_adapt = &reflection.GetStructValue(*to_adapt);
          }
          return AdaptStruct(arena, *to_adapt, std::move(adapted));
        }
        case google::protobuf::Value::kListValue: {
          if (adapted) {
            // We can release.
            adapted = reflection.ReleaseListValue(cel::to_address(adapted));
            to_adapt = cel::to_address(adapted);
          } else {
            to_adapt = &reflection.GetListValue(*to_adapt);
          }
          return AdaptListValue(arena, *to_adapt, std::move(adapted));
        }
        default:
          return absl::InvalidArgumentError(
              absl::StrCat("unexpected value kind case: ", kind_case));
      }
    }
    case Descriptor::WELLKNOWNTYPE_LISTVALUE:
      return AdaptListValue(arena, *to_adapt, std::move(adapted));
    case Descriptor::WELLKNOWNTYPE_STRUCT:
      return AdaptStruct(arena, *to_adapt, std::move(adapted));
    default:
      if (adapted) {
        return adapted;
      }
      return absl::monostate{};
  }
}

}  // namespace cel::well_known_types
