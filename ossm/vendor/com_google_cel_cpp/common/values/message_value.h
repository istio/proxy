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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_MESSAGE_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_MESSAGE_VALUE_H_

#include <cstdint>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "absl/utility/utility.h"
#include "base/attribute.h"
#include "common/json.h"
#include "common/optional_ref.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/parsed_message_value.h"
#include "common/values/struct_value_interface.h"
#include "common/values/values.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message_lite.h"

namespace cel {

class Value;
class ValueManager;
class StructValue;

class MessageValue final {
 public:
  static constexpr ValueKind kKind = ValueKind::kStruct;

  // NOLINTNEXTLINE(google-explicit-constructor)
  MessageValue(const ParsedMessageValue& other)
      : variant_(absl::in_place_type<ParsedMessageValue>, other) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  MessageValue(ParsedMessageValue&& other)
      : variant_(absl::in_place_type<ParsedMessageValue>, std::move(other)) {}

  // Places the `MessageValue` into an unspecified state. Anything except
  // assigning to `MessageValue` is undefined behavior.
  MessageValue() = default;
  MessageValue(const MessageValue&) = default;
  MessageValue(MessageValue&&) = default;
  MessageValue& operator=(const MessageValue&) = default;
  MessageValue& operator=(MessageValue&&) = default;

  static ValueKind kind() { return kKind; }

  absl::string_view GetTypeName() const { return GetDescriptor()->full_name(); }

  MessageType GetRuntimeType() const { return MessageType(GetDescriptor()); }

  absl::Nonnull<const google::protobuf::Descriptor*> GetDescriptor() const;

  bool IsZeroValue() const;

  std::string DebugString() const;

  absl::Status SerializeTo(AnyToJsonConverter& converter,
                           absl::Cord& value) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter& converter) const;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

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

  bool IsParsed() const {
    return absl::holds_alternative<ParsedMessageValue>(variant_);
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>, bool> Is() const {
    return IsParsed();
  }

  cel::optional_ref<const ParsedMessageValue> AsParsed() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).AsParsed();
  }
  cel::optional_ref<const ParsedMessageValue> AsParsed()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<ParsedMessageValue> AsParsed() &&;
  absl::optional<ParsedMessageValue> AsParsed() const&& {
    return common_internal::AsOptional(AsParsed());
  }

  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                       cel::optional_ref<const ParsedMessageValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsParsed();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                   cel::optional_ref<const ParsedMessageValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return IsParsed();
  }
  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                       absl::optional<ParsedMessageValue>>
      As() && ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::move(*this).AsParsed();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                   absl::optional<ParsedMessageValue>>
  As() const&& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::move(*this).AsParsed();
  }

  const ParsedMessageValue& GetParsed() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).GetParsed();
  }
  const ParsedMessageValue& GetParsed() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  ParsedMessageValue GetParsed() &&;
  ParsedMessageValue GetParsed() const&& { return GetParsed(); }

  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                       const ParsedMessageValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsed();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                   const ParsedMessageValue&>
  Get() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsed();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>, ParsedMessageValue>
  Get() && {
    return std::move(*this).GetParsed();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>, ParsedMessageValue>
  Get() const&& {
    return std::move(*this).GetParsed();
  }

  explicit operator bool() const {
    return !absl::holds_alternative<absl::monostate>(variant_);
  }

  friend void swap(MessageValue& lhs, MessageValue& rhs) noexcept {
    lhs.variant_.swap(rhs.variant_);
  }

 private:
  friend class Value;
  friend class StructValue;

  common_internal::ValueVariant ToValueVariant() const&;
  common_internal::ValueVariant ToValueVariant() &&;

  common_internal::StructValueVariant ToStructValueVariant() const&;
  common_internal::StructValueVariant ToStructValueVariant() &&;

  absl::variant<absl::monostate, ParsedMessageValue> variant_;
};

inline std::ostream& operator<<(std::ostream& out, const MessageValue& value) {
  return out << value.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_MESSAGE_VALUE_H_
