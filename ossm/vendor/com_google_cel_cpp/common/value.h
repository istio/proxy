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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUE_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
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
#include "common/allocator.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/optional_ref.h"
#include "common/type.h"
#include "common/value_interface.h"  // IWYU pragma: export
#include "common/value_kind.h"
#include "common/values/bool_value.h"  // IWYU pragma: export
#include "common/values/bytes_value.h"  // IWYU pragma: export
#include "common/values/double_value.h"  // IWYU pragma: export
#include "common/values/duration_value.h"  // IWYU pragma: export
#include "common/values/enum_value.h"  // IWYU pragma: export
#include "common/values/error_value.h"  // IWYU pragma: export
#include "common/values/int_value.h"  // IWYU pragma: export
#include "common/values/list_value.h"  // IWYU pragma: export
#include "common/values/map_value.h"  // IWYU pragma: export
#include "common/values/message_value.h"  // IWYU pragma: export
#include "common/values/null_value.h"  // IWYU pragma: export
#include "common/values/opaque_value.h"  // IWYU pragma: export
#include "common/values/optional_value.h"  // IWYU pragma: export
#include "common/values/parsed_json_list_value.h"  // IWYU pragma: export
#include "common/values/parsed_json_map_value.h"  // IWYU pragma: export
#include "common/values/parsed_map_field_value.h"  // IWYU pragma: export
#include "common/values/parsed_message_value.h"  // IWYU pragma: export
#include "common/values/parsed_repeated_field_value.h"  // IWYU pragma: export
#include "common/values/string_value.h"  // IWYU pragma: export
#include "common/values/struct_value.h"  // IWYU pragma: export
#include "common/values/timestamp_value.h"  // IWYU pragma: export
#include "common/values/type_value.h"  // IWYU pragma: export
#include "common/values/uint_value.h"  // IWYU pragma: export
#include "common/values/unknown_value.h"  // IWYU pragma: export
#include "common/values/values.h"
#include "internal/status_macros.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/generated_enum_reflection.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/message.h"

namespace cel {

// `Value` is a composition type which encompasses all values supported by the
// Common Expression Language. When default constructed or moved, `Value` is in
// a known but invalid state. Any attempt to use it from then on, without
// assigning another type, is undefined behavior. In debug builds, we do our
// best to fail.
class Value final {
 public:
  // Returns an appropriate `Value` for the dynamic protobuf enum. For open
  // enums, returns `cel::IntValue`. For closed enums, returns `cel::ErrorValue`
  // if the value is not present in the enum otherwise returns `cel::IntValue`.
  static Value Enum(absl::Nonnull<const google::protobuf::EnumValueDescriptor*> value);
  static Value Enum(absl::Nonnull<const google::protobuf::EnumDescriptor*> type,
                    int32_t number);

  // SFINAE overload for generated protobuf enums which are not well-known.
  // Always returns `cel::IntValue`.
  template <typename T>
  static common_internal::EnableIfGeneratedEnum<T, IntValue> Enum(T value) {
    return IntValue(value);
  }

  // SFINAE overload for google::protobuf::NullValue. Always returns
  // `cel::NullValue`.
  template <typename T>
  static common_internal::EnableIfWellKnownEnum<T, google::protobuf::NullValue,
                                                NullValue>
  Enum(T) {
    return NullValue();
  }

  // Returns an appropriate `Value` for the dynamic protobuf message. If
  // `message` is the well known type `google.protobuf.Any`, `descriptor_pool`
  // and `message_factory` will be used to unpack the value. Both must outlive
  // the resulting value and any of its shallow copies.
  static Value Message(Allocator<> allocator, const google::protobuf::Message& message,
                       absl::Nonnull<const google::protobuf::DescriptorPool*>
                           descriptor_pool ABSL_ATTRIBUTE_LIFETIME_BOUND,
                       absl::Nonnull<google::protobuf::MessageFactory*> message_factory
                           ABSL_ATTRIBUTE_LIFETIME_BOUND);
  static Value Message(Allocator<> allocator, google::protobuf::Message&& message,
                       absl::Nonnull<const google::protobuf::DescriptorPool*>
                           descriptor_pool ABSL_ATTRIBUTE_LIFETIME_BOUND,
                       absl::Nonnull<google::protobuf::MessageFactory*> message_factory
                           ABSL_ATTRIBUTE_LIFETIME_BOUND);
  static Value Message(Borrowed<const google::protobuf::Message> message,
                       absl::Nonnull<const google::protobuf::DescriptorPool*>
                           descriptor_pool ABSL_ATTRIBUTE_LIFETIME_BOUND,
                       absl::Nonnull<google::protobuf::MessageFactory*> message_factory
                           ABSL_ATTRIBUTE_LIFETIME_BOUND);

  // Returns an appropriate `Value` for the dynamic protobuf message field. If
  // `field` in `message` is the well known type `google.protobuf.Any`,
  // `descriptor_pool` and `message_factory` will be used to unpack the value.
  // Both must outlive the resulting value and any of its shallow copies.
  static Value Field(Borrowed<const google::protobuf::Message> message,
                     absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
                     ProtoWrapperTypeOptions wrapper_type_options =
                         ProtoWrapperTypeOptions::kUnsetNull);
  static Value Field(Borrowed<const google::protobuf::Message> message,
                     absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
                     absl::Nonnull<const google::protobuf::DescriptorPool*>
                         descriptor_pool ABSL_ATTRIBUTE_LIFETIME_BOUND,
                     absl::Nonnull<google::protobuf::MessageFactory*> message_factory
                         ABSL_ATTRIBUTE_LIFETIME_BOUND,
                     ProtoWrapperTypeOptions wrapper_type_options =
                         ProtoWrapperTypeOptions::kUnsetNull);

  // Returns an appropriate `Value` for the dynamic protobuf message repeated
  // field. If `field` in `message` is the well known type
  // `google.protobuf.Any`, `descriptor_pool` and `message_factory` will be used
  // to unpack the value. Both must outlive the resulting value and any of its
  // shallow copies.
  static Value RepeatedField(
      Borrowed<const google::protobuf::Message> message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index);
  static Value RepeatedField(
      Borrowed<const google::protobuf::Message> message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory
          ABSL_ATTRIBUTE_LIFETIME_BOUND);

  // Returns an appropriate `StringValue` for the dynamic protobuf message map
  // field key. The map field key must be a string or the behavior is undefined.
  static StringValue MapFieldKeyString(Borrowed<const google::protobuf::Message> message,
                                       const google::protobuf::MapKey& key);

  // Returns an appropriate `Value` for the dynamic protobuf message map
  // field value. If `field` in `message`, which is `value`, is the well known
  // type `google.protobuf.Any`, `descriptor_pool` and `message_factory` will be
  // used to unpack the value. Both must outlive the resulting value and any of
  // its shallow copies.
  static Value MapFieldValue(
      Borrowed<const google::protobuf::Message> message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      const google::protobuf::MapValueConstRef& value);
  static Value MapFieldValue(
      Borrowed<const google::protobuf::Message> message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      const google::protobuf::MapValueConstRef& value,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory
          ABSL_ATTRIBUTE_LIFETIME_BOUND);

  Value() = default;
  Value(const Value&) = default;
  Value& operator=(const Value&) = default;
  Value(Value&& other) = default;
  Value& operator=(Value&&) = default;

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const ListValue& value) : variant_(value.ToValueVariant()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(ListValue&& value) : variant_(std::move(value).ToValueVariant()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(const ListValue& value) {
    variant_ = value.ToValueVariant();
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(ListValue&& value) {
    variant_ = std::move(value).ToValueVariant();
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const ParsedRepeatedFieldValue& value)
      : variant_(absl::in_place_type<ParsedRepeatedFieldValue>, value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(ParsedRepeatedFieldValue&& value)
      : variant_(absl::in_place_type<ParsedRepeatedFieldValue>,
                 std::move(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(const ParsedRepeatedFieldValue& value) {
    variant_.emplace<ParsedRepeatedFieldValue>(value);
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(ParsedRepeatedFieldValue&& value) {
    variant_.emplace<ParsedRepeatedFieldValue>(std::move(value));
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const ParsedJsonListValue& value)
      : variant_(absl::in_place_type<ParsedJsonListValue>, value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(ParsedJsonListValue&& value)
      : variant_(absl::in_place_type<ParsedJsonListValue>, std::move(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(const ParsedJsonListValue& value) {
    variant_.emplace<ParsedJsonListValue>(value);
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(ParsedJsonListValue&& value) {
    variant_.emplace<ParsedJsonListValue>(std::move(value));
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const MapValue& value) : variant_(value.ToValueVariant()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(MapValue&& value) : variant_(std::move(value).ToValueVariant()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(const MapValue& value) {
    variant_ = value.ToValueVariant();
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(MapValue&& value) {
    variant_ = std::move(value).ToValueVariant();
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const ParsedMapFieldValue& value)
      : variant_(absl::in_place_type<ParsedMapFieldValue>, value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(ParsedMapFieldValue&& value)
      : variant_(absl::in_place_type<ParsedMapFieldValue>, std::move(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(const ParsedMapFieldValue& value) {
    variant_.emplace<ParsedMapFieldValue>(value);
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(ParsedMapFieldValue&& value) {
    variant_.emplace<ParsedMapFieldValue>(std::move(value));
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const ParsedJsonMapValue& value)
      : variant_(absl::in_place_type<ParsedJsonMapValue>, value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(ParsedJsonMapValue&& value)
      : variant_(absl::in_place_type<ParsedJsonMapValue>, std::move(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(const ParsedJsonMapValue& value) {
    variant_.emplace<ParsedJsonMapValue>(value);
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(ParsedJsonMapValue&& value) {
    variant_.emplace<ParsedJsonMapValue>(std::move(value));
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const StructValue& value) : variant_(value.ToValueVariant()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(StructValue&& value) : variant_(std::move(value).ToValueVariant()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(const StructValue& value) {
    variant_ = value.ToValueVariant();
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(StructValue&& value) {
    variant_ = std::move(value).ToValueVariant();
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const MessageValue& value) : variant_(value.ToValueVariant()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(MessageValue&& value) : variant_(std::move(value).ToValueVariant()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(const MessageValue& value) {
    variant_ = value.ToValueVariant();
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(MessageValue&& value) {
    variant_ = std::move(value).ToValueVariant();
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const ParsedMessageValue& value)
      : variant_(absl::in_place_type<ParsedMessageValue>, value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(ParsedMessageValue&& value)
      : variant_(absl::in_place_type<ParsedMessageValue>, std::move(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(const ParsedMessageValue& value) {
    variant_.emplace<ParsedMessageValue>(value);
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(ParsedMessageValue&& value) {
    variant_.emplace<ParsedMessageValue>(std::move(value));
    return *this;
  }

  template <typename T,
            typename = std::enable_if_t<common_internal::IsValueInterfaceV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const Shared<const T>& interface) noexcept
      : variant_(
            absl::in_place_type<common_internal::BaseValueAlternativeForT<T>>,
            interface) {}

  template <typename T,
            typename = std::enable_if_t<common_internal::IsValueInterfaceV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(Shared<const T>&& interface) noexcept
      : variant_(
            absl::in_place_type<common_internal::BaseValueAlternativeForT<T>>,
            std::move(interface)) {}

  template <typename T,
            typename = std::enable_if_t<
                common_internal::IsValueAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(T&& alternative) noexcept
      : variant_(absl::in_place_type<common_internal::BaseValueAlternativeForT<
                     absl::remove_cvref_t<T>>>,
                 std::forward<T>(alternative)) {}

  template <typename T,
            typename = std::enable_if_t<
                common_internal::IsValueAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(T&& type) noexcept {
    variant_.emplace<
        common_internal::BaseValueAlternativeForT<absl::remove_cvref_t<T>>>(
        std::forward<T>(type));
    return *this;
  }

  ValueKind kind() const;

  Type GetRuntimeType() const;

  absl::string_view GetTypeName() const;

  std::string DebugString() const;

  // `SerializeTo` serializes this value and appends it to `value`. If this
  // value does not support serialization, `FAILED_PRECONDITION` is returned.
  absl::Status SerializeTo(AnyToJsonConverter& value_manager,
                           absl::Cord& value) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter& value_manager) const;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const;

  // Clones the value to another allocator, if necessary. For compatible
  // allocators, no allocation is performed. The exact logic for whether
  // allocators are compatible is a little fuzzy at the moment, so avoid calling
  // this function as it should be considered experimental.
  Value Clone(Allocator<> allocator) const;

  friend void swap(Value& lhs, Value& rhs) noexcept;

  friend std::ostream& operator<<(std::ostream& out, const Value& value);

  ABSL_DEPRECATED("Just use operator.()")
  Value* operator->() { return this; }

  ABSL_DEPRECATED("Just use operator.()")
  const Value* operator->() const { return this; }

  // Returns `true` if this value is an instance of a bool value.
  bool IsBool() const { return absl::holds_alternative<BoolValue>(variant_); }

  // Returns `true` if this value is an instance of a bool value and true.
  bool IsTrue() const { return IsBool() && GetBool().NativeValue(); }

  // Returns `true` if this value is an instance of a bool value and false.
  bool IsFalse() const { return IsBool() && !GetBool().NativeValue(); }

  // Returns `true` if this value is an instance of a bytes value.
  bool IsBytes() const { return absl::holds_alternative<BytesValue>(variant_); }

  // Returns `true` if this value is an instance of a double value.
  bool IsDouble() const {
    return absl::holds_alternative<DoubleValue>(variant_);
  }

  // Returns `true` if this value is an instance of a duration value.
  bool IsDuration() const {
    return absl::holds_alternative<DurationValue>(variant_);
  }

  // Returns `true` if this value is an instance of an error value.
  bool IsError() const { return absl::holds_alternative<ErrorValue>(variant_); }

  // Returns `true` if this value is an instance of an int value.
  bool IsInt() const { return absl::holds_alternative<IntValue>(variant_); }

  // Returns `true` if this value is an instance of a list value.
  bool IsList() const {
    return absl::holds_alternative<common_internal::LegacyListValue>(
               variant_) ||
           absl::holds_alternative<ParsedListValue>(variant_) ||
           absl::holds_alternative<ParsedRepeatedFieldValue>(variant_) ||
           absl::holds_alternative<ParsedJsonListValue>(variant_);
  }

  // Returns `true` if this value is an instance of a map value.
  bool IsMap() const {
    return absl::holds_alternative<common_internal::LegacyMapValue>(variant_) ||
           absl::holds_alternative<ParsedMapValue>(variant_) ||
           absl::holds_alternative<ParsedMapFieldValue>(variant_) ||
           absl::holds_alternative<ParsedJsonMapValue>(variant_);
  }

  // Returns `true` if this value is an instance of a message value. If `true`
  // is returned, it is implied that `IsStruct()` would also return true.
  bool IsMessage() const {
    return absl::holds_alternative<ParsedMessageValue>(variant_);
  }

  // Returns `true` if this value is an instance of a null value.
  bool IsNull() const { return absl::holds_alternative<NullValue>(variant_); }

  // Returns `true` if this value is an instance of an opaque value.
  bool IsOpaque() const {
    return absl::holds_alternative<OpaqueValue>(variant_);
  }

  // Returns `true` if this value is an instance of an optional value. If `true`
  // is returned, it is implied that `IsOpaque()` would also return true.
  bool IsOptional() const {
    if (const auto* alternative = absl::get_if<OpaqueValue>(&variant_);
        alternative != nullptr) {
      return alternative->IsOptional();
    }
    return false;
  }

  // Returns `true` if this value is an instance of a parsed JSON list value. If
  // `true` is returned, it is implied that `IsList()` would also return
  // true.
  bool IsParsedJsonList() const {
    return absl::holds_alternative<ParsedJsonListValue>(variant_);
  }

  // Returns `true` if this value is an instance of a parsed JSON map value. If
  // `true` is returned, it is implied that `IsMap()` would also return
  // true.
  bool IsParsedJsonMap() const {
    return absl::holds_alternative<ParsedJsonMapValue>(variant_);
  }

  // Returns `true` if this value is an instance of a parsed list value. If
  // `true` is returned, it is implied that `IsList()` would also return
  // true.
  bool IsParsedList() const {
    return absl::holds_alternative<ParsedListValue>(variant_);
  }

  // Returns `true` if this value is an instance of a parsed map value. If
  // `true` is returned, it is implied that `IsMap()` would also return
  // true.
  bool IsParsedMap() const {
    return absl::holds_alternative<ParsedMapValue>(variant_);
  }

  // Returns `true` if this value is an instance of a parsed map field value. If
  // `true` is returned, it is implied that `IsMap()` would also return
  // true.
  bool IsParsedMapField() const {
    return absl::holds_alternative<ParsedMapFieldValue>(variant_);
  }

  // Returns `true` if this value is an instance of a parsed message value. If
  // `true` is returned, it is implied that `IsMessage()` would also return
  // true.
  bool IsParsedMessage() const {
    return absl::holds_alternative<ParsedMessageValue>(variant_);
  }

  // Returns `true` if this value is an instance of a parsed repeated field
  // value. If `true` is returned, it is implied that `IsList()` would also
  // return true.
  bool IsParsedRepeatedField() const {
    return absl::holds_alternative<ParsedRepeatedFieldValue>(variant_);
  }

  // Returns `true` if this value is an instance of a parsed struct value. If
  // `true` is returned, it is implied that `IsStruct()` would also return
  // true.
  bool IsParsedStruct() const {
    return absl::holds_alternative<ParsedStructValue>(variant_);
  }

  // Returns `true` if this value is an instance of a string value.
  bool IsString() const {
    return absl::holds_alternative<StringValue>(variant_);
  }

  // Returns `true` if this value is an instance of a struct value.
  bool IsStruct() const {
    return absl::holds_alternative<common_internal::LegacyStructValue>(
               variant_) ||
           absl::holds_alternative<ParsedStructValue>(variant_) ||
           absl::holds_alternative<ParsedMessageValue>(variant_);
  }

  // Returns `true` if this value is an instance of a timestamp value.
  bool IsTimestamp() const {
    return absl::holds_alternative<TimestampValue>(variant_);
  }

  // Returns `true` if this value is an instance of a type value.
  bool IsType() const { return absl::holds_alternative<TypeValue>(variant_); }

  // Returns `true` if this value is an instance of a uint value.
  bool IsUint() const { return absl::holds_alternative<UintValue>(variant_); }

  // Returns `true` if this value is an instance of an unknown value.
  bool IsUnknown() const {
    return absl::holds_alternative<UnknownValue>(variant_);
  }

  // Convenience method for use with template metaprogramming. See
  // `IsBool()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<BoolValue, T>, bool> Is() const {
    return IsBool();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsBytes()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<BytesValue, T>, bool> Is() const {
    return IsBytes();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsDouble()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleValue, T>, bool> Is() const {
    return IsDouble();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsDuration()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<DurationValue, T>, bool> Is() const {
    return IsDuration();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsError()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<ErrorValue, T>, bool> Is() const {
    return IsError();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsInt()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<IntValue, T>, bool> Is() const {
    return IsInt();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsList()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<ListValue, T>, bool> Is() const {
    return IsList();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsMap()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<MapValue, T>, bool> Is() const {
    return IsMap();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsMessage()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<MessageValue, T>, bool> Is() const {
    return IsMessage();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsNull()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<NullValue, T>, bool> Is() const {
    return IsNull();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsOpaque()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<OpaqueValue, T>, bool> Is() const {
    return IsOpaque();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsOptional()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>, bool> Is() const {
    return IsOptional();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsParsedJsonList()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedJsonListValue, T>, bool> Is() const {
    return IsParsedJsonList();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsParsedJsonMap()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedJsonMapValue, T>, bool> Is() const {
    return IsParsedJsonMap();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsParsedList()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedListValue, T>, bool> Is() const {
    return IsParsedList();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsParsedMap()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMapValue, T>, bool> Is() const {
    return IsParsedMap();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsParsedMapField()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMapFieldValue, T>, bool> Is() const {
    return IsParsedMapField();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsParsedMessage()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>, bool> Is() const {
    return IsParsedMessage();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsParsedRepeatedField()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedRepeatedFieldValue, T>, bool> Is()
      const {
    return IsParsedRepeatedField();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsParsedStruct()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedStructValue, T>, bool> Is() const {
    return IsParsedStruct();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsString()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<StringValue, T>, bool> Is() const {
    return IsString();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsStruct()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<StructValue, T>, bool> Is() const {
    return IsStruct();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsTimestamp()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<TimestampValue, T>, bool> Is() const {
    return IsTimestamp();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsType()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<TypeValue, T>, bool> Is() const {
    return IsType();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsUint()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<UintValue, T>, bool> Is() const {
    return IsUint();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsUnknown()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<UnknownValue, T>, bool> Is() const {
    return IsUnknown();
  }

  // Performs a checked cast from a value to a bool value,
  // returning a non-empty optional with either a value or reference to the
  // bool value. Otherwise an empty optional is returned.
  absl::optional<BoolValue> AsBool() const;

  // Performs a checked cast from a value to a bytes value,
  // returning a non-empty optional with either a value or reference to the
  // bytes value. Otherwise an empty optional is returned.
  optional_ref<const BytesValue> AsBytes() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).AsBytes();
  }
  optional_ref<const BytesValue> AsBytes() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<BytesValue> AsBytes() &&;
  absl::optional<BytesValue> AsBytes() const&& {
    return common_internal::AsOptional(AsBytes());
  }

  // Performs a checked cast from a value to a double value,
  // returning a non-empty optional with either a value or reference to the
  // double value. Otherwise an empty optional is returned.
  absl::optional<DoubleValue> AsDouble() const;

  // Performs a checked cast from a value to a duration value,
  // returning a non-empty optional with either a value or reference to the
  // duration value. Otherwise an empty optional is returned.
  absl::optional<DurationValue> AsDuration() const;

  // Performs a checked cast from a value to an error value,
  // returning a non-empty optional with either a value or reference to the
  // error value. Otherwise an empty optional is returned.
  optional_ref<const ErrorValue> AsError() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).AsError();
  }
  optional_ref<const ErrorValue> AsError() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<ErrorValue> AsError() &&;
  absl::optional<ErrorValue> AsError() const&& {
    return common_internal::AsOptional(AsError());
  }

  // Performs a checked cast from a value to an int value,
  // returning a non-empty optional with either a value or reference to the
  // int value. Otherwise an empty optional is returned.
  absl::optional<IntValue> AsInt() const;

  // Performs a checked cast from a value to a list value,
  // returning a non-empty optional with either a value or reference to the
  // list value. Otherwise an empty optional is returned.
  absl::optional<ListValue> AsList() & { return std::as_const(*this).AsList(); }
  absl::optional<ListValue> AsList() const&;
  absl::optional<ListValue> AsList() &&;
  absl::optional<ListValue> AsList() const&& {
    return common_internal::AsOptional(AsList());
  }

  // Performs a checked cast from a value to a map value,
  // returning a non-empty optional with either a value or reference to the
  // map value. Otherwise an empty optional is returned.
  absl::optional<MapValue> AsMap() & { return std::as_const(*this).AsMap(); }
  absl::optional<MapValue> AsMap() const&;
  absl::optional<MapValue> AsMap() &&;
  absl::optional<MapValue> AsMap() const&& {
    return common_internal::AsOptional(AsMap());
  }

  // Performs a checked cast from a value to a message value,
  // returning a non-empty optional with either a value or reference to the
  // message value. Otherwise an empty optional is returned.
  absl::optional<MessageValue> AsMessage() & {
    return std::as_const(*this).AsMessage();
  }
  absl::optional<MessageValue> AsMessage() const&;
  absl::optional<MessageValue> AsMessage() &&;
  absl::optional<MessageValue> AsMessage() const&& {
    return common_internal::AsOptional(AsMessage());
  }

  // Performs a checked cast from a value to a null value,
  // returning a non-empty optional with either a value or reference to the
  // null value. Otherwise an empty optional is returned.
  absl::optional<NullValue> AsNull() const;

  // Performs a checked cast from a value to an opaque value,
  // returning a non-empty optional with either a value or reference to the
  // opaque value. Otherwise an empty optional is returned.
  optional_ref<const OpaqueValue> AsOpaque() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).AsOpaque();
  }
  optional_ref<const OpaqueValue> AsOpaque()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<OpaqueValue> AsOpaque() &&;
  absl::optional<OpaqueValue> AsOpaque() const&& {
    return common_internal::AsOptional(AsOpaque());
  }

  // Performs a checked cast from a value to an optional value,
  // returning a non-empty optional with either a value or reference to the
  // optional value. Otherwise an empty optional is returned.
  optional_ref<const OptionalValue> AsOptional() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).AsOptional();
  }
  optional_ref<const OptionalValue> AsOptional()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<OptionalValue> AsOptional() &&;
  absl::optional<OptionalValue> AsOptional() const&& {
    return common_internal::AsOptional(AsOptional());
  }

  // Performs a checked cast from a value to a parsed JSON list value,
  // returning a non-empty optional with either a value or reference to the
  // parsed message value. Otherwise an empty optional is returned.
  optional_ref<const ParsedJsonListValue> AsParsedJsonList() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).AsParsedJsonList();
  }
  optional_ref<const ParsedJsonListValue> AsParsedJsonList()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<ParsedJsonListValue> AsParsedJsonList() &&;
  absl::optional<ParsedJsonListValue> AsParsedJsonList() const&& {
    return common_internal::AsOptional(AsParsedJsonList());
  }

  // Performs a checked cast from a value to a parsed JSON map value,
  // returning a non-empty optional with either a value or reference to the
  // parsed message value. Otherwise an empty optional is returned.
  optional_ref<const ParsedJsonMapValue> AsParsedJsonMap() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).AsParsedJsonMap();
  }
  optional_ref<const ParsedJsonMapValue> AsParsedJsonMap()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<ParsedJsonMapValue> AsParsedJsonMap() &&;
  absl::optional<ParsedJsonMapValue> AsParsedJsonMap() const&& {
    return common_internal::AsOptional(AsParsedJsonMap());
  }

  // Performs a checked cast from a value to a parsed list value,
  // returning a non-empty optional with either a value or reference to the
  // parsed list value. Otherwise an empty optional is returned.
  optional_ref<const ParsedListValue> AsParsedList() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).AsParsedList();
  }
  optional_ref<const ParsedListValue> AsParsedList()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<ParsedListValue> AsParsedList() &&;
  absl::optional<ParsedListValue> AsParsedList() const&& {
    return common_internal::AsOptional(AsParsedList());
  }

  // Performs a checked cast from a value to a parsed map value,
  // returning a non-empty optional with either a value or reference to the
  // parsed map value. Otherwise an empty optional is returned.
  optional_ref<const ParsedMapValue> AsParsedMap() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).AsParsedMap();
  }
  optional_ref<const ParsedMapValue> AsParsedMap()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<ParsedMapValue> AsParsedMap() &&;
  absl::optional<ParsedMapValue> AsParsedMap() const&& {
    return common_internal::AsOptional(AsParsedMap());
  }

  // Performs a checked cast from a value to a parsed map field value,
  // returning a non-empty optional with either a value or reference to the
  // parsed map field value. Otherwise an empty optional is returned.
  optional_ref<const ParsedMapFieldValue> AsParsedMapField() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).AsParsedMapField();
  }
  optional_ref<const ParsedMapFieldValue> AsParsedMapField()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<ParsedMapFieldValue> AsParsedMapField() &&;
  absl::optional<ParsedMapFieldValue> AsParsedMapField() const&& {
    return common_internal::AsOptional(AsParsedMapField());
  }

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

  // Performs a checked cast from a value to a parsed repeated field value,
  // returning a non-empty optional with either a value or reference to the
  // parsed repeated field value. Otherwise an empty optional is returned.
  optional_ref<const ParsedRepeatedFieldValue> AsParsedRepeatedField() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).AsParsedRepeatedField();
  }
  optional_ref<const ParsedRepeatedFieldValue> AsParsedRepeatedField()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<ParsedRepeatedFieldValue> AsParsedRepeatedField() &&;
  absl::optional<ParsedRepeatedFieldValue> AsParsedRepeatedField() const&& {
    return common_internal::AsOptional(AsParsedRepeatedField());
  }

  // Performs a checked cast from a value to a parsed struct value,
  // returning a non-empty optional with either a value or reference to the
  // parsed struct value. Otherwise an empty optional is returned.
  optional_ref<const ParsedStructValue> AsParsedStruct() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).AsParsedStruct();
  }
  optional_ref<const ParsedStructValue> AsParsedStruct()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<ParsedStructValue> AsParsedStruct() &&;
  absl::optional<ParsedStructValue> AsParsedStruct() const&& {
    return common_internal::AsOptional(AsParsedStruct());
  }

  // Performs a checked cast from a value to a string value,
  // returning a non-empty optional with either a value or reference to the
  // string value. Otherwise an empty optional is returned.
  optional_ref<const StringValue> AsString() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).AsString();
  }
  optional_ref<const StringValue> AsString()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<StringValue> AsString() &&;
  absl::optional<StringValue> AsString() const&& {
    return common_internal::AsOptional(AsString());
  }

  // Performs a checked cast from a value to a struct value,
  // returning a non-empty optional with either a value or reference to the
  // struct value. Otherwise an empty optional is returned.
  absl::optional<StructValue> AsStruct() & {
    return std::as_const(*this).AsStruct();
  }
  absl::optional<StructValue> AsStruct() const&;
  absl::optional<StructValue> AsStruct() &&;
  absl::optional<StructValue> AsStruct() const&& {
    return common_internal::AsOptional(AsStruct());
  }

  // Performs a checked cast from a value to a timestamp value,
  // returning a non-empty optional with either a value or reference to the
  // timestamp value. Otherwise an empty optional is returned.
  absl::optional<TimestampValue> AsTimestamp() const;

  // Performs a checked cast from a value to a type value,
  // returning a non-empty optional with either a value or reference to the
  // type value. Otherwise an empty optional is returned.
  optional_ref<const TypeValue> AsType() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).AsType();
  }
  optional_ref<const TypeValue> AsType() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<TypeValue> AsType() &&;
  absl::optional<TypeValue> AsType() const&& {
    return common_internal::AsOptional(AsType());
  }

  // Performs a checked cast from a value to an uint value,
  // returning a non-empty optional with either a value or reference to the
  // uint value. Otherwise an empty optional is returned.
  absl::optional<UintValue> AsUint() const;

  // Performs a checked cast from a value to an unknown value,
  // returning a non-empty optional with either a value or reference to the
  // unknown value. Otherwise an empty optional is returned.
  optional_ref<const UnknownValue> AsUnknown() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).AsUnknown();
  }
  optional_ref<const UnknownValue> AsUnknown()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<UnknownValue> AsUnknown() &&;
  absl::optional<UnknownValue> AsUnknown() const&& {
    return common_internal::AsOptional(AsUnknown());
  }

  // Convenience method for use with template metaprogramming. See
  // `AsBool()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<BoolValue, T>, absl::optional<BoolValue>>
  As() & {
    return AsBool();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<BoolValue, T>, absl::optional<BoolValue>> As()
      const& {
    return AsBool();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<BoolValue, T>, absl::optional<BoolValue>>
  As() && {
    return AsBool();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<BoolValue, T>, absl::optional<BoolValue>> As()
      const&& {
    return AsBool();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsBytes()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<BytesValue, T>,
                       optional_ref<const BytesValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsBytes();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<BytesValue, T>,
                   optional_ref<const BytesValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsBytes();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<BytesValue, T>, absl::optional<BytesValue>>
  As() && {
    return std::move(*this).AsBytes();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<BytesValue, T>, absl::optional<BytesValue>>
  As() const&& {
    return std::move(*this).AsBytes();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsDouble()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleValue, T>, absl::optional<DoubleValue>>
  As() & {
    return AsDouble();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleValue, T>, absl::optional<DoubleValue>>
  As() const& {
    return AsDouble();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleValue, T>, absl::optional<DoubleValue>>
  As() && {
    return AsDouble();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleValue, T>, absl::optional<DoubleValue>>
  As() const&& {
    return AsDouble();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsDuration()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<DurationValue, T>,
                   absl::optional<DurationValue>>
  As() & {
    return AsDuration();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<DurationValue, T>,
                   absl::optional<DurationValue>>
  As() const& {
    return AsDuration();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<DurationValue, T>,
                   absl::optional<DurationValue>>
  As() && {
    return AsDuration();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<DurationValue, T>,
                   absl::optional<DurationValue>>
  As() const&& {
    return AsDuration();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsError()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ErrorValue, T>,
                       optional_ref<const ErrorValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsError();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ErrorValue, T>,
                   optional_ref<const ErrorValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsError();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ErrorValue, T>, absl::optional<ErrorValue>>
  As() && {
    return std::move(*this).AsError();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ErrorValue, T>, absl::optional<ErrorValue>>
  As() const&& {
    return std::move(*this).AsError();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsInt()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<IntValue, T>, absl::optional<IntValue>>
  As() & {
    return AsInt();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<IntValue, T>, absl::optional<IntValue>> As()
      const& {
    return AsInt();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<IntValue, T>, absl::optional<IntValue>>
  As() && {
    return AsInt();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<IntValue, T>, absl::optional<IntValue>> As()
      const&& {
    return AsInt();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsList()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<ListValue, T>, absl::optional<ListValue>>
  As() & {
    return AsList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ListValue, T>, absl::optional<ListValue>> As()
      const& {
    return AsList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ListValue, T>, absl::optional<ListValue>>
  As() && {
    return std::move(*this).AsList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ListValue, T>, absl::optional<ListValue>> As()
      const&& {
    return std::move(*this).AsList();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsMap()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<MapValue, T>, absl::optional<MapValue>>
  As() & {
    return AsMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<MapValue, T>, absl::optional<MapValue>> As()
      const& {
    return AsMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<MapValue, T>, absl::optional<MapValue>>
  As() && {
    return std::move(*this).AsMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<MapValue, T>, absl::optional<MapValue>> As()
      const&& {
    return std::move(*this).AsMap();
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
  // `AsNull()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<NullValue, T>, absl::optional<NullValue>>
  As() & {
    return AsNull();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<NullValue, T>, absl::optional<NullValue>> As()
      const& {
    return AsNull();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<NullValue, T>, absl::optional<NullValue>>
  As() && {
    return AsNull();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<NullValue, T>, absl::optional<NullValue>> As()
      const&& {
    return AsNull();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsOpaque()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<OpaqueValue, T>,
                       optional_ref<const OpaqueValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsOpaque();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<OpaqueValue, T>,
                   optional_ref<const OpaqueValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsOpaque();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<OpaqueValue, T>, absl::optional<OpaqueValue>>
  As() && {
    return std::move(*this).AsOpaque();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<OpaqueValue, T>, absl::optional<OpaqueValue>>
  As() const&& {
    return std::move(*this).AsOpaque();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsOptional()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<OptionalValue, T>,
                       optional_ref<const OptionalValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsOptional();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>,
                   optional_ref<const OptionalValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsOptional();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>,
                   absl::optional<OptionalValue>>
  As() && {
    return std::move(*this).AsOptional();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>,
                   absl::optional<OptionalValue>>
  As() const&& {
    return std::move(*this).AsOptional();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsParsedJsonList()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedJsonListValue, T>,
                       optional_ref<const ParsedJsonListValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsParsedJsonList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedJsonListValue, T>,
                   optional_ref<const ParsedJsonListValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsParsedJsonList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedJsonListValue, T>,
                   absl::optional<ParsedJsonListValue>>
  As() && {
    return std::move(*this).AsParsedJsonList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedJsonListValue, T>,
                   absl::optional<ParsedJsonListValue>>
  As() const&& {
    return std::move(*this).AsParsedJsonList();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsParsedJsonMap()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedJsonMapValue, T>,
                       optional_ref<const ParsedJsonMapValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsParsedJsonMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedJsonMapValue, T>,
                   optional_ref<const ParsedJsonMapValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsParsedJsonMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedJsonMapValue, T>,
                   absl::optional<ParsedJsonMapValue>>
  As() && {
    return std::move(*this).AsParsedJsonMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedJsonMapValue, T>,
                   absl::optional<ParsedJsonMapValue>>
  As() const&& {
    return std::move(*this).AsParsedJsonMap();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsParsedList()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedListValue, T>,
                       optional_ref<const ParsedListValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsParsedList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedListValue, T>,
                   optional_ref<const ParsedListValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsParsedList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedListValue, T>,
                   absl::optional<ParsedListValue>>
  As() && {
    return std::move(*this).AsParsedList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedListValue, T>,
                   absl::optional<ParsedListValue>>
  As() const&& {
    return std::move(*this).AsParsedList();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsParsedMap()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedMapValue, T>,
                       optional_ref<const ParsedMapValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsParsedMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMapValue, T>,
                   optional_ref<const ParsedMapValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsParsedMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMapValue, T>,
                   absl::optional<ParsedMapValue>>
  As() && {
    return std::move(*this).AsParsedMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMapValue, T>,
                   absl::optional<ParsedMapValue>>
  As() const&& {
    return std::move(*this).AsParsedMap();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsParsedMapField()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedMapFieldValue, T>,
                       optional_ref<const ParsedMapFieldValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsParsedMapField();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMapFieldValue, T>,
                   optional_ref<const ParsedMapFieldValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsParsedMapField();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMapFieldValue, T>,
                   absl::optional<ParsedMapFieldValue>>
  As() && {
    return std::move(*this).AsParsedMapField();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMapFieldValue, T>,
                   absl::optional<ParsedMapFieldValue>>
  As() const&& {
    return std::move(*this).AsParsedMapField();
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

  // Convenience method for use with template metaprogramming. See
  // `AsParsedRepeatedField()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedRepeatedFieldValue, T>,
                       optional_ref<const ParsedRepeatedFieldValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsParsedRepeatedField();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedRepeatedFieldValue, T>,
                   optional_ref<const ParsedRepeatedFieldValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsParsedRepeatedField();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedRepeatedFieldValue, T>,
                   absl::optional<ParsedRepeatedFieldValue>>
  As() && {
    return std::move(*this).AsParsedRepeatedField();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedRepeatedFieldValue, T>,
                   absl::optional<ParsedRepeatedFieldValue>>
  As() const&& {
    return std::move(*this).AsParsedRepeatedField();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsParsedStruct()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedStructValue, T>,
                       optional_ref<const ParsedStructValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsParsedStruct();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedStructValue, T>,
                   optional_ref<const ParsedStructValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsParsedStruct();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedStructValue, T>,
                   absl::optional<ParsedStructValue>>
  As() && {
    return std::move(*this).AsParsedStruct();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedStructValue, T>,
                   absl::optional<ParsedStructValue>>
  As() const&& {
    return std::move(*this).AsParsedStruct();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsString()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<StringValue, T>,
                       optional_ref<const StringValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsString();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<StringValue, T>,
                   optional_ref<const StringValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsString();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<StringValue, T>, absl::optional<StringValue>>
  As() && {
    return std::move(*this).AsString();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<StringValue, T>, absl::optional<StringValue>>
  As() const&& {
    return std::move(*this).AsString();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsStruct()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<StructValue, T>, absl::optional<StructValue>>
  As() & {
    return AsStruct();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<StructValue, T>, absl::optional<StructValue>>
  As() const& {
    return AsStruct();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<StructValue, T>, absl::optional<StructValue>>
  As() && {
    return std::move(*this).AsStruct();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<StructValue, T>, absl::optional<StructValue>>
  As() const&& {
    return std::move(*this).AsStruct();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsTimestamp()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<TimestampValue, T>,
                   absl::optional<TimestampValue>>
  As() & {
    return AsTimestamp();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<TimestampValue, T>,
                   absl::optional<TimestampValue>>
  As() const& {
    return AsTimestamp();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<TimestampValue, T>,
                   absl::optional<TimestampValue>>
  As() && {
    return AsTimestamp();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<TimestampValue, T>,
                   absl::optional<TimestampValue>>
  As() const&& {
    return AsTimestamp();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsType()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<TypeValue, T>,
                       optional_ref<const TypeValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsType();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<TypeValue, T>, optional_ref<const TypeValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsType();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<TypeValue, T>, absl::optional<TypeValue>>
  As() && {
    return std::move(*this).AsType();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<TypeValue, T>, absl::optional<TypeValue>> As()
      const&& {
    return std::move(*this).AsType();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsUint()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<UintValue, T>, absl::optional<UintValue>>
  As() & {
    return AsUint();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<UintValue, T>, absl::optional<UintValue>> As()
      const& {
    return AsUint();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<UintValue, T>, absl::optional<UintValue>>
  As() && {
    return AsUint();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<UintValue, T>, absl::optional<UintValue>> As()
      const&& {
    return AsUint();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsUnknown()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<UnknownValue, T>,
                       optional_ref<const UnknownValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsUnknown();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<UnknownValue, T>,
                   optional_ref<const UnknownValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsUnknown();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<UnknownValue, T>,
                   absl::optional<UnknownValue>>
  As() && {
    return std::move(*this).AsUnknown();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<UnknownValue, T>,
                   absl::optional<UnknownValue>>
  As() const&& {
    return std::move(*this).AsUnknown();
  }

  // Performs an unchecked cast from a value to a bool value. In
  // debug builds a best effort is made to crash. If `IsBool()` would return
  // false, calling this method is undefined behavior.
  BoolValue GetBool() const;

  // Performs an unchecked cast from a value to a bytes value. In
  // debug builds a best effort is made to crash. If `IsBytes()` would return
  // false, calling this method is undefined behavior.
  const BytesValue& GetBytes() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).GetBytes();
  }
  const BytesValue& GetBytes() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  BytesValue GetBytes() &&;
  BytesValue GetBytes() const&& { return GetBytes(); }

  // Performs an unchecked cast from a value to a double value. In
  // debug builds a best effort is made to crash. If `IsDouble()` would return
  // false, calling this method is undefined behavior.
  DoubleValue GetDouble() const;

  // Performs an unchecked cast from a value to a duration value. In
  // debug builds a best effort is made to crash. If `IsDuration()` would return
  // false, calling this method is undefined behavior.
  DurationValue GetDuration() const;

  // Performs an unchecked cast from a value to an error value. In
  // debug builds a best effort is made to crash. If `IsError()` would return
  // false, calling this method is undefined behavior.
  const ErrorValue& GetError() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).GetError();
  }
  const ErrorValue& GetError() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  ErrorValue GetError() &&;
  ErrorValue GetError() const&& { return GetError(); }

  // Performs an unchecked cast from a value to an int value. In
  // debug builds a best effort is made to crash. If `IsInt()` would return
  // false, calling this method is undefined behavior.
  IntValue GetInt() const;

  // Performs an unchecked cast from a value to a list value. In
  // debug builds a best effort is made to crash. If `IsList()` would return
  // false, calling this method is undefined behavior.
  ListValue GetList() & { return std::as_const(*this).GetList(); }
  ListValue GetList() const&;
  ListValue GetList() &&;
  ListValue GetList() const&& { return GetList(); }

  // Performs an unchecked cast from a value to a map value. In
  // debug builds a best effort is made to crash. If `IsMap()` would return
  // false, calling this method is undefined behavior.
  MapValue GetMap() & { return std::as_const(*this).GetMap(); }
  MapValue GetMap() const&;
  MapValue GetMap() &&;
  MapValue GetMap() const&& { return GetMap(); }

  // Performs an unchecked cast from a value to a message value. In
  // debug builds a best effort is made to crash. If `IsMessage()` would return
  // false, calling this method is undefined behavior.
  MessageValue GetMessage() & { return std::as_const(*this).GetMessage(); }
  MessageValue GetMessage() const&;
  MessageValue GetMessage() &&;
  MessageValue GetMessage() const&& { return GetMessage(); }

  // Performs an unchecked cast from a value to a null value. In
  // debug builds a best effort is made to crash. If `IsNull()` would return
  // false, calling this method is undefined behavior.
  NullValue GetNull() const;

  // Performs an unchecked cast from a value to an opaque value. In
  // debug builds a best effort is made to crash. If `IsOpaque()` would return
  // false, calling this method is undefined behavior.
  const OpaqueValue& GetOpaque() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).GetOpaque();
  }
  const OpaqueValue& GetOpaque() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  OpaqueValue GetOpaque() &&;
  OpaqueValue GetOpaque() const&& { return GetOpaque(); }

  // Performs an unchecked cast from a value to an optional value. In
  // debug builds a best effort is made to crash. If `IsOptional()` would return
  // false, calling this method is undefined behavior.
  const OptionalValue& GetOptional() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).GetOptional();
  }
  const OptionalValue& GetOptional() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  OptionalValue GetOptional() &&;
  OptionalValue GetOptional() const&& { return GetOptional(); }

  // Performs an unchecked cast from a value to a parsed message value. In
  // debug builds a best effort is made to crash. If `IsParsedJsonList()` would
  // return false, calling this method is undefined behavior.
  const ParsedJsonListValue& GetParsedJsonList() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).GetParsedJsonList();
  }
  const ParsedJsonListValue& GetParsedJsonList()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  ParsedJsonListValue GetParsedJsonList() &&;
  ParsedJsonListValue GetParsedJsonList() const&& {
    return GetParsedJsonList();
  }

  // Performs an unchecked cast from a value to a parsed message value. In
  // debug builds a best effort is made to crash. If `IsParsedJsonMap()` would
  // return false, calling this method is undefined behavior.
  const ParsedJsonMapValue& GetParsedJsonMap() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).GetParsedJsonMap();
  }
  const ParsedJsonMapValue& GetParsedJsonMap()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  ParsedJsonMapValue GetParsedJsonMap() &&;
  ParsedJsonMapValue GetParsedJsonMap() const&& { return GetParsedJsonMap(); }

  // Performs an unchecked cast from a value to a parsed list value. In
  // debug builds a best effort is made to crash. If `IsParsedList()` would
  // return false, calling this method is undefined behavior.
  const ParsedListValue& GetParsedList() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).GetParsedList();
  }
  const ParsedListValue& GetParsedList() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  ParsedListValue GetParsedList() &&;
  ParsedListValue GetParsedList() const&& { return GetParsedList(); }

  // Performs an unchecked cast from a value to a parsed map value. In
  // debug builds a best effort is made to crash. If `IsParsedMap()` would
  // return false, calling this method is undefined behavior.
  const ParsedMapValue& GetParsedMap() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).GetParsedMap();
  }
  const ParsedMapValue& GetParsedMap() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  ParsedMapValue GetParsedMap() &&;
  ParsedMapValue GetParsedMap() const&& { return GetParsedMap(); }

  // Performs an unchecked cast from a value to a parsed map field value. In
  // debug builds a best effort is made to crash. If `IsParsedMapField()` would
  // return false, calling this method is undefined behavior.
  const ParsedMapFieldValue& GetParsedMapField() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).GetParsedMapField();
  }
  const ParsedMapFieldValue& GetParsedMapField()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  ParsedMapFieldValue GetParsedMapField() &&;
  ParsedMapFieldValue GetParsedMapField() const&& {
    return GetParsedMapField();
  }

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

  // Performs an unchecked cast from a value to a parsed repeated field value.
  // In debug builds a best effort is made to crash. If
  // `IsParsedRepeatedField()` would return false, calling this method is
  // undefined behavior.
  const ParsedRepeatedFieldValue& GetParsedRepeatedField() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).GetParsedRepeatedField();
  }
  const ParsedRepeatedFieldValue& GetParsedRepeatedField()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  ParsedRepeatedFieldValue GetParsedRepeatedField() &&;
  ParsedRepeatedFieldValue GetParsedRepeatedField() const&& {
    return GetParsedRepeatedField();
  }

  // Performs an unchecked cast from a value to a parsed struct value. In
  // debug builds a best effort is made to crash. If `IsParsedStruct()` would
  // return false, calling this method is undefined behavior.
  const ParsedStructValue& GetParsedStruct() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).GetParsedStruct();
  }
  const ParsedStructValue& GetParsedStruct()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  ParsedStructValue GetParsedStruct() &&;
  ParsedStructValue GetParsedStruct() const&& { return GetParsedStruct(); }

  // Performs an unchecked cast from a value to a string value. In
  // debug builds a best effort is made to crash. If `IsString()` would return
  // false, calling this method is undefined behavior.
  const StringValue& GetString() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).GetString();
  }
  const StringValue& GetString() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  StringValue GetString() &&;
  StringValue GetString() const&& { return GetString(); }

  // Performs an unchecked cast from a value to a struct value. In
  // debug builds a best effort is made to crash. If `IsStruct()` would return
  // false, calling this method is undefined behavior.
  StructValue GetStruct() & { return std::as_const(*this).GetStruct(); }
  StructValue GetStruct() const&;
  StructValue GetStruct() &&;
  StructValue GetStruct() const&& { return GetStruct(); }

  // Performs an unchecked cast from a value to a timestamp value. In
  // debug builds a best effort is made to crash. If `IsTimestamp()` would
  // return false, calling this method is undefined behavior.
  TimestampValue GetTimestamp() const;

  // Performs an unchecked cast from a value to a type value. In
  // debug builds a best effort is made to crash. If `IsType()` would return
  // false, calling this method is undefined behavior.
  const TypeValue& GetType() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).GetType();
  }
  const TypeValue& GetType() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  TypeValue GetType() &&;
  TypeValue GetType() const&& { return GetType(); }

  // Performs an unchecked cast from a value to an uint value. In
  // debug builds a best effort is made to crash. If `IsUint()` would return
  // false, calling this method is undefined behavior.
  UintValue GetUint() const;

  // Performs an unchecked cast from a value to an unknown value. In
  // debug builds a best effort is made to crash. If `IsUnknown()` would return
  // false, calling this method is undefined behavior.
  const UnknownValue& GetUnknown() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).GetUnknown();
  }
  const UnknownValue& GetUnknown() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  UnknownValue GetUnknown() &&;
  UnknownValue GetUnknown() const&& { return GetUnknown(); }

  // Convenience method for use with template metaprogramming. See
  // `GetBool()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<BoolValue, T>, BoolValue> Get() & {
    return GetBool();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<BoolValue, T>, BoolValue> Get() const& {
    return GetBool();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<BoolValue, T>, BoolValue> Get() && {
    return GetBool();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<BoolValue, T>, BoolValue> Get() const&& {
    return GetBool();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetBytes()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<BytesValue, T>, const BytesValue&> Get() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetBytes();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<BytesValue, T>, const BytesValue&> Get()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetBytes();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<BytesValue, T>, BytesValue> Get() && {
    return std::move(*this).GetBytes();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<BytesValue, T>, BytesValue> Get() const&& {
    return std::move(*this).GetBytes();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetDouble()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleValue, T>, DoubleValue> Get() & {
    return GetDouble();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleValue, T>, DoubleValue> Get() const& {
    return GetDouble();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleValue, T>, DoubleValue> Get() && {
    return GetDouble();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleValue, T>, DoubleValue> Get() const&& {
    return GetDouble();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetDuration()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<DurationValue, T>, DurationValue> Get() & {
    return GetDuration();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<DurationValue, T>, DurationValue> Get()
      const& {
    return GetDuration();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<DurationValue, T>, DurationValue> Get() && {
    return GetDuration();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<DurationValue, T>, DurationValue> Get()
      const&& {
    return GetDuration();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetError()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ErrorValue, T>, const ErrorValue&> Get() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetError();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ErrorValue, T>, const ErrorValue&> Get()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetError();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ErrorValue, T>, ErrorValue> Get() && {
    return std::move(*this).GetError();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ErrorValue, T>, ErrorValue> Get() const&& {
    return std::move(*this).GetError();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetInt()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<IntValue, T>, IntValue> Get() & {
    return GetInt();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<IntValue, T>, IntValue> Get() const& {
    return GetInt();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<IntValue, T>, IntValue> Get() && {
    return GetInt();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<IntValue, T>, IntValue> Get() const&& {
    return GetInt();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetList()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<ListValue, T>, ListValue> Get() & {
    return GetList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ListValue, T>, ListValue> Get() const& {
    return GetList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ListValue, T>, ListValue> Get() && {
    return std::move(*this).GetList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ListValue, T>, ListValue> Get() const&& {
    return std::move(*this).GetList();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetMap()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<MapValue, T>, MapValue> Get() & {
    return GetMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<MapValue, T>, MapValue> Get() const& {
    return GetMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<MapValue, T>, MapValue> Get() && {
    return std::move(*this).GetMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<MapValue, T>, MapValue> Get() const&& {
    return std::move(*this).GetMap();
  }

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
  // `GetNull()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<NullValue, T>, NullValue> Get() & {
    return GetNull();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<NullValue, T>, NullValue> Get() const& {
    return GetNull();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<NullValue, T>, NullValue> Get() && {
    return GetNull();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<NullValue, T>, NullValue> Get() const&& {
    return GetNull();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetOpaque()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<OpaqueValue, T>, const OpaqueValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetOpaque();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<OpaqueValue, T>, const OpaqueValue&> Get()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetOpaque();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<OpaqueValue, T>, OpaqueValue> Get() && {
    return std::move(*this).GetOpaque();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<OpaqueValue, T>, OpaqueValue> Get() const&& {
    return std::move(*this).GetOpaque();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetOptional()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<OptionalValue, T>, const OptionalValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetOptional();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>, const OptionalValue&> Get()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetOptional();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>, OptionalValue> Get() && {
    return std::move(*this).GetOptional();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>, OptionalValue> Get()
      const&& {
    return std::move(*this).GetOptional();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetParsedJsonList()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedJsonListValue, T>,
                       const ParsedJsonListValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsedJsonList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedJsonListValue, T>,
                   const ParsedJsonListValue&>
  Get() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsedJsonList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedJsonListValue, T>, ParsedJsonListValue>
  Get() && {
    return std::move(*this).GetParsedJsonList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedJsonListValue, T>, ParsedJsonListValue>
  Get() const&& {
    return std::move(*this).GetParsedJsonList();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetParsedJsonMap()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedJsonMapValue, T>,
                       const ParsedJsonMapValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsedJsonMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedJsonMapValue, T>,
                   const ParsedJsonMapValue&>
  Get() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsedJsonMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedJsonMapValue, T>, ParsedJsonMapValue>
  Get() && {
    return std::move(*this).GetParsedJsonMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedJsonMapValue, T>, ParsedJsonMapValue>
  Get() const&& {
    return std::move(*this).GetParsedJsonMap();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetParsedList()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedListValue, T>,
                       const ParsedListValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsedList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedListValue, T>, const ParsedListValue&>
  Get() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsedList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedListValue, T>, ParsedListValue>
  Get() && {
    return std::move(*this).GetParsedList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedListValue, T>, ParsedListValue> Get()
      const&& {
    return std::move(*this).GetParsedList();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetParsedMap()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedMapValue, T>, const ParsedMapValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsedMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMapValue, T>, const ParsedMapValue&>
  Get() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsedMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMapValue, T>, ParsedMapValue> Get() && {
    return std::move(*this).GetParsedMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMapValue, T>, ParsedMapValue> Get()
      const&& {
    return std::move(*this).GetParsedMap();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetParsedMapField()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedMapFieldValue, T>,
                       const ParsedMapFieldValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsedMapField();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMapFieldValue, T>,
                   const ParsedMapFieldValue&>
  Get() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsedMapField();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMapFieldValue, T>, ParsedMapFieldValue>
  Get() && {
    return std::move(*this).GetParsedMapField();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMapFieldValue, T>, ParsedMapFieldValue>
  Get() const&& {
    return std::move(*this).GetParsedMapField();
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

  // Convenience method for use with template metaprogramming. See
  // `GetParsedRepeatedField()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedRepeatedFieldValue, T>,
                       const ParsedRepeatedFieldValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsedRepeatedField();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedRepeatedFieldValue, T>,
                   const ParsedRepeatedFieldValue&>
  Get() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsedRepeatedField();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedRepeatedFieldValue, T>,
                   ParsedRepeatedFieldValue>
  Get() && {
    return std::move(*this).GetParsedRepeatedField();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedRepeatedFieldValue, T>,
                   ParsedRepeatedFieldValue>
  Get() const&& {
    return std::move(*this).GetParsedRepeatedField();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetParsedStruct()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedStructValue, T>,
                       const ParsedStructValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsedStruct();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedStructValue, T>,
                   const ParsedStructValue&>
  Get() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsedStruct();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedStructValue, T>, ParsedStructValue>
  Get() && {
    return std::move(*this).GetParsedStruct();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedStructValue, T>, ParsedStructValue>
  Get() const&& {
    return std::move(*this).GetParsedStruct();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetString()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<StringValue, T>, const StringValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetString();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<StringValue, T>, const StringValue&> Get()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetString();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<StringValue, T>, StringValue> Get() && {
    return std::move(*this).GetString();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<StringValue, T>, StringValue> Get() const&& {
    return std::move(*this).GetString();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetStruct()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<StructValue, T>, StructValue> Get() & {
    return GetStruct();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<StructValue, T>, StructValue> Get() const& {
    return GetStruct();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<StructValue, T>, StructValue> Get() && {
    return std::move(*this).GetStruct();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<StructValue, T>, StructValue> Get() const&& {
    return std::move(*this).GetStruct();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetTimestamp()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<TimestampValue, T>, TimestampValue> Get() & {
    return GetTimestamp();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<TimestampValue, T>, TimestampValue> Get()
      const& {
    return GetTimestamp();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<TimestampValue, T>, TimestampValue> Get() && {
    return GetTimestamp();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<TimestampValue, T>, TimestampValue> Get()
      const&& {
    return GetTimestamp();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetType()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<TypeValue, T>, const TypeValue&> Get() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetType();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<TypeValue, T>, const TypeValue&> Get()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetType();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<TypeValue, T>, TypeValue> Get() && {
    return std::move(*this).GetType();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<TypeValue, T>, TypeValue> Get() const&& {
    return std::move(*this).GetType();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetUint()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<UintValue, T>, UintValue> Get() & {
    return GetUint();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<UintValue, T>, UintValue> Get() const& {
    return GetUint();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<UintValue, T>, UintValue> Get() && {
    return GetUint();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<UintValue, T>, UintValue> Get() const&& {
    return GetUint();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetUnknown()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<UnknownValue, T>, const UnknownValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetUnknown();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<UnknownValue, T>, const UnknownValue&> Get()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetUnknown();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<UnknownValue, T>, UnknownValue> Get() && {
    return std::move(*this).GetUnknown();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<UnknownValue, T>, UnknownValue> Get()
      const&& {
    return std::move(*this).GetUnknown();
  }

  // When `Value` is default constructed, it is in a valid but undefined state.
  // Any attempt to use it invokes undefined behavior. This mention can be used
  // to test whether this value is valid.
  explicit operator bool() const { return IsValid(); }

 private:
  friend struct NativeTypeTraits<Value>;
  friend bool common_internal::IsLegacyListValue(const Value& value);
  friend common_internal::LegacyListValue common_internal::GetLegacyListValue(
      const Value& value);
  friend bool common_internal::IsLegacyMapValue(const Value& value);
  friend common_internal::LegacyMapValue common_internal::GetLegacyMapValue(
      const Value& value);
  friend bool common_internal::IsLegacyStructValue(const Value& value);
  friend common_internal::LegacyStructValue
  common_internal::GetLegacyStructValue(const Value& value);

  constexpr bool IsValid() const {
    return !absl::holds_alternative<absl::monostate>(variant_);
  }

  void AssertIsValid() const {
    ABSL_DCHECK(IsValid()) << "use of invalid Value";
  }

  common_internal::ValueVariant variant_;
};

// Overloads for heterogeneous equality of numeric values.
bool operator==(IntValue lhs, UintValue rhs);
bool operator==(UintValue lhs, IntValue rhs);
bool operator==(IntValue lhs, DoubleValue rhs);
bool operator==(DoubleValue lhs, IntValue rhs);
bool operator==(UintValue lhs, DoubleValue rhs);
bool operator==(DoubleValue lhs, UintValue rhs);
inline bool operator!=(IntValue lhs, UintValue rhs) {
  return !operator==(lhs, rhs);
}
inline bool operator!=(UintValue lhs, IntValue rhs) {
  return !operator==(lhs, rhs);
}
inline bool operator!=(IntValue lhs, DoubleValue rhs) {
  return !operator==(lhs, rhs);
}
inline bool operator!=(DoubleValue lhs, IntValue rhs) {
  return !operator==(lhs, rhs);
}
inline bool operator!=(UintValue lhs, DoubleValue rhs) {
  return !operator==(lhs, rhs);
}
inline bool operator!=(DoubleValue lhs, UintValue rhs) {
  return !operator==(lhs, rhs);
}

template <>
struct NativeTypeTraits<Value> final {
  static NativeTypeId Id(const Value& value) {
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

  static bool SkipDestructor(const Value& value) {
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

// Statically assert some expectations.
static_assert(std::is_default_constructible_v<Value>);
static_assert(std::is_copy_constructible_v<Value>);
static_assert(std::is_copy_assignable_v<Value>);
static_assert(std::is_nothrow_move_constructible_v<Value>);
static_assert(std::is_nothrow_move_assignable_v<Value>);
static_assert(std::is_nothrow_swappable_v<Value>);

using ValueIteratorPtr = std::unique_ptr<ValueIterator>;

class ValueIterator {
 public:
  virtual ~ValueIterator() = default;

  virtual bool HasNext() = 0;

  // Returns a view of the next value. If the underlying implementation cannot
  // directly return a view of a value, the value will be stored in `scratch`,
  // and the returned view will be that of `scratch`.
  virtual absl::Status Next(ValueManager& value_manager, Value& result) = 0;

  absl::StatusOr<Value> Next(ValueManager& value_manager) {
    Value result;
    CEL_RETURN_IF_ERROR(Next(value_manager, result));
    return result;
  }
};

absl::Nonnull<std::unique_ptr<ValueIterator>> NewEmptyValueIterator();

class ValueBuilder {
 public:
  virtual ~ValueBuilder() = default;

  virtual absl::Status SetFieldByName(absl::string_view name, Value value) = 0;

  virtual absl::Status SetFieldByNumber(int64_t number, Value value) = 0;

  virtual Value Build() && = 0;
};

using ValueBuilderPtr = std::unique_ptr<ValueBuilder>;

using ListValueBuilderInterface = ListValueBuilder;
using MapValueBuilderInterface = MapValueBuilder;
using StructValueBuilderInterface = StructValueBuilder;

// Now that Value is complete, we can define various parts of list, map, opaque,
// and struct which depend on Value.

inline absl::Status ParsedListValue::Get(ValueManager& value_manager,
                                         size_t index, Value& result) const {
  return interface_->Get(value_manager, index, result);
}

inline absl::Status ParsedListValue::ForEach(ValueManager& value_manager,
                                             ForEachCallback callback) const {
  return interface_->ForEach(value_manager, callback);
}

inline absl::Status ParsedListValue::ForEach(
    ValueManager& value_manager, ForEachWithIndexCallback callback) const {
  return interface_->ForEach(value_manager, callback);
}

inline absl::StatusOr<absl::Nonnull<ValueIteratorPtr>>
ParsedListValue::NewIterator(ValueManager& value_manager) const {
  return interface_->NewIterator(value_manager);
}

inline absl::Status ParsedListValue::Equal(ValueManager& value_manager,
                                           const Value& other,
                                           Value& result) const {
  return interface_->Equal(value_manager, other, result);
}

inline absl::Status ParsedListValue::Contains(ValueManager& value_manager,
                                              const Value& other,
                                              Value& result) const {
  return interface_->Contains(value_manager, other, result);
}

inline absl::Status OpaqueValue::Equal(ValueManager& value_manager,
                                       const Value& other,
                                       Value& result) const {
  return interface_->Equal(value_manager, other, result);
}

inline cel::Value OptionalValueInterface::Value() const {
  cel::Value result;
  Value(result);
  return result;
}

inline void OptionalValue::Value(cel::Value& result) const {
  (*this)->Value(result);
}

inline cel::Value OptionalValue::Value() const { return (*this)->Value(); }

inline absl::Status ParsedMapValue::Get(ValueManager& value_manager,
                                        const Value& key, Value& result) const {
  return interface_->Get(value_manager, key, result);
}

inline absl::StatusOr<bool> ParsedMapValue::Find(ValueManager& value_manager,
                                                 const Value& key,
                                                 Value& result) const {
  return interface_->Find(value_manager, key, result);
}

inline absl::Status ParsedMapValue::Has(ValueManager& value_manager,
                                        const Value& key, Value& result) const {
  return interface_->Has(value_manager, key, result);
}

inline absl::Status ParsedMapValue::ListKeys(ValueManager& value_manager,
                                             ListValue& result) const {
  return interface_->ListKeys(value_manager, result);
}

inline absl::Status ParsedMapValue::ForEach(ValueManager& value_manager,
                                            ForEachCallback callback) const {
  return interface_->ForEach(value_manager, callback);
}

inline absl::StatusOr<absl::Nonnull<ValueIteratorPtr>>
ParsedMapValue::NewIterator(ValueManager& value_manager) const {
  return interface_->NewIterator(value_manager);
}

inline absl::Status ParsedMapValue::Equal(ValueManager& value_manager,
                                          const Value& other,
                                          Value& result) const {
  return interface_->Equal(value_manager, other, result);
}

inline absl::Status ParsedStructValue::GetFieldByName(
    ValueManager& value_manager, absl::string_view name, Value& result,
    ProtoWrapperTypeOptions unboxing_options) const {
  return interface_->GetFieldByName(value_manager, name, result,
                                    unboxing_options);
}

inline absl::Status ParsedStructValue::GetFieldByNumber(
    ValueManager& value_manager, int64_t number, Value& result,
    ProtoWrapperTypeOptions unboxing_options) const {
  return interface_->GetFieldByNumber(value_manager, number, result,
                                      unboxing_options);
}

inline absl::Status ParsedStructValue::Equal(ValueManager& value_manager,
                                             const Value& other,
                                             Value& result) const {
  return interface_->Equal(value_manager, other, result);
}

inline absl::Status ParsedStructValue::ForEachField(
    ValueManager& value_manager, ForEachFieldCallback callback) const {
  return interface_->ForEachField(value_manager, callback);
}

inline absl::StatusOr<int> ParsedStructValue::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test, Value& result) const {
  return interface_->Qualify(value_manager, qualifiers, presence_test, result);
}

namespace common_internal {

using MapFieldKeyAccessor = void (*)(Allocator<>, Borrower,
                                     const google::protobuf::MapKey&, Value&);

absl::StatusOr<MapFieldKeyAccessor> MapFieldKeyAccessorFor(
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field);

using MapFieldValueAccessor =
    void (*)(Borrower, const google::protobuf::MapValueConstRef&,
             absl::Nonnull<const google::protobuf::FieldDescriptor*>,
             absl::Nonnull<const google::protobuf::DescriptorPool*>,
             absl::Nonnull<google::protobuf::MessageFactory*>, Value&);

absl::StatusOr<MapFieldValueAccessor> MapFieldValueAccessorFor(
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field);

using RepeatedFieldAccessor =
    void (*)(Allocator<>, Borrowed<const google::protobuf::Message>,
             absl::Nonnull<const google::protobuf::FieldDescriptor*>,
             absl::Nonnull<const google::protobuf::Reflection*>, int,
             absl::Nonnull<const google::protobuf::DescriptorPool*>,
             absl::Nonnull<google::protobuf::MessageFactory*>, Value&);

absl::StatusOr<RepeatedFieldAccessor> RepeatedFieldAccessorFor(
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field);

// Wrapper around `Value`, providing the same API as `TrivialValue`.
class NonTrivialValue final {
 public:
  NonTrivialValue() = default;
  NonTrivialValue(const NonTrivialValue&) = default;
  NonTrivialValue(NonTrivialValue&&) = default;
  NonTrivialValue& operator=(const NonTrivialValue&) = default;
  NonTrivialValue& operator=(NonTrivialValue&&) = default;

  explicit NonTrivialValue(const Value& other) : value_(other) {}

  explicit NonTrivialValue(Value&& other) : value_(std::move(other)) {}

  absl::Nonnull<Value*> get() { return std::addressof(value_); }

  absl::Nonnull<const Value*> get() const { return std::addressof(value_); }

  Value& operator*() ABSL_ATTRIBUTE_LIFETIME_BOUND { return *get(); }

  const Value& operator*() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return *get();
  }

  absl::Nonnull<Value*> operator->() { return get(); }

  absl::Nonnull<const Value*> operator->() const { return get(); }

  friend void swap(NonTrivialValue& lhs, NonTrivialValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
  }

 private:
  Value value_;
};

class TrivialValue;

TrivialValue MakeTrivialValue(const Value& value,
                              absl::Nonnull<google::protobuf::Arena*> arena);

// Wrapper around `Value` which makes it trivial, providing the same API as
// `NonTrivialValue`.
class TrivialValue final {
 public:
  TrivialValue() : TrivialValue(Value()) {}
  TrivialValue(const TrivialValue&) = default;
  TrivialValue(TrivialValue&&) = default;
  TrivialValue& operator=(const TrivialValue&) = default;
  TrivialValue& operator=(TrivialValue&&) = default;

  absl::Nonnull<Value*> get() {
    return std::launder(reinterpret_cast<Value*>(&value_[0]));
  }

  absl::Nonnull<const Value*> get() const {
    return std::launder(reinterpret_cast<const Value*>(&value_[0]));
  }

  Value& operator*() ABSL_ATTRIBUTE_LIFETIME_BOUND { return *get(); }

  const Value& operator*() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return *get();
  }

  absl::Nonnull<Value*> operator->() { return get(); }

  absl::Nonnull<const Value*> operator->() const { return get(); }

  absl::string_view ToString() const;

  absl::string_view ToBytes() const;

 private:
  friend TrivialValue MakeTrivialValue(const Value& value,
                                       absl::Nonnull<google::protobuf::Arena*> arena);

  explicit TrivialValue(const Value& other) {
    std::memcpy(&value_[0], static_cast<const void*>(std::addressof(other)),
                sizeof(Value));
  }

  alignas(Value) char value_[sizeof(Value)];
};

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUE_H_
