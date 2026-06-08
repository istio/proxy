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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_REPEATED_FIELD_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_REPEATED_FIELD_VALUE_H_

#include <cstddef>
#include <ostream>
#include <string>
#include <utility>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/custom_list_value.h"
#include "common/values/values.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

class Value;
class ValueIterator;
class ParsedJsonListValue;

// ParsedRepeatedFieldValue is a ListValue over a repeated field of a parsed
// protocol buffer message.
class ParsedRepeatedFieldValue final
    : private common_internal::ListValueMixin<ParsedRepeatedFieldValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kList;
  static constexpr absl::string_view kName = "list";

  ParsedRepeatedFieldValue(const google::protobuf::Message* absl_nonnull message,
                           const google::protobuf::FieldDescriptor* absl_nonnull field,
                           google::protobuf::Arena* absl_nonnull arena)
      : message_(message), field_(field), arena_(arena) {
    ABSL_DCHECK(message != nullptr);
    ABSL_DCHECK(field != nullptr);
    ABSL_DCHECK(arena != nullptr);
    ABSL_DCHECK(field_->is_repeated() && !field_->is_map())
        << field_->full_name() << " must be a repeated field";
    ABSL_DCHECK_OK(CheckArena(message_, arena_));
  }

  // Places the `ParsedRepeatedFieldValue` into an invalid state. Anything
  // except assigning to `ParsedRepeatedFieldValue` is undefined behavior.
  ParsedRepeatedFieldValue() = default;

  ParsedRepeatedFieldValue(const ParsedRepeatedFieldValue&) = default;
  ParsedRepeatedFieldValue(ParsedRepeatedFieldValue&&) = default;
  ParsedRepeatedFieldValue& operator=(const ParsedRepeatedFieldValue&) =
      default;
  ParsedRepeatedFieldValue& operator=(ParsedRepeatedFieldValue&&) = default;

  static constexpr ValueKind kind() { return kKind; }

  static constexpr absl::string_view GetTypeName() { return kName; }

  static ListType GetRuntimeType() { return ListType(); }

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

  // See Value::ConvertToJsonArray().
  absl::Status ConvertToJsonArray(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const;

  absl::Status Equal(const Value& other,
                     const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                     google::protobuf::MessageFactory* absl_nonnull message_factory,
                     google::protobuf::Arena* absl_nonnull arena,
                     Value* absl_nonnull result) const;
  using ListValueMixin::Equal;

  bool IsZeroValue() const;

  bool IsEmpty() const;

  ParsedRepeatedFieldValue Clone(google::protobuf::Arena* absl_nonnull arena) const;

  size_t Size() const;

  // See ListValueInterface::Get for documentation.
  absl::Status Get(size_t index,
                   const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                   google::protobuf::MessageFactory* absl_nonnull message_factory,
                   google::protobuf::Arena* absl_nonnull arena,
                   Value* absl_nonnull result) const;
  using ListValueMixin::Get;

  using ForEachCallback = typename CustomListValueInterface::ForEachCallback;

  using ForEachWithIndexCallback =
      typename CustomListValueInterface::ForEachWithIndexCallback;

  absl::Status ForEach(
      ForEachWithIndexCallback callback,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const;
  using ListValueMixin::ForEach;

  absl::StatusOr<absl_nonnull ValueIteratorPtr> NewIterator() const;

  absl::Status Contains(
      const Value& other,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const;
  using ListValueMixin::Contains;

  const google::protobuf::Message& message() const {
    ABSL_DCHECK(*this);
    return *message_;
  }

  const google::protobuf::FieldDescriptor* absl_nonnull field() const {
    ABSL_DCHECK(*this);
    return field_;
  }

  // Returns `true` if `ParsedRepeatedFieldValue` is in a valid state.
  explicit operator bool() const { return field_ != nullptr; }

  friend void swap(ParsedRepeatedFieldValue& lhs,
                   ParsedRepeatedFieldValue& rhs) noexcept {
    using std::swap;
    swap(lhs.message_, rhs.message_);
    swap(lhs.field_, rhs.field_);
    swap(lhs.arena_, rhs.arena_);
  }

 private:
  friend class ParsedJsonListValue;
  friend class common_internal::ValueMixin<ParsedRepeatedFieldValue>;
  friend class common_internal::ListValueMixin<ParsedRepeatedFieldValue>;
  friend ParsedRepeatedFieldValue UnsafeParsedRepeatedFieldValue(
      const google::protobuf::Message* absl_nonnull message,
      const google::protobuf::FieldDescriptor* absl_nonnull field);

  ParsedRepeatedFieldValue(const google::protobuf::Message* absl_nonnull message,
                           const google::protobuf::FieldDescriptor* absl_nonnull field)
      : message_(message), field_(field), arena_(message->GetArena()) {
    ABSL_DCHECK(message != nullptr);
    ABSL_DCHECK(field != nullptr);
    ABSL_DCHECK(field_->is_repeated() && !field_->is_map())
        << field_->full_name() << " must be a repeated field";
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

  const google::protobuf::Reflection* absl_nonnull GetReflection() const;

  const google::protobuf::Message* absl_nullable message_ = nullptr;
  const google::protobuf::FieldDescriptor* absl_nullable field_ = nullptr;
  google::protobuf::Arena* absl_nullable arena_ = nullptr;
};

inline std::ostream& operator<<(std::ostream& out,
                                const ParsedRepeatedFieldValue& value) {
  return out << value.DebugString();
}

// Creates a `ParsedRepeatedFieldValue` without specifying a managing arena.
// The message must outlive the `ParsedRepeatedFieldValue` or any value that
// might be derived from it. Prefer to use
// `cel::Value::WrapRepeatedFieldUnsafe()`.
inline ParsedRepeatedFieldValue UnsafeParsedRepeatedFieldValue(
    const google::protobuf::Message* absl_nonnull message,
    const google::protobuf::FieldDescriptor* absl_nonnull field) {
  return ParsedRepeatedFieldValue(message, field);
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_REPEATED_FIELD_VALUE_H_
