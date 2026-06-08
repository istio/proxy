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

// IWYU pragma: private, include "common/value.h"
// IWYU pragma: friend "common/value.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_MESSAGE_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_MESSAGE_VALUE_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/struct.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/custom_struct_value.h"
#include "common/values/values.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

class MessageValue;
class StructValue;
class Value;

class ParsedMessageValue final
    : private common_internal::StructValueMixin<ParsedMessageValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kStruct;

  using element_type = const google::protobuf::Message;

  ParsedMessageValue(
      const google::protobuf::Message* absl_nonnull value ABSL_ATTRIBUTE_LIFETIME_BOUND,
      google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : value_(value), arena_(arena) {
    ABSL_DCHECK(value != nullptr);
    ABSL_DCHECK(arena != nullptr);
    ABSL_DCHECK(!value_ || !IsWellKnownMessageType(value_->GetDescriptor()))
        << value_->GetTypeName() << " is a well known type";
    ABSL_DCHECK(!value_ || value_->GetReflection() != nullptr)
        << value_->GetTypeName() << " is missing reflection";
    ABSL_DCHECK_OK(CheckArena(value_, arena_));
  }

  // Places the `ParsedMessageValue` into a special state where it is logically
  // equivalent to the default instance of `google.protobuf.Empty`, however
  // dereferencing via `operator*` or `operator->` is not allowed.
  ParsedMessageValue();
  ParsedMessageValue(const ParsedMessageValue&) = default;
  ParsedMessageValue(ParsedMessageValue&&) = default;
  ParsedMessageValue& operator=(const ParsedMessageValue&) = default;
  ParsedMessageValue& operator=(ParsedMessageValue&&) = default;

  static constexpr ValueKind kind() { return kKind; }

  absl::string_view GetTypeName() const { return GetDescriptor()->full_name(); }

  MessageType GetRuntimeType() const { return MessageType(GetDescriptor()); }

  const google::protobuf::Descriptor* absl_nonnull GetDescriptor() const {
    return (*this)->GetDescriptor();
  }

  const google::protobuf::Reflection* absl_nonnull GetReflection() const {
    return (*this)->GetReflection();
  }

  const google::protobuf::Message& operator*() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return *value_;
  }

  const google::protobuf::Message* absl_nonnull operator->() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return value_;
  }

  bool IsZeroValue() const;

  std::string DebugString() const;

  // See Value::SerializeTo().
  absl::Status SerializeTo(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const;

  // See Value::ConvertToJson().
  absl::Status ConvertToJson(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const;

  // See Value::ConvertToJsonObject().
  absl::Status ConvertToJsonObject(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const;

  absl::Status Equal(const Value& other,
                     const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                     google::protobuf::MessageFactory* absl_nonnull message_factory,
                     google::protobuf::Arena* absl_nonnull arena,
                     Value* absl_nonnull result) const;
  using StructValueMixin::Equal;

  ParsedMessageValue Clone(google::protobuf::Arena* absl_nonnull arena) const;

  absl::Status GetFieldByName(
      absl::string_view name, ProtoWrapperTypeOptions unboxing_options,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const;
  using StructValueMixin::GetFieldByName;

  absl::Status GetFieldByNumber(
      int64_t number, ProtoWrapperTypeOptions unboxing_options,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const;
  using StructValueMixin::GetFieldByNumber;

  absl::StatusOr<bool> HasFieldByName(absl::string_view name) const;

  absl::StatusOr<bool> HasFieldByNumber(int64_t number) const;

  using ForEachFieldCallback = CustomStructValueInterface::ForEachFieldCallback;

  absl::Status ForEachField(
      ForEachFieldCallback callback,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const;

  absl::Status Qualify(
      absl::Span<const SelectQualifier> qualifiers, bool presence_test,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result,
      int* absl_nonnull count) const;
  using StructValueMixin::Qualify;

  friend void swap(ParsedMessageValue& lhs, ParsedMessageValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
    swap(lhs.arena_, rhs.arena_);
  }

 private:
  friend std::pointer_traits<ParsedMessageValue>;
  friend class StructValue;
  friend class common_internal::ValueMixin<ParsedMessageValue>;
  friend class common_internal::StructValueMixin<ParsedMessageValue>;
  friend ParsedMessageValue UnsafeParsedMessageValue(
      const google::protobuf::Message* absl_nonnull value);

  explicit ParsedMessageValue(
      const google::protobuf::Message* absl_nonnull value ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : value_(value), arena_(value->GetArena()) {
    ABSL_DCHECK(value != nullptr);
    ABSL_DCHECK(!value_ || !IsWellKnownMessageType(value_->GetDescriptor()))
        << value_->GetTypeName() << " is a well known type";
    ABSL_DCHECK(!value_ || value_->GetReflection() != nullptr)
        << value_->GetTypeName() << " is missing reflection";
  }

  static absl::Status CheckArena(const google::protobuf::Message* absl_nullable message,
                                 google::protobuf::Arena* absl_nonnull arena) {
    if (message != nullptr && message->GetArena() != nullptr &&
        message->GetArena() != arena) {
      return absl::InvalidArgumentError(
          "message arena must be the same as arena");
    }
    return absl::OkStatus();
  }

  absl::Status GetField(
      const google::protobuf::FieldDescriptor* absl_nonnull field,
      ProtoWrapperTypeOptions unboxing_options,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const;

  bool HasField(const google::protobuf::FieldDescriptor* absl_nonnull field) const;

  const google::protobuf::Message* absl_nonnull value_;
  // Arena that is attributed as owning the value. May be null to indicate that
  // the value is managed externally.
  google::protobuf::Arena* absl_nullable arena_;
};

inline std::ostream& operator<<(std::ostream& out,
                                const ParsedMessageValue& value) {
  return out << value.DebugString();
}

// Creates a `ParsedMessageValue` without specifying a managing arena.
// The message must outlive the `ParsedMessageValue` or any value that might
// be derived from it. Prefer to use `cel::Value::WrapMessageUnsafe()`.
inline ParsedMessageValue UnsafeParsedMessageValue(
    const google::protobuf::Message* absl_nonnull value) {
  return ParsedMessageValue(value);
}

}  // namespace cel

namespace std {

template <>
struct pointer_traits<cel::ParsedMessageValue> {
  using pointer = cel::ParsedMessageValue;
  using element_type = typename cel::ParsedMessageValue::element_type;
  using difference_type = ptrdiff_t;

  static element_type* to_address(const pointer& p) noexcept {
    return cel::to_address(p.value_);
  }
};

}  // namespace std

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_MESSAGE_VALUE_H_
