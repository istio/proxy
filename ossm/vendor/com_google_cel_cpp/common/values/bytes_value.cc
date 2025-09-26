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

#include <cstddef>
#include <string>

#include "google/protobuf/wrappers.pb.h"
#include "absl/base/nullability.h"
#include "absl/functional/overload.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/internal/byte_string.h"
#include "common/value.h"
#include "internal/status_macros.h"
#include "internal/strings.h"
#include "internal/well_known_types.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

namespace {

using ::cel::well_known_types::ValueReflection;

template <typename Bytes>
std::string BytesDebugString(const Bytes& value) {
  return value.NativeValue(absl::Overload(
      [](absl::string_view string) -> std::string {
        return internal::FormatBytesLiteral(string);
      },
      [](const absl::Cord& cord) -> std::string {
        if (auto flat = cord.TryFlat(); flat.has_value()) {
          return internal::FormatBytesLiteral(*flat);
        }
        return internal::FormatBytesLiteral(static_cast<std::string>(cord));
      }));
}

}  // namespace

BytesValue BytesValue::Concat(const BytesValue& lhs, const BytesValue& rhs,
                              google::protobuf::Arena* absl_nonnull arena) {
  return BytesValue(
      common_internal::ByteString::Concat(lhs.value_, rhs.value_, arena));
}

std::string BytesValue::DebugString() const { return BytesDebugString(*this); }

absl::Status BytesValue::SerializeTo(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(output != nullptr);

  google::protobuf::BytesValue message;
  message.set_value(NativeString());
  if (!message.SerializePartialToZeroCopyStream(output)) {
    return absl::UnknownError(
        absl::StrCat("failed to serialize message: ", message.GetTypeName()));
  }

  return absl::OkStatus();
}

absl::Status BytesValue::ConvertToJson(
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
  NativeValue([&](const auto& value) {
    value_reflection.SetStringValueFromBytes(json, value);
  });

  return absl::OkStatus();
}

absl::Status BytesValue::Equal(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  if (auto other_value = other.AsBytes(); other_value.has_value()) {
    *result = NativeValue([other_value](const auto& value) -> BoolValue {
      return other_value->NativeValue(
          [&value](const auto& other_value) -> BoolValue {
            return BoolValue{value == other_value};
          });
    });
    return absl::OkStatus();
  }
  *result = FalseValue();
  return absl::OkStatus();
}

BytesValue BytesValue::Clone(google::protobuf::Arena* absl_nonnull arena) const {
  return BytesValue(value_.Clone(arena));
}

size_t BytesValue::Size() const {
  return NativeValue(
      [](const auto& alternative) -> size_t { return alternative.size(); });
}

bool BytesValue::IsEmpty() const {
  return NativeValue(
      [](const auto& alternative) -> bool { return alternative.empty(); });
}

bool BytesValue::Equals(absl::string_view bytes) const {
  return NativeValue([bytes](const auto& alternative) -> bool {
    return alternative == bytes;
  });
}

bool BytesValue::Equals(const absl::Cord& bytes) const {
  return NativeValue([&bytes](const auto& alternative) -> bool {
    return alternative == bytes;
  });
}

bool BytesValue::Equals(const BytesValue& bytes) const {
  return bytes.NativeValue(
      [this](const auto& alternative) -> bool { return Equals(alternative); });
}

namespace {

int CompareImpl(absl::string_view lhs, absl::string_view rhs) {
  return lhs.compare(rhs);
}

int CompareImpl(absl::string_view lhs, const absl::Cord& rhs) {
  return -rhs.Compare(lhs);
}

int CompareImpl(const absl::Cord& lhs, absl::string_view rhs) {
  return lhs.Compare(rhs);
}

int CompareImpl(const absl::Cord& lhs, const absl::Cord& rhs) {
  return lhs.Compare(rhs);
}

}  // namespace

int BytesValue::Compare(absl::string_view bytes) const {
  return NativeValue([bytes](const auto& alternative) -> int {
    return CompareImpl(alternative, bytes);
  });
}

int BytesValue::Compare(const absl::Cord& bytes) const {
  return NativeValue([&bytes](const auto& alternative) -> int {
    return CompareImpl(alternative, bytes);
  });
}

int BytesValue::Compare(const BytesValue& bytes) const {
  return bytes.NativeValue(
      [this](const auto& alternative) -> int { return Compare(alternative); });
}

}  // namespace cel
