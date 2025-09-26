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

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
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
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/utility/utility.h"
#include "base/attribute.h"
#include "common/arena.h"
#include "common/native_type.h"
#include "common/optional_ref.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/bool_value.h"  // IWYU pragma: export
#include "common/values/bytes_value.h"  // IWYU pragma: export
#include "common/values/bytes_value_input_stream.h"  // IWYU pragma: export
#include "common/values/bytes_value_output_stream.h"  // IWYU pragma: export
#include "common/values/custom_list_value.h"  // IWYU pragma: export
#include "common/values/custom_map_value.h"  // IWYU pragma: export
#include "common/values/custom_struct_value.h"  // IWYU pragma: export
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
#include "common/values/value_variant.h"
#include "common/values/values.h"
#include "internal/status_macros.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/generated_enum_reflection.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/message.h"

namespace cel {

// `Value` is a composition type which encompasses all values supported by the
// Common Expression Language. When default constructed or moved, `Value` is in
// a known but invalid state. Any attempt to use it from then on, without
// assigning another type, is undefined behavior. In debug builds, we do our
// best to fail.
class Value final : private common_internal::ValueMixin<Value> {
 public:
  // Returns an appropriate `Value` for the dynamic protobuf enum. For open
  // enums, returns `cel::IntValue`. For closed enums, returns `cel::ErrorValue`
  // if the value is not present in the enum otherwise returns `cel::IntValue`.
  static Value Enum(const google::protobuf::EnumValueDescriptor* absl_nonnull value);
  static Value Enum(const google::protobuf::EnumDescriptor* absl_nonnull type,
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
  // the resulting value and any of its shallow copies. Otherwise the message is
  // copied using `arena`.
  static Value FromMessage(
      const google::protobuf::Message& message,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      google::protobuf::MessageFactory* absl_nonnull message_factory
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND);
  static Value FromMessage(
      google::protobuf::Message&& message,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      google::protobuf::MessageFactory* absl_nonnull message_factory
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND);

  // Returns an appropriate `Value` for the dynamic protobuf message. If
  // `message` is the well known type `google.protobuf.Any`, `descriptor_pool`
  // and `message_factory` will be used to unpack the value. Both must outlive
  // the resulting value and any of its shallow copies. Otherwise the message is
  // borrowed (no copying). If the message is on an arena, that arena will be
  // attributed as the owner. Otherwise `arena` is used.
  static Value WrapMessage(
      const google::protobuf::Message* absl_nonnull message ABSL_ATTRIBUTE_LIFETIME_BOUND,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      google::protobuf::MessageFactory* absl_nonnull message_factory
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND);

  // Returns an appropriate `Value` for the dynamic protobuf message field. If
  // `field` in `message` is the well known type `google.protobuf.Any`,
  // `descriptor_pool` and `message_factory` will be used to unpack the value.
  // Both must outlive the resulting value and any of its shallow copies.
  // Otherwise the field is borrowed (no copying). If the message is on an
  // arena, that arena will be attributed as the owner. Otherwise `arena` is
  // used.
  static Value WrapField(
      ProtoWrapperTypeOptions wrapper_type_options,
      const google::protobuf::Message* absl_nonnull message ABSL_ATTRIBUTE_LIFETIME_BOUND,
      const google::protobuf::FieldDescriptor* absl_nonnull field
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      google::protobuf::MessageFactory* absl_nonnull message_factory
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND);
  static Value WrapField(
      const google::protobuf::Message* absl_nonnull message ABSL_ATTRIBUTE_LIFETIME_BOUND,
      const google::protobuf::FieldDescriptor* absl_nonnull field
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      google::protobuf::MessageFactory* absl_nonnull message_factory
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    return WrapField(ProtoWrapperTypeOptions::kUnsetNull, message, field,
                     descriptor_pool, message_factory, arena);
  }

  // Returns an appropriate `Value` for the dynamic protobuf message repeated
  // field. If `field` in `message` is the well known type
  // `google.protobuf.Any`, `descriptor_pool` and `message_factory` will be used
  // to unpack the value. Both must outlive the resulting value and any of its
  // shallow copies.
  static Value WrapRepeatedField(
      int index,
      const google::protobuf::Message* absl_nonnull message ABSL_ATTRIBUTE_LIFETIME_BOUND,
      const google::protobuf::FieldDescriptor* absl_nonnull field
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      google::protobuf::MessageFactory* absl_nonnull message_factory
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND);

  // Returns an appropriate `StringValue` for the dynamic protobuf message map
  // field key. The map field key must be a string or the behavior is undefined.
  static StringValue WrapMapFieldKeyString(
      const google::protobuf::MapKey& key,
      const google::protobuf::Message* absl_nonnull message ABSL_ATTRIBUTE_LIFETIME_BOUND,
      google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND);

  // Returns an appropriate `Value` for the dynamic protobuf message map
  // field value. If `field` in `message`, which is `value`, is the well known
  // type `google.protobuf.Any`, `descriptor_pool` and `message_factory` will be
  // used to unpack the value. Both must outlive the resulting value and any of
  // its shallow copies.
  static Value WrapMapFieldValue(
      const google::protobuf::MapValueConstRef& value,
      const google::protobuf::Message* absl_nonnull message ABSL_ATTRIBUTE_LIFETIME_BOUND,
      const google::protobuf::FieldDescriptor* absl_nonnull field
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      google::protobuf::MessageFactory* absl_nonnull message_factory
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND);

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
  Value(const OptionalValue& value)
      : variant_(absl::in_place_type<OpaqueValue>,
                 static_cast<const OpaqueValue&>(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(OptionalValue&& value)
      : variant_(absl::in_place_type<OpaqueValue>,
                 static_cast<OpaqueValue&&>(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(const OptionalValue& value) {
    variant_.Assign(static_cast<const OpaqueValue&>(value));
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(OptionalValue&& value) {
    variant_.Assign(static_cast<OpaqueValue&&>(value));
    return *this;
  }

  template <typename T,
            typename = std::enable_if_t<
                common_internal::IsValueAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(T&& alternative) noexcept
      : variant_(absl::in_place_type<absl::remove_cvref_t<T>>,
                 std::forward<T>(alternative)) {}

  template <typename T,
            typename = std::enable_if_t<
                common_internal::IsValueAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(T&& alternative) noexcept {
    variant_.Assign(std::forward<T>(alternative));
    return *this;
  }

  ValueKind kind() const { return variant_.kind(); }

  Type GetRuntimeType() const;

  absl::string_view GetTypeName() const;

  std::string DebugString() const;

  // `SerializeTo` serializes this value to `output`. If an error is returned,
  // `output` is in a valid but unspecified state. If this value does not
  // support serialization, `FAILED_PRECONDITION` is returned.
  absl::Status SerializeTo(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const;

  // `ConvertToJson` converts this value to its JSON representation. The
  // argument `json` **MUST** be an instance of `google.protobuf.Value` which is
  // can either be the generated message or a dynamic message. The descriptor
  // pool `descriptor_pool` and message factory `message_factory` are used to
  // deal with serialized messages and a few corners cases.
  absl::Status ConvertToJson(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const;

  // `ConvertToJsonArray` converts this value to its JSON representation if and
  // only if it can be represented as an array. The argument `json` **MUST** be
  // an instance of `google.protobuf.ListValue` which is can either be the
  // generated message or a dynamic message. The descriptor pool
  // `descriptor_pool` and message factory `message_factory` are used to deal
  // with serialized messages and a few corners cases.
  absl::Status ConvertToJsonArray(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const;

  // `ConvertToJsonArray` converts this value to its JSON representation if and
  // only if it can be represented as an object. The argument `json` **MUST** be
  // an instance of `google.protobuf.Struct` which is can either be the
  // generated message or a dynamic message. The descriptor pool
  // `descriptor_pool` and message factory `message_factory` are used to deal
  // with serialized messages and a few corners cases.
  absl::Status ConvertToJsonObject(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const;

  absl::Status Equal(const Value& other,
                     const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                     google::protobuf::MessageFactory* absl_nonnull message_factory,
                     google::protobuf::Arena* absl_nonnull arena,
                     Value* absl_nonnull result) const;
  using ValueMixin::Equal;

  bool IsZeroValue() const;

  // Clones the value to another arena, if necessary, such that the lifetime of
  // the value is tied to the arena.
  Value Clone(google::protobuf::Arena* absl_nonnull arena) const;

  friend void swap(Value& lhs, Value& rhs) noexcept {
    using std::swap;
    swap(lhs.variant_, rhs.variant_);
  }

  friend std::ostream& operator<<(std::ostream& out, const Value& value);

  ABSL_DEPRECATED("Just use operator.()")
  Value* operator->() { return this; }

  ABSL_DEPRECATED("Just use operator.()")
  const Value* operator->() const { return this; }

  // Returns `true` if this value is an instance of a bool value.
  bool IsBool() const { return variant_.Is<BoolValue>(); }

  // Returns `true` if this value is an instance of a bool value and true.
  bool IsTrue() const { return IsBool() && GetBool().NativeValue(); }

  // Returns `true` if this value is an instance of a bool value and false.
  bool IsFalse() const { return IsBool() && !GetBool().NativeValue(); }

  // Returns `true` if this value is an instance of a bytes value.
  bool IsBytes() const { return variant_.Is<BytesValue>(); }

  // Returns `true` if this value is an instance of a double value.
  bool IsDouble() const { return variant_.Is<DoubleValue>(); }

  // Returns `true` if this value is an instance of a duration value.
  bool IsDuration() const { return variant_.Is<DurationValue>(); }

  // Returns `true` if this value is an instance of an error value.
  bool IsError() const { return variant_.Is<ErrorValue>(); }

  // Returns `true` if this value is an instance of an int value.
  bool IsInt() const { return variant_.Is<IntValue>(); }

  // Returns `true` if this value is an instance of a list value.
  bool IsList() const {
    return variant_.Is<common_internal::LegacyListValue>() ||
           variant_.Is<CustomListValue>() ||
           variant_.Is<ParsedRepeatedFieldValue>() ||
           variant_.Is<ParsedJsonListValue>();
  }

  // Returns `true` if this value is an instance of a map value.
  bool IsMap() const {
    return variant_.Is<common_internal::LegacyMapValue>() ||
           variant_.Is<CustomMapValue>() ||
           variant_.Is<ParsedMapFieldValue>() ||
           variant_.Is<ParsedJsonMapValue>();
  }

  // Returns `true` if this value is an instance of a message value. If `true`
  // is returned, it is implied that `IsStruct()` would also return true.
  bool IsMessage() const { return variant_.Is<ParsedMessageValue>(); }

  // Returns `true` if this value is an instance of a null value.
  bool IsNull() const { return variant_.Is<NullValue>(); }

  // Returns `true` if this value is an instance of an opaque value.
  bool IsOpaque() const { return variant_.Is<OpaqueValue>(); }

  // Returns `true` if this value is an instance of an optional value. If `true`
  // is returned, it is implied that `IsOpaque()` would also return true.
  bool IsOptional() const {
    if (const auto* alternative = variant_.As<OpaqueValue>();
        alternative != nullptr) {
      return alternative->IsOptional();
    }
    return false;
  }

  // Returns `true` if this value is an instance of a parsed JSON list value. If
  // `true` is returned, it is implied that `IsList()` would also return
  // true.
  bool IsParsedJsonList() const { return variant_.Is<ParsedJsonListValue>(); }

  // Returns `true` if this value is an instance of a parsed JSON map value. If
  // `true` is returned, it is implied that `IsMap()` would also return
  // true.
  bool IsParsedJsonMap() const { return variant_.Is<ParsedJsonMapValue>(); }

  // Returns `true` if this value is an instance of a custom list value. If
  // `true` is returned, it is implied that `IsList()` would also return
  // true.
  bool IsCustomList() const { return variant_.Is<CustomListValue>(); }

  // Returns `true` if this value is an instance of a custom map value. If
  // `true` is returned, it is implied that `IsMap()` would also return
  // true.
  bool IsCustomMap() const { return variant_.Is<CustomMapValue>(); }

  // Returns `true` if this value is an instance of a parsed map field value. If
  // `true` is returned, it is implied that `IsMap()` would also return
  // true.
  bool IsParsedMapField() const { return variant_.Is<ParsedMapFieldValue>(); }

  // Returns `true` if this value is an instance of a parsed message value. If
  // `true` is returned, it is implied that `IsMessage()` would also return
  // true.
  bool IsParsedMessage() const { return variant_.Is<ParsedMessageValue>(); }

  // Returns `true` if this value is an instance of a parsed repeated field
  // value. If `true` is returned, it is implied that `IsList()` would also
  // return true.
  bool IsParsedRepeatedField() const {
    return variant_.Is<ParsedRepeatedFieldValue>();
  }

  // Returns `true` if this value is an instance of a custom struct value. If
  // `true` is returned, it is implied that `IsStruct()` would also return
  // true.
  bool IsCustomStruct() const { return variant_.Is<CustomStructValue>(); }

  // Returns `true` if this value is an instance of a string value.
  bool IsString() const { return variant_.Is<StringValue>(); }

  // Returns `true` if this value is an instance of a struct value.
  bool IsStruct() const {
    return variant_.Is<common_internal::LegacyStructValue>() ||
           variant_.Is<CustomStructValue>() ||
           variant_.Is<ParsedMessageValue>();
  }

  // Returns `true` if this value is an instance of a timestamp value.
  bool IsTimestamp() const { return variant_.Is<TimestampValue>(); }

  // Returns `true` if this value is an instance of a type value.
  bool IsType() const { return variant_.Is<TypeValue>(); }

  // Returns `true` if this value is an instance of a uint value.
  bool IsUint() const { return variant_.Is<UintValue>(); }

  // Returns `true` if this value is an instance of an unknown value.
  bool IsUnknown() const { return variant_.Is<UnknownValue>(); }

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
  // `IsCustomList()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomListValue, T>, bool> Is() const {
    return IsCustomList();
  }

  // Convenience method for use with template metaprogramming. See
  // `IsCustomMap()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomMapValue, T>, bool> Is() const {
    return IsCustomMap();
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
  std::enable_if_t<std::is_same_v<CustomStructValue, T>, bool> Is() const {
    return IsCustomStruct();
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
  absl::optional<BoolValue> AsBool() const {
    if (const auto* alternative = variant_.As<BoolValue>();
        alternative != nullptr) {
      return *alternative;
    }
    return absl::nullopt;
  }

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

  // Performs a checked cast from a value to a custom list value,
  // returning a non-empty optional with either a value or reference to the
  // custom list value. Otherwise an empty optional is returned.
  optional_ref<const CustomListValue> AsCustomList() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).AsCustomList();
  }
  optional_ref<const CustomListValue> AsCustomList()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<CustomListValue> AsCustomList() &&;
  absl::optional<CustomListValue> AsCustomList() const&& {
    return common_internal::AsOptional(AsCustomList());
  }

  // Performs a checked cast from a value to a custom map value,
  // returning a non-empty optional with either a value or reference to the
  // custom map value. Otherwise an empty optional is returned.
  optional_ref<const CustomMapValue> AsCustomMap() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).AsCustomMap();
  }
  optional_ref<const CustomMapValue> AsCustomMap()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<CustomMapValue> AsCustomMap() &&;
  absl::optional<CustomMapValue> AsCustomMap() const&& {
    return common_internal::AsOptional(AsCustomMap());
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

  // Performs a checked cast from a value to a custom struct value,
  // returning a non-empty optional with either a value or reference to the
  // custom struct value. Otherwise an empty optional is returned.
  optional_ref<const CustomStructValue> AsCustomStruct() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).AsCustomStruct();
  }
  optional_ref<const CustomStructValue> AsCustomStruct()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<CustomStructValue> AsCustomStruct() &&;
  absl::optional<CustomStructValue> AsCustomStruct() const&& {
    return common_internal::AsOptional(AsCustomStruct());
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
  // `AsCustomList()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<CustomListValue, T>,
                       optional_ref<const CustomListValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsCustomList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomListValue, T>,
                   optional_ref<const CustomListValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsCustomList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomListValue, T>,
                   absl::optional<CustomListValue>>
  As() && {
    return std::move(*this).AsCustomList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomListValue, T>,
                   absl::optional<CustomListValue>>
  As() const&& {
    return std::move(*this).AsCustomList();
  }

  // Convenience method for use with template metaprogramming. See
  // `AsCustomMap()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<CustomMapValue, T>,
                       optional_ref<const CustomMapValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsCustomMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomMapValue, T>,
                   optional_ref<const CustomMapValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsCustomMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomMapValue, T>,
                   absl::optional<CustomMapValue>>
  As() && {
    return std::move(*this).AsCustomMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomMapValue, T>,
                   absl::optional<CustomMapValue>>
  As() const&& {
    return std::move(*this).AsCustomMap();
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
  // `AsCustomStruct()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<CustomStructValue, T>,
                       optional_ref<const CustomStructValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsCustomStruct();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomStructValue, T>,
                   optional_ref<const CustomStructValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsCustomStruct();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomStructValue, T>,
                   absl::optional<CustomStructValue>>
  As() && {
    return std::move(*this).AsCustomStruct();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomStructValue, T>,
                   absl::optional<CustomStructValue>>
  As() const&& {
    return std::move(*this).AsCustomStruct();
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
  BoolValue GetBool() const {
    ABSL_DCHECK(IsBool()) << *this;
    return variant_.Get<BoolValue>();
  }

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

  // Performs an unchecked cast from a value to a custom list value. In
  // debug builds a best effort is made to crash. If `IsCustomList()` would
  // return false, calling this method is undefined behavior.
  const CustomListValue& GetCustomList() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).GetCustomList();
  }
  const CustomListValue& GetCustomList() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  CustomListValue GetCustomList() &&;
  CustomListValue GetCustomList() const&& { return GetCustomList(); }

  // Performs an unchecked cast from a value to a custom map value. In
  // debug builds a best effort is made to crash. If `IsCustomMap()` would
  // return false, calling this method is undefined behavior.
  const CustomMapValue& GetCustomMap() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).GetCustomMap();
  }
  const CustomMapValue& GetCustomMap() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  CustomMapValue GetCustomMap() &&;
  CustomMapValue GetCustomMap() const&& { return GetCustomMap(); }

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

  // Performs an unchecked cast from a value to a custom struct value. In
  // debug builds a best effort is made to crash. If `IsCustomStruct()` would
  // return false, calling this method is undefined behavior.
  const CustomStructValue& GetCustomStruct() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).GetCustomStruct();
  }
  const CustomStructValue& GetCustomStruct()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  CustomStructValue GetCustomStruct() &&;
  CustomStructValue GetCustomStruct() const&& { return GetCustomStruct(); }

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
  // `GetCustomList()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<CustomListValue, T>,
                       const CustomListValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetCustomList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomListValue, T>, const CustomListValue&>
  Get() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetCustomList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomListValue, T>, CustomListValue>
  Get() && {
    return std::move(*this).GetCustomList();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomListValue, T>, CustomListValue> Get()
      const&& {
    return std::move(*this).GetCustomList();
  }

  // Convenience method for use with template metaprogramming. See
  // `GetCustomMap()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<CustomMapValue, T>, const CustomMapValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetCustomMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomMapValue, T>, const CustomMapValue&>
  Get() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetCustomMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomMapValue, T>, CustomMapValue> Get() && {
    return std::move(*this).GetCustomMap();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomMapValue, T>, CustomMapValue> Get()
      const&& {
    return std::move(*this).GetCustomMap();
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
  // `GetCustomStruct()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<CustomStructValue, T>,
                       const CustomStructValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetCustomStruct();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomStructValue, T>,
                   const CustomStructValue&>
  Get() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetCustomStruct();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomStructValue, T>, CustomStructValue>
  Get() && {
    return std::move(*this).GetCustomStruct();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomStructValue, T>, CustomStructValue>
  Get() const&& {
    return std::move(*this).GetCustomStruct();
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
  explicit operator bool() const { return true; }

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
  friend class common_internal::ValueMixin<Value>;
  friend struct ArenaTraits<Value>;

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
    return value.variant_.Visit([](const auto& alternative) -> NativeTypeId {
      return NativeTypeId::Of(alternative);
    });
  }
};

template <>
struct ArenaTraits<Value> {
  static bool trivially_destructible(const Value& value) {
    return value.variant_.Visit([](const auto& alternative) -> bool {
      return ArenaTraits<>::trivially_destructible(alternative);
    });
  }
};

// Statically assert some expectations.
static_assert(sizeof(Value) <= 32);
static_assert(alignof(Value) <= alignof(std::max_align_t));
static_assert(std::is_default_constructible_v<Value>);
static_assert(std::is_copy_constructible_v<Value>);
static_assert(std::is_copy_assignable_v<Value>);
static_assert(std::is_nothrow_move_constructible_v<Value>);
static_assert(std::is_nothrow_move_assignable_v<Value>);
static_assert(std::is_nothrow_swappable_v<Value>);

inline common_internal::ImplicitlyConvertibleStatus
ErrorValueAssign::operator()(absl::Status status) const {
  *value_ = ErrorValue(std::move(status));
  return common_internal::ImplicitlyConvertibleStatus();
}

namespace common_internal {

template <typename Base>
absl::StatusOr<Value> ValueMixin<Base>::Equal(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

  Value result;
  CEL_RETURN_IF_ERROR(static_cast<const Base*>(this)->Equal(
      other, descriptor_pool, message_factory, arena, &result));
  return result;
}

template <typename Base>
absl::StatusOr<Value> ListValueMixin<Base>::Get(
    size_t index, const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

  Value result;
  CEL_RETURN_IF_ERROR(static_cast<const Base*>(this)->Get(
      index, descriptor_pool, message_factory, arena, &result));
  return result;
}

template <typename Base>
absl::StatusOr<Value> ListValueMixin<Base>::Contains(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

  Value result;
  CEL_RETURN_IF_ERROR(static_cast<const Base*>(this)->Contains(
      other, descriptor_pool, message_factory, arena, &result));
  return result;
}

template <typename Base>
absl::StatusOr<Value> MapValueMixin<Base>::Get(
    const Value& key,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

  Value result;
  CEL_RETURN_IF_ERROR(static_cast<const Base*>(this)->Get(
      key, descriptor_pool, message_factory, arena, &result));
  return result;
}

template <typename Base>
absl::StatusOr<absl::optional<Value>> MapValueMixin<Base>::Find(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

  Value result;
  CEL_ASSIGN_OR_RETURN(
      bool found, static_cast<const Base*>(this)->Find(
                      other, descriptor_pool, message_factory, arena, &result));
  if (found) {
    return result;
  }
  return absl::nullopt;
}

template <typename Base>
absl::StatusOr<Value> MapValueMixin<Base>::Has(
    const Value& key,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

  Value result;
  CEL_RETURN_IF_ERROR(static_cast<const Base*>(this)->Has(
      key, descriptor_pool, message_factory, arena, &result));
  return result;
}

template <typename Base>
absl::StatusOr<ListValue> MapValueMixin<Base>::ListKeys(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

  ListValue result;
  CEL_RETURN_IF_ERROR(static_cast<const Base*>(this)->ListKeys(
      descriptor_pool, message_factory, arena, &result));
  return result;
}

template <typename Base>
absl::StatusOr<Value> StructValueMixin<Base>::GetFieldByName(
    absl::string_view name,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

  Value result;
  CEL_RETURN_IF_ERROR(static_cast<const Base*>(this)->GetFieldByName(
      name, ProtoWrapperTypeOptions::kUnsetNull, descriptor_pool,
      message_factory, arena, &result));
  return result;
}

template <typename Base>
absl::StatusOr<Value> StructValueMixin<Base>::GetFieldByName(
    absl::string_view name, ProtoWrapperTypeOptions unboxing_options,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

  Value result;
  CEL_RETURN_IF_ERROR(static_cast<const Base*>(this)->GetFieldByName(
      name, unboxing_options, descriptor_pool, message_factory, arena,
      &result));
  return result;
}

template <typename Base>
absl::StatusOr<Value> StructValueMixin<Base>::GetFieldByNumber(
    int64_t number, const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

  Value result;
  CEL_RETURN_IF_ERROR(static_cast<const Base*>(this)->GetFieldByNumber(
      number, ProtoWrapperTypeOptions::kUnsetNull, descriptor_pool,
      message_factory, arena, &result));
  return result;
}

template <typename Base>
absl::StatusOr<Value> StructValueMixin<Base>::GetFieldByNumber(
    int64_t number, ProtoWrapperTypeOptions unboxing_options,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

  Value result;
  CEL_RETURN_IF_ERROR(static_cast<const Base*>(this)->GetFieldByNumber(
      number, unboxing_options, descriptor_pool, message_factory, arena,
      &result));
  return result;
}

template <typename Base>
absl::StatusOr<std::pair<Value, int>> StructValueMixin<Base>::Qualify(
    absl::Span<const SelectQualifier> qualifiers, bool presence_test,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK_GT(static_cast<int>(qualifiers.size()), 0);
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

  Value result;
  int count;
  CEL_RETURN_IF_ERROR(static_cast<const Base*>(this)->Qualify(
      qualifiers, presence_test, descriptor_pool, message_factory, arena,
      &result, &count));
  return std::pair{std::move(result), count};
}

}  // namespace common_internal

using ValueIteratorPtr = std::unique_ptr<ValueIterator>;

inline absl::StatusOr<Value> ValueIterator::Next(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

  Value result;
  CEL_RETURN_IF_ERROR(Next(descriptor_pool, message_factory, arena, &result));
  return result;
}

inline absl::StatusOr<absl::optional<Value>> ValueIterator::Next1(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

  Value key_or_value;
  CEL_ASSIGN_OR_RETURN(
      bool ok, Next1(descriptor_pool, message_factory, arena, &key_or_value));
  if (!ok) {
    return absl::nullopt;
  }
  return key_or_value;
}

inline absl::StatusOr<absl::optional<std::pair<Value, Value>>>
ValueIterator::Next2(const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                     google::protobuf::MessageFactory* absl_nonnull message_factory,
                     google::protobuf::Arena* absl_nonnull arena) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

  Value key;
  Value value;
  CEL_ASSIGN_OR_RETURN(
      bool ok, Next2(descriptor_pool, message_factory, arena, &key, &value));
  if (!ok) {
    return absl::nullopt;
  }
  return std::pair{std::move(key), std::move(value)};
}

absl_nonnull std::unique_ptr<ValueIterator> NewEmptyValueIterator();

class ValueBuilder {
 public:
  virtual ~ValueBuilder() = default;

  virtual absl::StatusOr<absl::optional<ErrorValue>> SetFieldByName(
      absl::string_view name, Value value) = 0;

  virtual absl::StatusOr<absl::optional<ErrorValue>> SetFieldByNumber(
      int64_t number, Value value) = 0;

  virtual absl::StatusOr<Value> Build() && = 0;
};

using ValueBuilderPtr = std::unique_ptr<ValueBuilder>;

absl_nonnull ListValueBuilderPtr
NewListValueBuilder(google::protobuf::Arena* absl_nonnull arena);

absl_nonnull MapValueBuilderPtr
NewMapValueBuilder(google::protobuf::Arena* absl_nonnull arena);

// Returns a new `StructValueBuilder`. Returns `nullptr` if there is no such
// message type with the name `name` in `descriptor_pool`. Returns an error if
// `message_factory` is unable to provide a prototype for the descriptor
// returned from `descriptor_pool`.
absl_nullable StructValueBuilderPtr NewStructValueBuilder(
    google::protobuf::Arena* absl_nonnull arena,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    absl::string_view name);

using ListValueBuilderInterface = ListValueBuilder;
using MapValueBuilderInterface = MapValueBuilder;
using StructValueBuilderInterface = StructValueBuilder;

// Now that Value is complete, we can define various parts of list, map, opaque,
// and struct which depend on Value.

namespace common_internal {

using MapFieldKeyAccessor = void (*)(const google::protobuf::MapKey&,
                                     const google::protobuf::Message* absl_nonnull,
                                     google::protobuf::Arena* absl_nonnull,
                                     Value* absl_nonnull);

absl::StatusOr<MapFieldKeyAccessor> MapFieldKeyAccessorFor(
    const google::protobuf::FieldDescriptor* absl_nonnull field);

using MapFieldValueAccessor = void (*)(
    const google::protobuf::MapValueConstRef&, const google::protobuf::Message* absl_nonnull,
    const google::protobuf::FieldDescriptor* absl_nonnull,
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull, google::protobuf::Arena* absl_nonnull,
    Value* absl_nonnull);

absl::StatusOr<MapFieldValueAccessor> MapFieldValueAccessorFor(
    const google::protobuf::FieldDescriptor* absl_nonnull field);

using RepeatedFieldAccessor =
    void (*)(int, const google::protobuf::Message* absl_nonnull,
             const google::protobuf::FieldDescriptor* absl_nonnull,
             const google::protobuf::Reflection* absl_nonnull,
             const google::protobuf::DescriptorPool* absl_nonnull,
             google::protobuf::MessageFactory* absl_nonnull, google::protobuf::Arena* absl_nonnull,
             Value* absl_nonnull);

absl::StatusOr<RepeatedFieldAccessor> RepeatedFieldAccessorFor(
    const google::protobuf::FieldDescriptor* absl_nonnull field);

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUE_H_
