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

#include <cstdint>
#include <string>

#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/functional/function_ref.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/value.h"
#include "common/values/values.h"
#include "internal/status_macros.h"
#include "internal/well_known_types.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

namespace {

using ::cel::well_known_types::ValueReflection;

}  // namespace

absl::Status CustomStructValueInterface::Equal(
    const StructValue& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  return common_internal::StructValueEqual(*this, other, descriptor_pool,
                                           message_factory, arena, result);
}

absl::Status CustomStructValueInterface::Qualify(
    absl::Span<const SelectQualifier> qualifiers, bool presence_test,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result,
    int* absl_nonnull count) const {
  return absl::UnimplementedError(absl::StrCat(
      GetTypeName(), " does not implement field selection optimization"));
}

NativeTypeId CustomStructValue::GetTypeId() const {
  if (dispatcher_ == nullptr) {
    CustomStructValueInterface::Content content =
        content_.To<CustomStructValueInterface::Content>();
    if (content.interface == nullptr) {
      return NativeTypeId();
    }
    return content.interface->GetNativeTypeId();
  }
  return dispatcher_->get_type_id(dispatcher_, content_);
}

StructType CustomStructValue::GetRuntimeType() const {
  ABSL_DCHECK(*this);

  if (dispatcher_ == nullptr) {
    CustomStructValueInterface::Content content =
        content_.To<CustomStructValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->GetRuntimeType();
  }
  if (dispatcher_->get_runtime_type != nullptr) {
    return dispatcher_->get_runtime_type(dispatcher_, content_);
  }
  return common_internal::MakeBasicStructType(GetTypeName());
}

absl::string_view CustomStructValue::GetTypeName() const {
  ABSL_DCHECK(*this);

  if (dispatcher_ == nullptr) {
    CustomStructValueInterface::Content content =
        content_.To<CustomStructValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->GetTypeName();
  }
  return dispatcher_->get_type_name(dispatcher_, content_);
}

std::string CustomStructValue::DebugString() const {
  ABSL_DCHECK(*this);

  if (dispatcher_ == nullptr) {
    CustomStructValueInterface::Content content =
        content_.To<CustomStructValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->DebugString();
  }
  if (dispatcher_->debug_string != nullptr) {
    return dispatcher_->debug_string(dispatcher_, content_);
  }
  return std::string(GetTypeName());
}

absl::Status CustomStructValue::SerializeTo(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const {
  ABSL_DCHECK(*this);

  if (dispatcher_ == nullptr) {
    CustomStructValueInterface::Content content =
        content_.To<CustomStructValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->SerializeTo(descriptor_pool, message_factory,
                                          output);
  }
  if (dispatcher_->serialize_to != nullptr) {
    return dispatcher_->serialize_to(dispatcher_, content_, descriptor_pool,
                                     message_factory, output);
  }
  return absl::UnimplementedError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::Status CustomStructValue::ConvertToJson(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE);
  ABSL_DCHECK(*this);

  ValueReflection value_reflection;
  CEL_RETURN_IF_ERROR(value_reflection.Initialize(json->GetDescriptor()));
  google::protobuf::Message* json_object = value_reflection.MutableStructValue(json);

  return ConvertToJsonObject(descriptor_pool, message_factory, json_object);
}

absl::Status CustomStructValue::ConvertToJsonObject(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK(*this);

  if (dispatcher_ == nullptr) {
    CustomStructValueInterface::Content content =
        content_.To<CustomStructValueInterface::Content>();
    if (ABSL_PREDICT_FALSE(content.interface == nullptr)) {
      json->Clear();
      return absl::OkStatus();
    }
    return content.interface->ConvertToJsonObject(descriptor_pool,
                                                  message_factory, json);
  }
  if (dispatcher_->convert_to_json_object != nullptr) {
    return dispatcher_->convert_to_json_object(
        dispatcher_, content_, descriptor_pool, message_factory, json);
  }
  return absl::UnimplementedError(
      absl::StrCat(GetTypeName(), " is not convertable to JSON"));
}

absl::Status CustomStructValue::Equal(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK(*this);

  if (auto other_struct_value = other.AsStruct(); other_struct_value) {
    if (dispatcher_ == nullptr) {
      CustomStructValueInterface::Content content =
          content_.To<CustomStructValueInterface::Content>();
      ABSL_DCHECK(content.interface != nullptr);
      return content.interface->Equal(*other_struct_value, descriptor_pool,
                                      message_factory, arena, result);
    }
    if (dispatcher_->equal != nullptr) {
      return dispatcher_->equal(dispatcher_, content_, *other_struct_value,
                                descriptor_pool, message_factory, arena,
                                result);
    }
    return common_internal::StructValueEqual(*this, *other_struct_value,
                                             descriptor_pool, message_factory,
                                             arena, result);
  }
  *result = FalseValue();
  return absl::OkStatus();
}

bool CustomStructValue::IsZeroValue() const {
  ABSL_DCHECK(*this);

  if (dispatcher_ == nullptr) {
    CustomStructValueInterface::Content content =
        content_.To<CustomStructValueInterface::Content>();
    if (content.interface == nullptr) {
      return true;
    }
    return content.interface->IsZeroValue();
  }
  return dispatcher_->is_zero_value(dispatcher_, content_);
}

CustomStructValue CustomStructValue::Clone(
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(*this);

  if (dispatcher_ == nullptr) {
    CustomStructValueInterface::Content content =
        content_.To<CustomStructValueInterface::Content>();
    if (content.interface == nullptr) {
      return *this;
    }
    if (content.arena != arena) {
      return content.interface->Clone(arena);
    }
    return *this;
  }
  return dispatcher_->clone(dispatcher_, content_, arena);
}

absl::Status CustomStructValue::GetFieldByName(
    absl::string_view name, ProtoWrapperTypeOptions unboxing_options,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK(*this);

  if (dispatcher_ == nullptr) {
    CustomStructValueInterface::Content content =
        content_.To<CustomStructValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->GetFieldByName(name, unboxing_options,
                                             descriptor_pool, message_factory,
                                             arena, result);
  }
  return dispatcher_->get_field_by_name(dispatcher_, content_, name,
                                        unboxing_options, descriptor_pool,
                                        message_factory, arena, result);
}

absl::Status CustomStructValue::GetFieldByNumber(
    int64_t number, ProtoWrapperTypeOptions unboxing_options,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK(*this);

  if (dispatcher_ == nullptr) {
    CustomStructValueInterface::Content content =
        content_.To<CustomStructValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->GetFieldByNumber(number, unboxing_options,
                                               descriptor_pool, message_factory,
                                               arena, result);
  }
  if (dispatcher_->get_field_by_number != nullptr) {
    return dispatcher_->get_field_by_number(dispatcher_, content_, number,
                                            unboxing_options, descriptor_pool,
                                            message_factory, arena, result);
  }
  return absl::UnimplementedError(absl::StrCat(
      GetTypeName(), " does not implement access by field number"));
}

absl::StatusOr<bool> CustomStructValue::HasFieldByName(
    absl::string_view name) const {
  ABSL_DCHECK(*this);

  if (dispatcher_ == nullptr) {
    CustomStructValueInterface::Content content =
        content_.To<CustomStructValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->HasFieldByName(name);
  }
  return dispatcher_->has_field_by_name(dispatcher_, content_, name);
}

absl::StatusOr<bool> CustomStructValue::HasFieldByNumber(int64_t number) const {
  ABSL_DCHECK(*this);

  if (dispatcher_ == nullptr) {
    CustomStructValueInterface::Content content =
        content_.To<CustomStructValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->HasFieldByNumber(number);
  }
  if (dispatcher_->has_field_by_number != nullptr) {
    return dispatcher_->has_field_by_number(dispatcher_, content_, number);
  }
  return absl::UnimplementedError(absl::StrCat(
      GetTypeName(), " does not implement access by field number"));
}

absl::Status CustomStructValue::ForEachField(
    ForEachFieldCallback callback,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(*this);

  if (dispatcher_ == nullptr) {
    CustomStructValueInterface::Content content =
        content_.To<CustomStructValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->ForEachField(callback, descriptor_pool,
                                           message_factory, arena);
  }
  return dispatcher_->for_each_field(dispatcher_, content_, callback,
                                     descriptor_pool, message_factory, arena);
}

absl::Status CustomStructValue::Qualify(
    absl::Span<const SelectQualifier> qualifiers, bool presence_test,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result,
    int* absl_nonnull count) const {
  ABSL_DCHECK_GT(qualifiers.size(), 0);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK(count != nullptr);
  ABSL_DCHECK(*this);

  if (dispatcher_ == nullptr) {
    CustomStructValueInterface::Content content =
        content_.To<CustomStructValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->Qualify(qualifiers, presence_test,
                                      descriptor_pool, message_factory, arena,
                                      result, count);
  }
  if (dispatcher_->qualify != nullptr) {
    return dispatcher_->qualify(dispatcher_, content_, qualifiers,
                                presence_test, descriptor_pool, message_factory,
                                arena, result, count);
  }
  return absl::UnimplementedError(absl::StrCat(
      GetTypeName(), " does not implement field selection optimization"));
}

}  // namespace cel
