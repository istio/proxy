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
#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/allocator.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/list_value_interface.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

class Value;
class ValueManager;
class ValueIterator;
class ParsedJsonListValue;

// ParsedRepeatedFieldValue is a ListValue over a repeated field of a parsed
// protocol buffer message.
class ParsedRepeatedFieldValue final {
 public:
  static constexpr ValueKind kKind = ValueKind::kList;
  static constexpr absl::string_view kName = "list";

  ParsedRepeatedFieldValue(Owned<const google::protobuf::Message> message,
                           absl::Nonnull<const google::protobuf::FieldDescriptor*> field)
      : message_(std::move(message)), field_(field) {
    ABSL_DCHECK(field_->is_repeated() && !field_->is_map())
        << field_->full_name() << " must be a repeated field";
  }

  // Places the `ParsedRepeatedFieldValue` into an invalid state. Anything
  // except assigning to `ParsedRepeatedFieldValue` is undefined behavior.
  ParsedRepeatedFieldValue() = default;

  ParsedRepeatedFieldValue(const ParsedRepeatedFieldValue&) = default;
  ParsedRepeatedFieldValue(ParsedRepeatedFieldValue&&) = default;
  ParsedRepeatedFieldValue& operator=(const ParsedRepeatedFieldValue&) =
      default;
  ParsedRepeatedFieldValue& operator=(ParsedRepeatedFieldValue&&) = default;

  static ValueKind kind() { return kKind; }

  static absl::string_view GetTypeName() { return kName; }

  static ListType GetRuntimeType() { return ListType(); }

  std::string DebugString() const;

  absl::Status SerializeTo(AnyToJsonConverter& converter,
                           absl::Cord& value) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter& converter) const;

  absl::StatusOr<JsonArray> ConvertToJsonArray(
      AnyToJsonConverter& converter) const;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const;

  bool IsEmpty() const;

  ParsedRepeatedFieldValue Clone(Allocator<> allocator) const;

  size_t Size() const;

  // See ListValueInterface::Get for documentation.
  absl::Status Get(ValueManager& value_manager, size_t index,
                   Value& result) const;
  absl::StatusOr<Value> Get(ValueManager& value_manager, size_t index) const;

  using ForEachCallback = typename ListValueInterface::ForEachCallback;

  using ForEachWithIndexCallback =
      typename ListValueInterface::ForEachWithIndexCallback;

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachCallback callback) const;

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachWithIndexCallback callback) const;

  absl::StatusOr<absl::Nonnull<std::unique_ptr<ValueIterator>>> NewIterator(
      ValueManager& value_manager) const;

  absl::Status Contains(ValueManager& value_manager, const Value& other,
                        Value& result) const;
  absl::StatusOr<Value> Contains(ValueManager& value_manager,
                                 const Value& other) const;

  const google::protobuf::Message& message() const {
    ABSL_DCHECK(*this);
    return *message_;
  }

  absl::Nonnull<const google::protobuf::FieldDescriptor*> field() const {
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
  }

 private:
  friend class ParsedJsonListValue;

  absl::Nonnull<const google::protobuf::Reflection*> GetReflection() const;

  Owned<const google::protobuf::Message> message_;
  absl::Nullable<const google::protobuf::FieldDescriptor*> field_ = nullptr;
};

inline std::ostream& operator<<(std::ostream& out,
                                const ParsedRepeatedFieldValue& value) {
  return out << value.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_REPEATED_FIELD_VALUE_H_
