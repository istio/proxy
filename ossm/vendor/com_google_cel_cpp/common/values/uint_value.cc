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
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "common/value.h"
#include "internal/number.h"
#include "internal/status_macros.h"
#include "internal/well_known_types.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

namespace {

using ::cel::well_known_types::ValueReflection;

std::string UintDebugString(int64_t value) { return absl::StrCat(value, "u"); }

}  // namespace

std::string UintValue::DebugString() const {
  return UintDebugString(NativeValue());
}

absl::Status UintValue::SerializeTo(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(output != nullptr);

  google::protobuf::UInt64Value message;
  message.set_value(NativeValue());
  if (!message.SerializePartialToZeroCopyStream(output)) {
    return absl::UnknownError(
        absl::StrCat("failed to serialize message: ", message.GetTypeName()));
  }

  return absl::OkStatus();
}

absl::Status UintValue::ConvertToJson(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE);

  ValueReflection value_reflection;
  CEL_RETURN_IF_ERROR(value_reflection.Initialize(json->GetDescriptor()));
  value_reflection.SetNumberValue(json, NativeValue());

  return absl::OkStatus();
}

absl::Status UintValue::Equal(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  if (auto other_value = other.AsUint(); other_value.has_value()) {
    *result = BoolValue{NativeValue() == other_value->NativeValue()};
    return absl::OkStatus();
  }
  if (auto other_value = other.AsDouble(); other_value.has_value()) {
    *result =
        BoolValue{internal::Number::FromUint64(NativeValue()) ==
                  internal::Number::FromDouble(other_value->NativeValue())};
    return absl::OkStatus();
  }
  if (auto other_value = other.AsInt(); other_value.has_value()) {
    *result =
        BoolValue{internal::Number::FromUint64(NativeValue()) ==
                  internal::Number::FromInt64(other_value->NativeValue())};
    return absl::OkStatus();
  }
  *result = FalseValue();
  return absl::OkStatus();
}

}  // namespace cel
