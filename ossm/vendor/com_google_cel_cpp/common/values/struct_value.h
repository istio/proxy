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
#include "absl/log/absl_check.h"
#include "absl/meta/type_traits.h"
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
#include "common/native_type.h"
#include "common/optional_ref.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/legacy_struct_value.h"  // IWYU pragma: export
#include "common/values/message_value.h"
#include "common/values/parsed_message_value.h"
#include "common/values/parsed_struct_value.h"  // IWYU pragma: export
#include "common/values/struct_value_interface.h"  // IWYU pragma: export
#include "common/values/values.h"
#include "runtime/runtime_options.h"

namespace cel {

class StructValueInterface;
class StructValue;
class Value;
class ValueManager;
class TypeManager;

class StructValue final {
 public:
  using interface_type = StructValueInterface;

  static constexpr ValueKind kKind = StructValueInterface::kKind;

  // Copy constructor for alternative struct values.
  template <
      typename T,
      typename = std::enable_if_t<
          common_internal::IsStructValueAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  StructValue(const T& value)
      : variant_(
            absl::in_place_type<common_internal::BaseStructValueAlternativeForT<
                absl::remove_cvref_t<T>>>,
            value) {}

  // Move constructor for alternative struct values.
  template <
      typename T,
      typename = std::enable_if_t<
          common_internal::IsStructValueAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  StructValue(T&& value)
      : variant_(
            absl::in_place_type<common_internal::BaseStructValueAlternativeForT<
                absl::remove_cvref_t<T>>>,
            std::forward<T>(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  StructValue(const MessageValue& other)
      : variant_(other.ToStructValueVariant()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  StructValue(MessageValue&& other)
      : variant_(std::move(other).ToStructValueVariant()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  StructValue(const ParsedMessageValue& other)
      : variant_(absl::in_place_type<ParsedMessageValue>, other) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  StructValue(ParsedMessageValue&& other)
      : variant_(absl::in_place_type<ParsedMessageValue>, std::move(other)) {}

  StructValue() = default;

  StructValue(const StructValue& other)
      : variant_((other.AssertIsValid(), other.variant_)) {}

  StructValue(StructValue&& other) noexcept
      : variant_((other.AssertIsValid(), std::move(other.variant_))) {}

  StructValue& operator=(const StructValue& other) {
    other.AssertIsValid();
    ABSL_DCHECK(this != std::addressof(other))
        << "StructValue should not be copied to itself";
    variant_ = other.variant_;
    return *this;
  }

  StructValue& operator=(StructValue&& other) noexcept {
    other.AssertIsValid();
    ABSL_DCHECK(this != std::addressof(other))
        << "StructValue should not be moved to itself";
    variant_ = std::move(other.variant_);
    other.variant_.emplace<absl::monostate>();
    return *this;
  }

  constexpr ValueKind kind() const { return kKind; }

  StructType GetRuntimeType() const;

  absl::string_view GetTypeName() const;

  std::string DebugString() const;

  absl::Status SerializeTo(AnyToJsonConverter& converter,
                           absl::Cord& value) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter& converter) const;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const;

  void swap(StructValue& other) noexcept {
    AssertIsValid();
    other.AssertIsValid();
    variant_.swap(other.variant_);
  }

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

  // Returns `true` if this value is an instance of a message value. If `true`
  // is returned, it is implied that `IsOpaque()` would also return true.
  bool IsMessage() const { return IsParsedMessage(); }

  // Returns `true` if this value is an instance of a parsed message value. If
  // `true` is returned, it is implied that `IsMessage()` would also return
  // true.
  bool IsParsedMessage() const {
    return absl::holds_alternative<ParsedMessageValue>(variant_);
  }

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
  As() &;
  template <typename T>
  std::enable_if_t<std::is_same_v<MessageValue, T>,
                   absl::optional<MessageValue>>
  As() const&;
  template <typename T>
  std::enable_if_t<std::is_same_v<MessageValue, T>,
                   absl::optional<MessageValue>>
  As() &&;
  template <typename T>
  std::enable_if_t<std::is_same_v<MessageValue, T>,
                   absl::optional<MessageValue>>
  As() const&&;

  // Convenience method for use with template metaprogramming. See
  // `AsParsedMessage()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                       optional_ref<const ParsedMessageValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND;
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                   optional_ref<const ParsedMessageValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                   absl::optional<ParsedMessageValue>>
  As() &&;
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                   absl::optional<ParsedMessageValue>>
  As() const&&;

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

 private:
  friend class Value;
  friend struct NativeTypeTraits<StructValue>;

  common_internal::ValueVariant ToValueVariant() const&;
  common_internal::ValueVariant ToValueVariant() &&;

  constexpr bool IsValid() const {
    return !absl::holds_alternative<absl::monostate>(variant_);
  }

  void AssertIsValid() const {
    ABSL_DCHECK(IsValid()) << "use of invalid StructValue";
  }

  // Unlike many of the other derived values, `StructValue` is itself a composed
  // type. This is to avoid making `StructValue` too big and by extension
  // `Value` too big. Instead we store the derived `StructValue` values in
  // `Value` and not `StructValue` itself.
  common_internal::StructValueVariant variant_;
};

inline void swap(StructValue& lhs, StructValue& rhs) noexcept { lhs.swap(rhs); }

inline std::ostream& operator<<(std::ostream& out, const StructValue& value) {
  return out << value.DebugString();
}

template <typename T>
inline std::enable_if_t<std::is_same_v<MessageValue, T>,
                        absl::optional<MessageValue>>
StructValue::As() & {
  return AsMessage();
}

template <typename T>
inline std::enable_if_t<std::is_same_v<MessageValue, T>,
                        absl::optional<MessageValue>>
StructValue::As() const& {
  return AsMessage();
}

template <typename T>
inline std::enable_if_t<std::is_same_v<MessageValue, T>,
                        absl::optional<MessageValue>>
StructValue::As() && {
  return std::move(*this).AsMessage();
}

template <typename T>
inline std::enable_if_t<std::is_same_v<MessageValue, T>,
                        absl::optional<MessageValue>>
StructValue::As() const&& {
  return std::move(*this).AsMessage();
}

template <typename T>
    inline std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                            optional_ref<const ParsedMessageValue>>
    StructValue::As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return AsParsedMessage();
}

template <typename T>
inline std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                        optional_ref<const ParsedMessageValue>>
StructValue::As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return AsParsedMessage();
}

template <typename T>
inline std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                        absl::optional<ParsedMessageValue>>
StructValue::As() && {
  return std::move(*this).AsParsedMessage();
}

template <typename T>
inline std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                        absl::optional<ParsedMessageValue>>
StructValue::As() const&& {
  return std::move(*this).AsParsedMessage();
}

template <>
struct NativeTypeTraits<StructValue> final {
  static NativeTypeId Id(const StructValue& value) {
    value.AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> NativeTypeId {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return
            // `NativeTypeId::For<absl::monostate>()`. In debug builds we cannot
            // reach here.
            return NativeTypeId::For<absl::monostate>();
          } else {
            return NativeTypeId::Of(alternative);
          }
        },
        value.variant_);
  }

  static bool SkipDestructor(const StructValue& value) {
    value.AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> bool {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just say we should skip the destructor.
            // In debug builds we cannot reach here.
            return true;
          } else {
            return NativeType::SkipDestructor(alternative);
          }
        },
        value.variant_);
  }
};

class StructValueBuilder {
 public:
  virtual ~StructValueBuilder() = default;

  virtual absl::Status SetFieldByName(absl::string_view name, Value value) = 0;

  virtual absl::Status SetFieldByNumber(int64_t number, Value value) = 0;

  virtual absl::StatusOr<StructValue> Build() && = 0;
};

using StructValueBuilderPtr = std::unique_ptr<StructValueBuilder>;

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_STRUCT_VALUE_H_
