/* Copyright 2019 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "util.h"
#include "istio_message.h"

#include "absl/strings/str_format.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"

namespace istio {
namespace dynamic_encoding {
using ::google::protobuf::Descriptor;
using ::google::protobuf::EnumDescriptor;
using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::FileDescriptorSet;
using ::google::protobuf::util::Status;
using ::google::protobuf::util::StatusOr;
using ::google::protobuf::util::error::INTERNAL;
using int32 = std::int32_t;
using uint32 = std::uint32_t;
using int64 = std::int64_t;
using uint64 = std::uint64_t;

Status GetFieldEncodingError(const FieldDescriptor* field_descriptor) {
  const std::string err_msg =
      absl::StrFormat("unable to encode: %s for field %s",
                      field_descriptor->type_name(), field_descriptor->name());
  return Status(INTERNAL, err_msg);
}

Status EncodeStaticField(absl::any* value, MessageEncoder* messageEncoder,
                         const FieldDescriptor* field_descriptor, int index) {
  if (messageEncoder == nullptr || messageEncoder->GetReflection() == nullptr) {
    return GetFieldEncodingError(field_descriptor);
  }
  switch (field_descriptor->cpp_type()) {
    case FieldDescriptor::CPPTYPE_ENUM: {
      auto status_or_value = GetEnumescriptorValue(value, field_descriptor);
      if (!status_or_value.ok()) {
        return status_or_value.status();
      }
      auto enum_value = status_or_value.ValueOrDie();
      ::google::protobuf::EnumValueDescriptor* enum_descriptor_value =
          absl::any_cast<::google::protobuf::EnumValueDescriptor*>(enum_value);
      if (enum_descriptor_value == nullptr) {
        return GetFieldEncodingError(field_descriptor);
      }
      if (field_descriptor->is_repeated()) {
        messageEncoder->GetReflection()->SetRepeatedEnum(
            messageEncoder->GetMessage(), field_descriptor, index,
            enum_descriptor_value);
      } else {
        messageEncoder->GetReflection()->SetEnum(messageEncoder->GetMessage(),
                                                 field_descriptor,
                                                 enum_descriptor_value);
      }

      break;
    }
#define SET_FIELD(CPPTYPE, METHOD, LTYPE)                                \
  case FieldDescriptor::CPPTYPE_##CPPTYPE: {                             \
    LTYPE* LTYPE_value = absl::any_cast<LTYPE>(value);                   \
    if (LTYPE_value == nullptr) {                                        \
      return GetFieldEncodingError(field_descriptor);                    \
    }                                                                    \
    if (!field_descriptor->is_repeated()) {                              \
      messageEncoder->GetReflection()->Set##METHOD(                      \
          messageEncoder->GetMessage(), field_descriptor, *LTYPE_value); \
    } else {                                                             \
      messageEncoder->GetReflection()->SetRepeated##METHOD(              \
          messageEncoder->GetMessage(), field_descriptor, index,         \
          *LTYPE_value);                                                 \
    }                                                                    \
  } break

    SET_FIELD(STRING, String, std::string);
    SET_FIELD(INT32, Int32, int32);
    SET_FIELD(INT64, Int64, int64);
    SET_FIELD(UINT32, UInt32, uint32);
    SET_FIELD(UINT64, UInt64, uint64);
    SET_FIELD(DOUBLE, Double, double);
    SET_FIELD(FLOAT, Float, float);
    SET_FIELD(BOOL, Bool, bool);
#undef SET_FIELD
    default:
      return GetFieldEncodingError(field_descriptor);
  }
  return Status::OK;
}

Status EncodeMessageField(absl::any* value, MessageEncoder* messageEncoder,
                          const FieldDescriptor* field_descriptor, int index) {
  if (messageEncoder == nullptr || messageEncoder->GetReflection() == nullptr ||
      field_descriptor->cpp_type() != FieldDescriptor::CPPTYPE_MESSAGE ||
      value == nullptr) {
    return GetFieldEncodingError(field_descriptor);
  }
  IstioMessage* msg_value = absl::any_cast<IstioMessage*>(*value);
  if (msg_value == nullptr) {
    return GetFieldEncodingError(field_descriptor);
  }
  if (field_descriptor->is_repeated()) {
    messageEncoder->GetReflection()
        ->MutableRepeatedMessage(messageEncoder->GetMessage(), field_descriptor,
                                 index)
        ->CopyFrom(*(msg_value->message()));
  } else {
    messageEncoder->GetReflection()
        ->MutableMessage(messageEncoder->GetMessage(), field_descriptor)
        ->CopyFrom(*(msg_value->message()));
  }
  return Status::OK;
}

::google::protobuf::util::StatusOr<absl::any> GetEnumescriptorValue(
    absl::any* value,
    const ::google::protobuf::FieldDescriptor* field_descriptor) {
  const ::google::protobuf::EnumDescriptor* enum_descriptor =
      field_descriptor->enum_type();
  if (enum_descriptor == nullptr) {
    const std::string err_msg =
        absl::StrFormat("Could not find enum descriptor for field %s",
                        field_descriptor->name());
    return Status(INTERNAL, err_msg);
  }

  int* int_value = absl::any_cast<int>(value);
  if (int_value != nullptr) {
    const ::google::protobuf::EnumValueDescriptor* enum_descriptor_value =
        enum_descriptor->FindValueByNumber(*int_value);
    return absl::any(enum_descriptor_value);
  }

  std::string* str_value = absl::any_cast<std::string>(value);
  if (str_value != nullptr) {
    const ::google::protobuf::EnumValueDescriptor* enum_descriptor_value =
        enum_descriptor->FindValueByName(*str_value);
    return absl::any(enum_descriptor_value);
  }

  const ::google::protobuf::EnumValueDescriptor* enum_value =
      absl::any_cast<const ::google::protobuf::EnumValueDescriptor*>(*value);
  if (enum_value != nullptr) {
    return *value;
  }

  const std::string err_msg =
      absl::StrFormat("Could not convert value to enum type for field %s",
                      field_descriptor->name());
  return Status(INTERNAL, err_msg);
}

}  // namespace dynamic_encoding
}  // namespace istio
