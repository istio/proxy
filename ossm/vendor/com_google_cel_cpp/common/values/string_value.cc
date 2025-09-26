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
#include <cstring>
#include <string>

#include "google/protobuf/wrappers.pb.h"
#include "absl/base/nullability.h"
#include "absl/functional/overload.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/internal/byte_string.h"
#include "common/value.h"
#include "internal/status_macros.h"
#include "internal/strings.h"
#include "internal/utf8.h"
#include "internal/well_known_types.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

namespace {

using ::cel::well_known_types::ValueReflection;

template <typename Bytes>
std::string StringDebugString(const Bytes& value) {
  return value.NativeValue(absl::Overload(
      [](absl::string_view string) -> std::string {
        return internal::FormatStringLiteral(string);
      },
      [](const absl::Cord& cord) -> std::string {
        if (auto flat = cord.TryFlat(); flat.has_value()) {
          return internal::FormatStringLiteral(*flat);
        }
        return internal::FormatStringLiteral(static_cast<std::string>(cord));
      }));
}

}  // namespace

StringValue StringValue::Concat(const StringValue& lhs, const StringValue& rhs,
                                google::protobuf::Arena* absl_nonnull arena) {
  return StringValue(
      common_internal::ByteString::Concat(lhs.value_, rhs.value_, arena));
}

std::string StringValue::DebugString() const {
  return StringDebugString(*this);
}

absl::Status StringValue::SerializeTo(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(output != nullptr);

  google::protobuf::StringValue message;
  message.set_value(NativeString());
  if (!message.SerializePartialToZeroCopyStream(output)) {
    return absl::UnknownError(
        absl::StrCat("failed to serialize message: ", message.GetTypeName()));
  }

  return absl::OkStatus();
}

absl::Status StringValue::ConvertToJson(
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
  NativeValue(
      [&](const auto& value) { value_reflection.SetStringValue(json, value); });

  return absl::OkStatus();
}

absl::Status StringValue::Equal(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  if (auto other_value = other.AsString(); other_value.has_value()) {
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

size_t StringValue::Size() const {
  return NativeValue([](const auto& alternative) -> size_t {
    return internal::Utf8CodePointCount(alternative);
  });
}

bool StringValue::IsEmpty() const {
  return NativeValue(
      [](const auto& alternative) -> bool { return alternative.empty(); });
}

bool StringValue::Equals(absl::string_view string) const {
  return value_.Equals(string);
}

bool StringValue::Equals(const absl::Cord& string) const {
  return value_.Equals(string);
}

bool StringValue::Equals(const StringValue& string) const {
  return value_.Equals(string.value_);
}

StringValue StringValue::Clone(google::protobuf::Arena* absl_nonnull arena) const {
  return StringValue(value_.Clone(arena));
}

int StringValue::Compare(absl::string_view string) const {
  return value_.Compare(string);
}

int StringValue::Compare(const absl::Cord& string) const {
  return value_.Compare(string);
}

int StringValue::Compare(const StringValue& string) const {
  return value_.Compare(string.value_);
}

bool StringValue::StartsWith(absl::string_view string) const {
  return value_.StartsWith(string);
}

bool StringValue::StartsWith(const absl::Cord& string) const {
  return value_.StartsWith(string);
}

bool StringValue::StartsWith(const StringValue& string) const {
  return value_.StartsWith(string.value_);
}

bool StringValue::EndsWith(absl::string_view string) const {
  return value_.EndsWith(string);
}

bool StringValue::EndsWith(const absl::Cord& string) const {
  return value_.EndsWith(string);
}

bool StringValue::EndsWith(const StringValue& string) const {
  return value_.EndsWith(string.value_);
}

bool StringValue::Contains(absl::string_view string) const {
  return value_.Visit(absl::Overload(
      [&](absl::string_view lhs) -> bool {
        return absl::StrContains(lhs, string);
      },
      [&](const absl::Cord& lhs) -> bool { return lhs.Contains(string); }));
}

bool StringValue::Contains(const absl::Cord& string) const {
  return value_.Visit(absl::Overload(
      [&](absl::string_view lhs) -> bool {
        if (auto flat = string.TryFlat(); flat) {
          return absl::StrContains(lhs, *flat);
        }
        // There is no nice way to do this. We cannot use std::search due to
        // absl::Cord::CharIterator being an input iterator instead of a forward
        // iterator. So just make an external cord with a noop releaser. We know
        // the external cord will not outlive this function.
        return absl::MakeCordFromExternal(lhs, []() {}).Contains(string);
      },
      [&](const absl::Cord& lhs) -> bool { return lhs.Contains(string); }));
}

bool StringValue::Contains(const StringValue& string) const {
  return string.value_.Visit(absl::Overload(
      [&](absl::string_view rhs) -> bool { return Contains(rhs); },
      [&](const absl::Cord& rhs) -> bool { return Contains(rhs); }));
}

}  // namespace cel
