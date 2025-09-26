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

// IWYU pragma: private, include "common/value.h"
// IWYU pragma: friend "common/value.h"

// `StructValue` is the value representation of `StructType`. `StructValue`
// itself is a composed type of more specific runtime representations.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_STRUCT_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_STRUCT_VALUE_H_

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/utility/utility.h"
#include "base/attribute.h"
#include "common/native_type.h"
#include "common/optional_ref.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/custom_struct_value.h"
#include "common/values/legacy_struct_value.h"
#include "common/values/message_value.h"
#include "common/values/parsed_message_value.h"
#include "common/values/struct_value_variant.h"
#include "common/values/values.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

class StructValue;
class Value;

class StructValue final
    : private common_internal::StructValueMixin<StructValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kStruct;

  template <
      typename T,
      typename = std::enable_if_t<
          common_internal::IsStructValueAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  StructValue(T&& value)
      : variant_(absl::in_place_type<absl::remove_cvref_t<T>>,
                 std::forward<T>(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  StructValue(const MessageValue& other)
      : variant_(other.ToStructValueVariant()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  StructValue(MessageValue&& other)
      : variant_(std::move(other).ToStructValueVariant()) {}

  StructValue() = default;
  StructValue(const StructValue&) = default;
  StructValue(StructValue&& other) = default;
  StructValue& operator=(const StructValue&) = default;
  StructValue& operator=(StructValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  StructType GetRuntimeType() const;

  absl::string_view GetTypeName() const;

  NativeTypeId GetTypeId() const;

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

  // Like ConvertToJson(), except `json` **MUST** be an instance of
  // `google.protobuf.Struct`.
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

  bool IsZeroValue() const;

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

  // Returns `true` if this value is an instance of a message value. If `true`
  // is returned, it is implied that `IsOpaque()` would also return true.
  bool IsMessage() const { return IsParsedMessage(); }

  // Returns `true` if this value is an instance of a parsed message value. If
  // `true` is returned, it is implied that `IsMessage()` would also return
  // true.
  bool IsParsedMessage() const { return variant_.Is<ParsedMessageValue>(); }

  // Convenience method for use with template metaprogramming. See
  // `IsMessage()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<MessageValue, T>, bool> Is() const {
    return IsMessage();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsParsedMessage()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>, bool> Is() const {
    return IsParsedMessage();
  }

  // Performs a checked cast from a value to a message value,
  // returning a non-empty optional with either a value or reference to the
  // message value. Otherwise an empty optional is returned.
  absl::optional<MessageValue> AsMessage() & {
    return std::as_const(*this).AsMessage();
  }
  absl::optional<MessageValue> AsMessage() const&;
  absl::optional<MessageValue> AsMessage() &&;
  absl::optional<MessageValue> AsMessage() const&& { return AsMessage(); }

  // Performs a checked cast from a value to a parsed message value,
  // returning a non-empty optional with either a value or reference to the
  // parsed message value. Otherwise an empty optional is returned.
  optional_ref<const ParsedMessageValue> AsParsedMessage() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).AsParsedMessage();
  }
  optional_ref<const ParsedMessageValue> AsParsedMessage()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<ParsedMessageValue> AsParsedMessage() &&;
  absl::optional<ParsedMessageValue> AsParsedMessage() const&& {
    return common_internal::AsOptional(AsParsedMessage());
  }

  // Convenience method for use with template metaprogramming. See
  // `AsMessage()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<MessageValue, T>,
                   absl::optional<MessageValue>>
  As() & {
    return AsMessage();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<MessageValue, T>,
                   absl::optional<MessageValue>>
  As() const& {
    return AsMessage();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<MessageValue, T>,
                   absl::optional<MessageValue>>
  As() && {
    return std::move(*this).AsMessage();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<MessageValue, T>,
                   absl::optional<MessageValue>>
  As() const&& {
    return std::move(*this).AsMessage();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsParsedMessage()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                       optional_ref<const ParsedMessageValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsParsedMessage();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                   optional_ref<const ParsedMessageValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsParsedMessage();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                   absl::optional<ParsedMessageValue>>
  As() && {
    return std::move(*this).AsParsedMessage();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                   absl::optional<ParsedMessageValue>>
  As() const&& {
    return std::move(*this).AsParsedMessage();
  }

  // Performs an unchecked cast from a value to a message value. In
  // debug builds a best effort is made to crash. If `IsMessage()` would return
  // false, calling this method is undefined behavior.
  MessageValue GetMessage() & { return std::as_const(*this).GetMessage(); }
  MessageValue GetMessage() const&;
  MessageValue GetMessage() &&;
  MessageValue GetMessage() const&& { return GetMessage(); }

  // Performs an unchecked cast from a value to a parsed message value. In
  // debug builds a best effort is made to crash. If `IsParsedMessage()` would
  // return false, calling this method is undefined behavior.
  const ParsedMessageValue& GetParsedMessage() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).GetParsedMessage();
  }
  const ParsedMessageValue& GetParsedMessage()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  ParsedMessageValue GetParsedMessage() &&;
  ParsedMessageValue GetParsedMessage() const&& { return GetParsedMessage(); }

  // Convenience method for use with template metaprogramming. See
  // `GetMessage()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<MessageValue, T>, MessageValue> Get() & {
    return GetMessage();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<MessageValue, T>, MessageValue> Get() const& {
    return GetMessage();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<MessageValue, T>, MessageValue> Get() && {
    return std::move(*this).GetMessage();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<MessageValue, T>, MessageValue> Get()
      const&& {
    return std::move(*this).GetMessage();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetParsedMessage()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                       const ParsedMessageValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsedMessage();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                   const ParsedMessageValue&>
  Get() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsedMessage();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>, ParsedMessageValue>
  Get() && {
    return std::move(*this).GetParsedMessage();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>, ParsedMessageValue>
  Get() const&& {
    return std::move(*this).GetParsedMessage();
  }

  friend void swap(StructValue& lhs, StructValue& rhs) noexcept {
    using std::swap;
    swap(lhs.variant_, rhs.variant_);
  }

 private:
  friend class Value;
  friend class common_internal::ValueMixin<StructValue>;
  friend class common_internal::StructValueMixin<StructValue>;

  common_internal::ValueVariant ToValueVariant() const&;
  common_internal::ValueVariant ToValueVariant() &&;

  // Unlike many of the other derived values, `StructValue` is itself a composed
  // type. This is to avoid making `StructValue` too big and by extension
  // `Value` too big. Instead we store the derived `StructValue` values in
  // `Value` and not `StructValue` itself.
  common_internal::StructValueVariant variant_;
};

inline std::ostream& operator<<(std::ostream& out, const StructValue& value) {
  return out << value.DebugString();
}

template <>
struct NativeTypeTraits<StructValue> final {
  static NativeTypeId Id(const StructValue& value) { return value.GetTypeId(); }
};

class StructValueBuilder {
 public:
  virtual ~StructValueBuilder() = default;

  virtual absl::StatusOr<absl::optional<ErrorValue>> SetFieldByName(
      absl::string_view name, Value value) = 0;

  virtual absl::StatusOr<absl::optional<ErrorValue>> SetFieldByNumber(
      int64_t number, Value value) = 0;

  virtual absl::StatusOr<StructValue> Build() && = 0;
};

using StructValueBuilderPtr = std::unique_ptr<StructValueBuilder>;

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_STRUCT_VALUE_H_
