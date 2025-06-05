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
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "common/allocator.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/struct_value_interface.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

class MessageValue;
class StructValue;
class Value;
class ValueManager;

class ParsedMessageValue final {
 public:
  static constexpr ValueKind kKind = ValueKind::kStruct;

  using element_type = const google::protobuf::Message;

  explicit ParsedMessageValue(Owned<const google::protobuf::Message> value)
      : value_(std::move(value)) {
    ABSL_DCHECK(!value_ || !IsWellKnownMessageType(value_->GetDescriptor()))
        << value_->GetTypeName() << " is a well known type";
    ABSL_DCHECK(!value_ || value_->GetReflection() != nullptr)
        << value_->GetTypeName() << " is missing reflection";
  }

  // Places the `ParsedMessageValue` into an invalid state. Anything except
  // assigning to `MessageValue` is undefined behavior.
  ParsedMessageValue() = default;

  ParsedMessageValue(const ParsedMessageValue&) = default;
  ParsedMessageValue(ParsedMessageValue&&) = default;
  ParsedMessageValue& operator=(const ParsedMessageValue&) = default;
  ParsedMessageValue& operator=(ParsedMessageValue&&) = default;

  static ValueKind kind() { return kKind; }

  Allocator<> get_allocator() const { return Allocator<>(value_.arena()); }

  absl::string_view GetTypeName() const { return GetDescriptor()->full_name(); }

  MessageType GetRuntimeType() const { return MessageType(GetDescriptor()); }

  absl::Nonnull<const google::protobuf::Descriptor*> GetDescriptor() const {
    return (*this)->GetDescriptor();
  }

  absl::Nonnull<const google::protobuf::Reflection*> GetReflection() const {
    return (*this)->GetReflection();
  }

  const google::protobuf::Message& operator*() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(*this);
    return *value_;
  }

  absl::Nonnull<const google::protobuf::Message*> operator->() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(*this);
    return value_.operator->();
  }

  bool IsZeroValue() const;

  std::string DebugString() const;

  absl::Status SerializeTo(AnyToJsonConverter& converter,
                           absl::Cord& value) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter& converter) const;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  ParsedMessageValue Clone(Allocator<> allocator) const;

  absl::Status GetFieldByName(ValueManager& value_manager,
                              absl::string_view name, Value& result,
                              ProtoWrapperTypeOptions unboxing_options =
                                  ProtoWrapperTypeOptions::kUnsetNull) const;
  absl::StatusOr<Value> GetFieldByName(
      ValueManager& value_manager, absl::string_view name,
      ProtoWrapperTypeOptions unboxing_options =
          ProtoWrapperTypeOptions::kUnsetNull) const;

  absl::Status GetFieldByNumber(ValueManager& value_manager, int64_t number,
                                Value& result,
                                ProtoWrapperTypeOptions unboxing_options =
                                    ProtoWrapperTypeOptions::kUnsetNull) const;
  absl::StatusOr<Value> GetFieldByNumber(
      ValueManager& value_manager, int64_t number,
      ProtoWrapperTypeOptions unboxing_options =
          ProtoWrapperTypeOptions::kUnsetNull) const;

  absl::StatusOr<bool> HasFieldByName(absl::string_view name) const;

  absl::StatusOr<bool> HasFieldByNumber(int64_t number) const;

  using ForEachFieldCallback = StructValueInterface::ForEachFieldCallback;

  absl::Status ForEachField(ValueManager& value_manager,
                            ForEachFieldCallback callback) const;

  absl::StatusOr<int> Qualify(ValueManager& value_manager,
                              absl::Span<const SelectQualifier> qualifiers,
                              bool presence_test, Value& result) const;
  absl::StatusOr<std::pair<Value, int>> Qualify(
      ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
      bool presence_test) const;

  // Returns `true` if `ParsedMessageValue` is in a valid state.
  explicit operator bool() const { return static_cast<bool>(value_); }

  friend void swap(ParsedMessageValue& lhs, ParsedMessageValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
  }

 private:
  friend std::pointer_traits<ParsedMessageValue>;
  friend class StructValue;

  absl::Status GetField(ValueManager& value_manager,
                        absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
                        Value& result,
                        ProtoWrapperTypeOptions unboxing_options) const;

  bool HasField(absl::Nonnull<const google::protobuf::FieldDescriptor*> field) const;

  Owned<const google::protobuf::Message> value_;
};

inline std::ostream& operator<<(std::ostream& out,
                                const ParsedMessageValue& value) {
  return out << value.DebugString();
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
