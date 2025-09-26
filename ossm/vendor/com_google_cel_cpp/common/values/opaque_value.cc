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

#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/native_type.h"
#include "common/optional_ref.h"
#include "common/type.h"
#include "common/value.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

// Code below assumes OptionalValue has the same layout as OpaqueValue.
static_assert(std::is_base_of_v<OpaqueValue, OptionalValue>);
static_assert(sizeof(OpaqueValue) == sizeof(OptionalValue));
static_assert(alignof(OpaqueValue) == alignof(OptionalValue));

OpaqueValue OpaqueValue::Clone(google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(*this);

  if (ABSL_PREDICT_FALSE(dispatcher_ == nullptr)) {
    OpaqueValueInterface::Content content =
        content_.To<OpaqueValueInterface::Content>();
    if (content.interface == nullptr) {
      return *this;
    }
    if (content.arena != arena) {
      return content.interface->Clone(arena);
    }
    return *this;
  }
  if (dispatcher_->get_arena(dispatcher_, content_) != arena) {
    return dispatcher_->clone(dispatcher_, content_, arena);
  }
  return *this;
}

OpaqueType OpaqueValue::GetRuntimeType() const {
  ABSL_DCHECK(*this);

  if (ABSL_PREDICT_FALSE(dispatcher_ == nullptr)) {
    OpaqueValueInterface::Content content =
        content_.To<OpaqueValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->GetRuntimeType();
  }
  return dispatcher_->get_runtime_type(dispatcher_, content_);
}

absl::string_view OpaqueValue::GetTypeName() const {
  ABSL_DCHECK(*this);

  if (ABSL_PREDICT_FALSE(dispatcher_ == nullptr)) {
    OpaqueValueInterface::Content content =
        content_.To<OpaqueValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->GetTypeName();
  }
  return dispatcher_->get_type_name(dispatcher_, content_);
}

std::string OpaqueValue::DebugString() const {
  ABSL_DCHECK(*this);

  if (ABSL_PREDICT_FALSE(dispatcher_ == nullptr)) {
    OpaqueValueInterface::Content content =
        content_.To<OpaqueValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->DebugString();
  }
  return dispatcher_->debug_string(dispatcher_, content_);
}

// See Value::SerializeTo().
absl::Status OpaqueValue::SerializeTo(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(output != nullptr);

  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), "is unserializable"));
}

// See Value::ConvertToJson().
absl::Status OpaqueValue::ConvertToJson(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);

  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is not convertable to JSON"));
}

absl::Status OpaqueValue::Equal(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  if (auto other_opaque = other.AsOpaque(); other_opaque) {
    if (ABSL_PREDICT_FALSE(dispatcher_ == nullptr)) {
      OpaqueValueInterface::Content content =
          content_.To<OpaqueValueInterface::Content>();
      ABSL_DCHECK(content.interface != nullptr);
      return content.interface->Equal(*other_opaque, descriptor_pool,
                                      message_factory, arena, result);
    }
    return dispatcher_->equal(dispatcher_, content_, *other_opaque,
                              descriptor_pool, message_factory, arena, result);
  }
  *result = FalseValue();
  return absl::OkStatus();
}

NativeTypeId OpaqueValue::GetTypeId() const {
  ABSL_DCHECK(*this);

  if (ABSL_PREDICT_FALSE(dispatcher_ == nullptr)) {
    OpaqueValueInterface::Content content =
        content_.To<OpaqueValueInterface::Content>();
    if (content.interface == nullptr) {
      return NativeTypeId();
    }
    return content.interface->GetNativeTypeId();
  }
  return dispatcher_->get_type_id(dispatcher_, content_);
}

bool OpaqueValue::IsOptional() const {
  return dispatcher_ != nullptr &&
         dispatcher_->get_type_id(dispatcher_, content_) ==
             NativeTypeId::For<OptionalValue>();
}

optional_ref<const OptionalValue> OpaqueValue::AsOptional() const& {
  if (IsOptional()) {
    return *reinterpret_cast<const OptionalValue*>(this);
  }
  return absl::nullopt;
}

absl::optional<OptionalValue> OpaqueValue::AsOptional() && {
  if (IsOptional()) {
    return std::move(*reinterpret_cast<OptionalValue*>(this));
  }
  return absl::nullopt;
}

const OptionalValue& OpaqueValue::GetOptional() const& {
  ABSL_DCHECK(IsOptional()) << *this;
  return *reinterpret_cast<const OptionalValue*>(this);
}

OptionalValue OpaqueValue::GetOptional() && {
  ABSL_DCHECK(IsOptional()) << *this;
  return std::move(*reinterpret_cast<OptionalValue*>(this));
}

}  // namespace cel
