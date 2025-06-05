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

// This file provides handling for well known protocol buffer types, which is
// agnostic to whether the types are dynamic or generated. It also performs
// exhaustive verification of the structure of the well known message types,
// ensuring they will work as intended throughout the rest of our codebase.
//
// For each well know type, there is a class `XReflection` where `X` is the
// unqualified well know type name. Each class can be initialized from a
// descriptor pool or a descriptor. Once initialized, they can be used with
// messages which use that exact descriptor. Using them with a different version
// of the descriptor from a separate descriptor pool results in undefined
// behavior. If unsure, you can initialize multiple times. If initializing with
// the same descriptor, it is a noop.

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_WELL_KNOWN_TYPES_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_WELL_KNOWN_TYPES_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/field_mask.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/variant.h"
#include "common/any.h"
#include "common/memory.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/message.h"
#include "google/protobuf/reflection.h"

namespace cel::well_known_types {

// Strongly typed variant capable of holding the value representation of any
// protocol buffer message string field. We do this instead of type aliasing to
// avoid collisions in other variants such as `well_known_types::Value`.
class StringValue final : public absl::variant<absl::string_view, absl::Cord> {
 public:
  using absl::variant<absl::string_view, absl::Cord>::variant;

  bool ConsumePrefix(absl::string_view prefix);
};

// Older versions of GCC do not deal with inheriting from variant correctly when
// using `visit`, so we cheat by upcasting.
inline const absl::variant<absl::string_view, absl::Cord>& AsVariant(
    const StringValue& value) {
  return static_cast<const absl::variant<absl::string_view, absl::Cord>&>(
      value);
}
inline absl::variant<absl::string_view, absl::Cord>& AsVariant(
    StringValue& value) {
  return static_cast<absl::variant<absl::string_view, absl::Cord>&>(value);
}
inline const absl::variant<absl::string_view, absl::Cord>&& AsVariant(
    const StringValue&& value) {
  return static_cast<const absl::variant<absl::string_view, absl::Cord>&&>(
      value);
}
inline absl::variant<absl::string_view, absl::Cord>&& AsVariant(
    StringValue&& value) {
  return static_cast<absl::variant<absl::string_view, absl::Cord>&&>(value);
}

inline bool operator==(const StringValue& lhs, const StringValue& rhs) {
  return absl::visit(
      [](const auto& lhs, const auto& rhs) { return lhs == rhs; },
      AsVariant(lhs), AsVariant(rhs));
}

inline bool operator!=(const StringValue& lhs, const StringValue& rhs) {
  return !operator==(lhs, rhs);
}

template <typename S>
void AbslStringify(S& sink, const StringValue& value) {
  sink.Append(absl::visit(
      [&](const auto& value) -> std::string { return absl::StrCat(value); },
      AsVariant(value)));
}

StringValue GetStringField(absl::Nonnull<const google::protobuf::Reflection*> reflection,
                           const google::protobuf::Message& message
                               ABSL_ATTRIBUTE_LIFETIME_BOUND,
                           absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
                           std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND);
inline StringValue GetStringField(
    const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return GetStringField(message.GetReflection(), message, field, scratch);
}

StringValue GetRepeatedStringField(
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
    std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND);
inline StringValue GetRepeatedStringField(
    const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
    std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return GetRepeatedStringField(message.GetReflection(), message, field, index,
                                scratch);
}

// Strongly typed variant capable of holding the value representation of any
// protocol buffer message bytes field. We do this instead of type aliasing to
// avoid collisions in other variants such as `well_known_types::Value`.
class BytesValue final : public absl::variant<absl::string_view, absl::Cord> {
 public:
  using absl::variant<absl::string_view, absl::Cord>::variant;
};

// Older versions of GCC do not deal with inheriting from variant correctly when
// using `visit`, so we cheat by upcasting.
inline const absl::variant<absl::string_view, absl::Cord>& AsVariant(
    const BytesValue& value) {
  return static_cast<const absl::variant<absl::string_view, absl::Cord>&>(
      value);
}
inline absl::variant<absl::string_view, absl::Cord>& AsVariant(
    BytesValue& value) {
  return static_cast<absl::variant<absl::string_view, absl::Cord>&>(value);
}
inline const absl::variant<absl::string_view, absl::Cord>&& AsVariant(
    const BytesValue&& value) {
  return static_cast<const absl::variant<absl::string_view, absl::Cord>&&>(
      value);
}
inline absl::variant<absl::string_view, absl::Cord>&& AsVariant(
    BytesValue&& value) {
  return static_cast<absl::variant<absl::string_view, absl::Cord>&&>(value);
}

inline bool operator==(const BytesValue& lhs, const BytesValue& rhs) {
  return absl::visit(
      [](const auto& lhs, const auto& rhs) { return lhs == rhs; },
      AsVariant(lhs), AsVariant(rhs));
}

inline bool operator!=(const BytesValue& lhs, const BytesValue& rhs) {
  return !operator==(lhs, rhs);
}

template <typename S>
void AbslStringify(S& sink, const BytesValue& value) {
  sink.Append(absl::visit(
      [&](const auto& value) -> std::string { return absl::StrCat(value); },
      AsVariant(value)));
}

BytesValue GetBytesField(absl::Nonnull<const google::protobuf::Reflection*> reflection,
                         const google::protobuf::Message& message
                             ABSL_ATTRIBUTE_LIFETIME_BOUND,
                         absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
                         std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND);
inline BytesValue GetBytesField(
    const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return GetBytesField(message.GetReflection(), message, field, scratch);
}

BytesValue GetRepeatedBytesField(
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
    std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND);
inline BytesValue GetRepeatedBytesField(
    const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
    std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return GetRepeatedBytesField(message.GetReflection(), message, field, index,
                               scratch);
}

class NullValueReflection final {
 public:
  NullValueReflection() = default;
  NullValueReflection(const NullValueReflection&) = default;
  NullValueReflection& operator=(const NullValueReflection&) = default;

  absl::Status Initialize(absl::Nonnull<const google::protobuf::DescriptorPool*> pool);

  absl::Status Initialize(
      absl::Nonnull<const google::protobuf::EnumDescriptor*> descriptor);

  bool IsInitialized() const { return descriptor_ != nullptr; }

 private:
  absl::Nullable<const google::protobuf::EnumDescriptor*> descriptor_ = nullptr;
  absl::Nullable<const google::protobuf::EnumValueDescriptor*> value_ = nullptr;
};

class BoolValueReflection final {
 public:
  static constexpr google::protobuf::Descriptor::WellKnownType kWellKnownType =
      google::protobuf::Descriptor::WELLKNOWNTYPE_BOOLVALUE;

  using GeneratedMessageType = google::protobuf::BoolValue;

  static bool GetValue(const GeneratedMessageType& message) {
    return message.value();
  }

  static void SetValue(absl::Nonnull<GeneratedMessageType*> message,
                       bool value) {
    message->set_value(value);
  }

  BoolValueReflection() = default;
  BoolValueReflection(const BoolValueReflection&) = default;
  BoolValueReflection& operator=(const BoolValueReflection&) = default;

  absl::Status Initialize(absl::Nonnull<const google::protobuf::DescriptorPool*> pool);

  absl::Status Initialize(absl::Nonnull<const google::protobuf::Descriptor*> descriptor);

  bool IsInitialized() const { return descriptor_ != nullptr; }

  absl::Nonnull<const google::protobuf::Descriptor*> GetDescriptor() const {
    ABSL_DCHECK(IsInitialized());
    return descriptor_;
  }

  bool GetValue(const google::protobuf::Message& message) const;

  void SetValue(absl::Nonnull<google::protobuf::Message*> message, bool value) const;

 private:
  absl::Nullable<const google::protobuf::Descriptor*> descriptor_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> value_field_ = nullptr;
};

absl::StatusOr<BoolValueReflection> GetBoolValueReflection(
    absl::Nonnull<const google::protobuf::Descriptor*> descriptor
        ABSL_ATTRIBUTE_LIFETIME_BOUND);

class Int32ValueReflection final {
 public:
  static constexpr google::protobuf::Descriptor::WellKnownType kWellKnownType =
      google::protobuf::Descriptor::WELLKNOWNTYPE_INT32VALUE;

  using GeneratedMessageType = google::protobuf::Int32Value;

  static int32_t GetValue(const GeneratedMessageType& message) {
    return message.value();
  }

  static void SetValue(absl::Nonnull<GeneratedMessageType*> message,
                       int32_t value) {
    message->set_value(value);
  }

  Int32ValueReflection() = default;
  Int32ValueReflection(const Int32ValueReflection&) = default;
  Int32ValueReflection& operator=(const Int32ValueReflection&) = default;

  absl::Status Initialize(absl::Nonnull<const google::protobuf::DescriptorPool*> pool);

  absl::Status Initialize(absl::Nonnull<const google::protobuf::Descriptor*> descriptor);

  bool IsInitialized() const { return descriptor_ != nullptr; }

  absl::Nonnull<const google::protobuf::Descriptor*> GetDescriptor() const {
    ABSL_DCHECK(IsInitialized());
    return descriptor_;
  }

  int32_t GetValue(const google::protobuf::Message& message) const;

  void SetValue(absl::Nonnull<google::protobuf::Message*> message, int32_t value) const;

 private:
  absl::Nullable<const google::protobuf::Descriptor*> descriptor_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> value_field_ = nullptr;
};

absl::StatusOr<Int32ValueReflection> GetInt32ValueReflection(
    absl::Nonnull<const google::protobuf::Descriptor*> descriptor
        ABSL_ATTRIBUTE_LIFETIME_BOUND);

class Int64ValueReflection final {
 public:
  static constexpr google::protobuf::Descriptor::WellKnownType kWellKnownType =
      google::protobuf::Descriptor::WELLKNOWNTYPE_INT64VALUE;

  using GeneratedMessageType = google::protobuf::Int64Value;

  static int64_t GetValue(const GeneratedMessageType& message) {
    return message.value();
  }

  static void SetValue(absl::Nonnull<GeneratedMessageType*> message,
                       int64_t value) {
    message->set_value(value);
  }

  Int64ValueReflection() = default;
  Int64ValueReflection(const Int64ValueReflection&) = default;
  Int64ValueReflection& operator=(const Int64ValueReflection&) = default;

  absl::Status Initialize(absl::Nonnull<const google::protobuf::DescriptorPool*> pool);

  absl::Status Initialize(absl::Nonnull<const google::protobuf::Descriptor*> descriptor);

  bool IsInitialized() const { return descriptor_ != nullptr; }

  absl::Nonnull<const google::protobuf::Descriptor*> GetDescriptor() const {
    ABSL_DCHECK(IsInitialized());
    return descriptor_;
  }

  int64_t GetValue(const google::protobuf::Message& message) const;

  void SetValue(absl::Nonnull<google::protobuf::Message*> message, int64_t value) const;

 private:
  absl::Nullable<const google::protobuf::Descriptor*> descriptor_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> value_field_ = nullptr;
};

absl::StatusOr<Int64ValueReflection> GetInt64ValueReflection(
    absl::Nonnull<const google::protobuf::Descriptor*> descriptor
        ABSL_ATTRIBUTE_LIFETIME_BOUND);

class UInt32ValueReflection final {
 public:
  static constexpr google::protobuf::Descriptor::WellKnownType kWellKnownType =
      google::protobuf::Descriptor::WELLKNOWNTYPE_UINT32VALUE;

  using GeneratedMessageType = google::protobuf::UInt32Value;

  static uint32_t GetValue(const GeneratedMessageType& message) {
    return message.value();
  }

  static void SetValue(absl::Nonnull<GeneratedMessageType*> message,
                       uint32_t value) {
    message->set_value(value);
  }

  UInt32ValueReflection() = default;
  UInt32ValueReflection(const UInt32ValueReflection&) = default;
  UInt32ValueReflection& operator=(const UInt32ValueReflection&) = default;

  absl::Status Initialize(absl::Nonnull<const google::protobuf::DescriptorPool*> pool);

  absl::Status Initialize(absl::Nonnull<const google::protobuf::Descriptor*> descriptor);

  bool IsInitialized() const { return descriptor_ != nullptr; }

  absl::Nonnull<const google::protobuf::Descriptor*> GetDescriptor() const {
    ABSL_DCHECK(IsInitialized());
    return descriptor_;
  }

  uint32_t GetValue(const google::protobuf::Message& message) const;

  void SetValue(absl::Nonnull<google::protobuf::Message*> message, uint32_t value) const;

 private:
  absl::Nullable<const google::protobuf::Descriptor*> descriptor_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> value_field_ = nullptr;
};

absl::StatusOr<UInt32ValueReflection> GetUInt32ValueReflection(
    absl::Nonnull<const google::protobuf::Descriptor*> descriptor
        ABSL_ATTRIBUTE_LIFETIME_BOUND);

class UInt64ValueReflection final {
 public:
  static constexpr google::protobuf::Descriptor::WellKnownType kWellKnownType =
      google::protobuf::Descriptor::WELLKNOWNTYPE_UINT64VALUE;

  using GeneratedMessageType = google::protobuf::UInt64Value;

  static uint64_t GetValue(const GeneratedMessageType& message) {
    return message.value();
  }

  static void SetValue(absl::Nonnull<GeneratedMessageType*> message,
                       uint64_t value) {
    message->set_value(value);
  }

  UInt64ValueReflection() = default;
  UInt64ValueReflection(const UInt64ValueReflection&) = default;
  UInt64ValueReflection& operator=(const UInt64ValueReflection&) = default;

  absl::Status Initialize(absl::Nonnull<const google::protobuf::DescriptorPool*> pool);

  absl::Status Initialize(absl::Nonnull<const google::protobuf::Descriptor*> descriptor);

  bool IsInitialized() const { return descriptor_ != nullptr; }

  absl::Nonnull<const google::protobuf::Descriptor*> GetDescriptor() const {
    ABSL_DCHECK(IsInitialized());
    return descriptor_;
  }

  uint64_t GetValue(const google::protobuf::Message& message) const;

  void SetValue(absl::Nonnull<google::protobuf::Message*> message, uint64_t value) const;

 private:
  absl::Nullable<const google::protobuf::Descriptor*> descriptor_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> value_field_ = nullptr;
};

absl::StatusOr<UInt64ValueReflection> GetUInt64ValueReflection(
    absl::Nonnull<const google::protobuf::Descriptor*> descriptor
        ABSL_ATTRIBUTE_LIFETIME_BOUND);

class FloatValueReflection final {
 public:
  static constexpr google::protobuf::Descriptor::WellKnownType kWellKnownType =
      google::protobuf::Descriptor::WELLKNOWNTYPE_FLOATVALUE;

  using GeneratedMessageType = google::protobuf::FloatValue;

  static float GetValue(const GeneratedMessageType& message) {
    return message.value();
  }

  static void SetValue(absl::Nonnull<GeneratedMessageType*> message,
                       float value) {
    message->set_value(value);
  }

  FloatValueReflection() = default;
  FloatValueReflection(const FloatValueReflection&) = default;
  FloatValueReflection& operator=(const FloatValueReflection&) = default;

  absl::Status Initialize(absl::Nonnull<const google::protobuf::DescriptorPool*> pool);

  absl::Status Initialize(absl::Nonnull<const google::protobuf::Descriptor*> descriptor);

  bool IsInitialized() const { return descriptor_ != nullptr; }

  absl::Nonnull<const google::protobuf::Descriptor*> GetDescriptor() const {
    ABSL_DCHECK(IsInitialized());
    return descriptor_;
  }

  float GetValue(const google::protobuf::Message& message) const;

  void SetValue(absl::Nonnull<google::protobuf::Message*> message, float value) const;

 private:
  absl::Nullable<const google::protobuf::Descriptor*> descriptor_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> value_field_ = nullptr;
};

absl::StatusOr<FloatValueReflection> GetFloatValueReflection(
    absl::Nonnull<const google::protobuf::Descriptor*> descriptor
        ABSL_ATTRIBUTE_LIFETIME_BOUND);

class DoubleValueReflection final {
 public:
  static constexpr google::protobuf::Descriptor::WellKnownType kWellKnownType =
      google::protobuf::Descriptor::WELLKNOWNTYPE_DOUBLEVALUE;

  using GeneratedMessageType = google::protobuf::DoubleValue;

  static double GetValue(const GeneratedMessageType& message) {
    return message.value();
  }

  static void SetValue(absl::Nonnull<GeneratedMessageType*> message,
                       double value) {
    message->set_value(value);
  }

  DoubleValueReflection() = default;
  DoubleValueReflection(const DoubleValueReflection&) = default;
  DoubleValueReflection& operator=(const DoubleValueReflection&) = default;

  absl::Status Initialize(absl::Nonnull<const google::protobuf::DescriptorPool*> pool);

  absl::Status Initialize(absl::Nonnull<const google::protobuf::Descriptor*> descriptor);

  bool IsInitialized() const { return descriptor_ != nullptr; }

  absl::Nonnull<const google::protobuf::Descriptor*> GetDescriptor() const {
    ABSL_DCHECK(IsInitialized());
    return descriptor_;
  }

  double GetValue(const google::protobuf::Message& message) const;

  void SetValue(absl::Nonnull<google::protobuf::Message*> message, double value) const;

 private:
  absl::Nullable<const google::protobuf::Descriptor*> descriptor_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> value_field_ = nullptr;
};

absl::StatusOr<DoubleValueReflection> GetDoubleValueReflection(
    absl::Nonnull<const google::protobuf::Descriptor*> descriptor
        ABSL_ATTRIBUTE_LIFETIME_BOUND);

class BytesValueReflection final {
 public:
  static constexpr google::protobuf::Descriptor::WellKnownType kWellKnownType =
      google::protobuf::Descriptor::WELLKNOWNTYPE_BYTESVALUE;

  using GeneratedMessageType = google::protobuf::BytesValue;

  static absl::Cord GetValue(const GeneratedMessageType& message) {
    return absl::Cord(message.value());
  }

  static void SetValue(absl::Nonnull<GeneratedMessageType*> message,
                       const absl::Cord& value) {
    message->set_value(static_cast<std::string>(value));
  }

  BytesValueReflection() = default;
  BytesValueReflection(const BytesValueReflection&) = default;
  BytesValueReflection& operator=(const BytesValueReflection&) = default;

  absl::Status Initialize(absl::Nonnull<const google::protobuf::DescriptorPool*> pool);

  absl::Status Initialize(absl::Nonnull<const google::protobuf::Descriptor*> descriptor);

  bool IsInitialized() const { return descriptor_ != nullptr; }

  absl::Nonnull<const google::protobuf::Descriptor*> GetDescriptor() const {
    ABSL_DCHECK(IsInitialized());
    return descriptor_;
  }

  BytesValue GetValue(const google::protobuf::Message& message
                          ABSL_ATTRIBUTE_LIFETIME_BOUND,
                      std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  void SetValue(absl::Nonnull<google::protobuf::Message*> message,
                absl::string_view value) const;

  void SetValue(absl::Nonnull<google::protobuf::Message*> message,
                const absl::Cord& value) const;

 private:
  absl::Nullable<const google::protobuf::Descriptor*> descriptor_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> value_field_ = nullptr;
  google::protobuf::FieldDescriptor::CppStringType value_field_string_type_;
};

absl::StatusOr<BytesValueReflection> GetBytesValueReflection(
    absl::Nonnull<const google::protobuf::Descriptor*> descriptor
        ABSL_ATTRIBUTE_LIFETIME_BOUND);

class StringValueReflection final {
 public:
  static constexpr google::protobuf::Descriptor::WellKnownType kWellKnownType =
      google::protobuf::Descriptor::WELLKNOWNTYPE_STRINGVALUE;

  using GeneratedMessageType = google::protobuf::StringValue;

  static absl::string_view GetValue(
      const GeneratedMessageType& message ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    return message.value();
  }

  static void SetValue(absl::Nonnull<GeneratedMessageType*> message,
                       absl::string_view value) {
    message->set_value(value);
  }

  StringValueReflection() = default;
  StringValueReflection(const StringValueReflection&) = default;
  StringValueReflection& operator=(const StringValueReflection&) = default;

  absl::Status Initialize(absl::Nonnull<const google::protobuf::DescriptorPool*> pool);

  absl::Status Initialize(absl::Nonnull<const google::protobuf::Descriptor*> descriptor);

  bool IsInitialized() const { return descriptor_ != nullptr; }

  absl::Nonnull<const google::protobuf::Descriptor*> GetDescriptor() const {
    ABSL_DCHECK(IsInitialized());
    return descriptor_;
  }

  StringValue GetValue(
      const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND,
      std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  void SetValue(absl::Nonnull<google::protobuf::Message*> message,
                absl::string_view value) const;

  void SetValue(absl::Nonnull<google::protobuf::Message*> message,
                const absl::Cord& value) const;

 private:
  absl::Nullable<const google::protobuf::Descriptor*> descriptor_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> value_field_ = nullptr;
  google::protobuf::FieldDescriptor::CppStringType value_field_string_type_;
};

absl::StatusOr<StringValueReflection> GetStringValueReflection(
    absl::Nonnull<const google::protobuf::Descriptor*> descriptor
        ABSL_ATTRIBUTE_LIFETIME_BOUND);

class AnyReflection final {
 public:
  static constexpr google::protobuf::Descriptor::WellKnownType kWellKnownType =
      google::protobuf::Descriptor::WELLKNOWNTYPE_ANY;

  using GeneratedMessageType = google::protobuf::Any;

  static absl::string_view GetTypeUrl(
      const GeneratedMessageType& message ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    return message.type_url();
  }

  static absl::Cord GetValue(const GeneratedMessageType& message) {
    return GetAnyValueAsCord(message);
  }

  static void SetTypeUrl(absl::Nonnull<GeneratedMessageType*> message,
                         absl::string_view type_url) {
    message->set_type_url(type_url);
  }

  static void SetValue(absl::Nonnull<GeneratedMessageType*> message,
                       const absl::Cord& value) {
    SetAnyValueFromCord(message, value);
  }

  AnyReflection() = default;
  AnyReflection(const AnyReflection&) = default;
  AnyReflection& operator=(const AnyReflection&) = default;

  absl::Status Initialize(absl::Nonnull<const google::protobuf::DescriptorPool*> pool);

  absl::Status Initialize(absl::Nonnull<const google::protobuf::Descriptor*> descriptor);

  bool IsInitialized() const { return descriptor_ != nullptr; }

  absl::Nonnull<const google::protobuf::Descriptor*> GetDescriptor() const {
    ABSL_DCHECK(IsInitialized());
    return descriptor_;
  }

  void SetTypeUrl(absl::Nonnull<google::protobuf::Message*> message,
                  absl::string_view type_url) const;

  void SetValue(absl::Nonnull<google::protobuf::Message*> message,
                const absl::Cord& value) const;

  StringValue GetTypeUrl(
      const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND,
      std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  BytesValue GetValue(const google::protobuf::Message& message
                          ABSL_ATTRIBUTE_LIFETIME_BOUND,
                      std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

 private:
  absl::Nullable<const google::protobuf::Descriptor*> descriptor_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> type_url_field_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> value_field_ = nullptr;
  google::protobuf::FieldDescriptor::CppStringType type_url_field_string_type_;
  google::protobuf::FieldDescriptor::CppStringType value_field_string_type_;
};

absl::StatusOr<AnyReflection> GetAnyReflection(
    absl::Nonnull<const google::protobuf::Descriptor*> descriptor
        ABSL_ATTRIBUTE_LIFETIME_BOUND);

AnyReflection GetAnyReflectionOrDie(
    absl::Nonnull<const google::protobuf::Descriptor*> descriptor
        ABSL_ATTRIBUTE_LIFETIME_BOUND);

class DurationReflection final {
 public:
  static constexpr google::protobuf::Descriptor::WellKnownType kWellKnownType =
      google::protobuf::Descriptor::WELLKNOWNTYPE_DURATION;

  using GeneratedMessageType = google::protobuf::Duration;

  static int64_t GetSeconds(const GeneratedMessageType& message) {
    return message.seconds();
  }

  static int64_t GetNanos(const GeneratedMessageType& message) {
    return message.nanos();
  }

  static void SetSeconds(absl::Nonnull<GeneratedMessageType*> message,
                         int64_t value) {
    message->set_seconds(value);
  }

  static void SetNanos(absl::Nonnull<GeneratedMessageType*> message,
                       int32_t value) {
    message->set_nanos(value);
  }

  DurationReflection() = default;
  DurationReflection(const DurationReflection&) = default;
  DurationReflection& operator=(const DurationReflection&) = default;

  absl::Status Initialize(absl::Nonnull<const google::protobuf::DescriptorPool*> pool);

  absl::Status Initialize(absl::Nonnull<const google::protobuf::Descriptor*> descriptor);

  bool IsInitialized() const { return descriptor_ != nullptr; }

  absl::Nonnull<const google::protobuf::Descriptor*> GetDescriptor() const {
    ABSL_DCHECK(IsInitialized());
    return descriptor_;
  }

  int64_t GetSeconds(const google::protobuf::Message& message) const;

  int32_t GetNanos(const google::protobuf::Message& message) const;

  void SetSeconds(absl::Nonnull<google::protobuf::Message*> message, int64_t value) const;

  void SetNanos(absl::Nonnull<google::protobuf::Message*> message, int32_t value) const;

  absl::Status SetFromAbslDuration(absl::Nonnull<google::protobuf::Message*> message,
                                   absl::Duration duration) const;

  // Converts `absl::Duration` to `google.protobuf.Duration` without performing
  // validity checks. Avoid use.
  void UnsafeSetFromAbslDuration(absl::Nonnull<google::protobuf::Message*> message,
                                 absl::Duration duration) const;

  absl::StatusOr<absl::Duration> ToAbslDuration(
      const google::protobuf::Message& message) const;

  // Converts `google.protobuf.Duration` to `absl::Duration` without performing
  // validity checks. Avoid use.
  absl::Duration UnsafeToAbslDuration(const google::protobuf::Message& message) const;

 private:
  absl::Nullable<const google::protobuf::Descriptor*> descriptor_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> seconds_field_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> nanos_field_ = nullptr;
};

absl::StatusOr<DurationReflection> GetDurationReflection(
    absl::Nonnull<const google::protobuf::Descriptor*> descriptor
        ABSL_ATTRIBUTE_LIFETIME_BOUND);

class TimestampReflection final {
 public:
  static constexpr google::protobuf::Descriptor::WellKnownType kWellKnownType =
      google::protobuf::Descriptor::WELLKNOWNTYPE_TIMESTAMP;

  using GeneratedMessageType = google::protobuf::Timestamp;

  static int64_t GetSeconds(const GeneratedMessageType& message) {
    return message.seconds();
  }

  static int64_t GetNanos(const GeneratedMessageType& message) {
    return message.nanos();
  }

  static void SetSeconds(absl::Nonnull<GeneratedMessageType*> message,
                         int64_t value) {
    message->set_seconds(value);
  }

  static void SetNanos(absl::Nonnull<GeneratedMessageType*> message,
                       int32_t value) {
    message->set_nanos(value);
  }

  TimestampReflection() = default;
  TimestampReflection(const TimestampReflection&) = default;
  TimestampReflection& operator=(const TimestampReflection&) = default;

  absl::Status Initialize(absl::Nonnull<const google::protobuf::DescriptorPool*> pool);

  absl::Status Initialize(absl::Nonnull<const google::protobuf::Descriptor*> descriptor);

  bool IsInitialized() const { return descriptor_ != nullptr; }

  absl::Nonnull<const google::protobuf::Descriptor*> GetDescriptor() const {
    ABSL_DCHECK(IsInitialized());
    return descriptor_;
  }

  int64_t GetSeconds(const google::protobuf::Message& message) const;

  int32_t GetNanos(const google::protobuf::Message& message) const;

  void SetSeconds(absl::Nonnull<google::protobuf::Message*> message, int64_t value) const;

  void SetNanos(absl::Nonnull<google::protobuf::Message*> message, int32_t value) const;

  absl::StatusOr<absl::Time> ToAbslTime(const google::protobuf::Message& message) const;

  // Converts `absl::Time` to `google.protobuf.Timestamp` without performing
  // validity checks. Avoid use.
  absl::Time UnsafeToAbslTime(const google::protobuf::Message& message) const;

  absl::Status SetFromAbslTime(absl::Nonnull<google::protobuf::Message*> message,
                               absl::Time time) const;

  // Converts `google.protobuf.Timestamp` to `absl::Time` without performing
  // validity checks. Avoid use.
  void UnsafeSetFromAbslTime(absl::Nonnull<google::protobuf::Message*> message,
                             absl::Time time) const;

 private:
  absl::Nullable<const google::protobuf::Descriptor*> descriptor_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> seconds_field_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> nanos_field_ = nullptr;
};

absl::StatusOr<TimestampReflection> GetTimestampReflection(
    absl::Nonnull<const google::protobuf::Descriptor*> descriptor
        ABSL_ATTRIBUTE_LIFETIME_BOUND);

class ValueReflection final {
 public:
  static constexpr google::protobuf::Descriptor::WellKnownType kWellKnownType =
      google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE;

  using GeneratedMessageType = google::protobuf::Value;

  static google::protobuf::Value::KindCase GetKindCase(
      const google::protobuf::Value& message) {
    return message.kind_case();
  }

  static bool GetBoolValue(const GeneratedMessageType& message) {
    return message.bool_value();
  }

  static double GetNumberValue(const GeneratedMessageType& message) {
    return message.number_value();
  }

  static absl::string_view GetStringValue(
      const GeneratedMessageType& message ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    return message.string_value();
  }

  static const google::protobuf::ListValue& GetListValue(
      const GeneratedMessageType& message ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    return message.list_value();
  }

  static const google::protobuf::Struct& GetStructValue(
      const GeneratedMessageType& message ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    return message.struct_value();
  }

  static void SetNullValue(absl::Nonnull<GeneratedMessageType*> message) {
    message->set_null_value(google::protobuf::NULL_VALUE);
  }

  static void SetBoolValue(absl::Nonnull<GeneratedMessageType*> message,
                           bool value) {
    message->set_bool_value(value);
  }

  static void SetNumberValue(absl::Nonnull<GeneratedMessageType*> message,
                             int64_t value);

  static void SetNumberValue(absl::Nonnull<GeneratedMessageType*> message,
                             uint64_t value);

  static void SetNumberValue(absl::Nonnull<GeneratedMessageType*> message,
                             double value) {
    message->set_number_value(value);
  }

  static void SetStringValue(absl::Nonnull<GeneratedMessageType*> message,
                             absl::string_view value) {
    message->set_string_value(value);
  }

  static void SetStringValue(absl::Nonnull<GeneratedMessageType*> message,
                             const absl::Cord& value) {
    message->set_string_value(static_cast<std::string>(value));
  }

  static absl::Nonnull<google::protobuf::ListValue*> MutableListValue(
      absl::Nonnull<GeneratedMessageType*> message) {
    return message->mutable_list_value();
  }

  static absl::Nonnull<google::protobuf::Struct*> MutableStructValue(
      absl::Nonnull<GeneratedMessageType*> message) {
    return message->mutable_struct_value();
  }

  ValueReflection() = default;
  ValueReflection(const ValueReflection&) = default;
  ValueReflection& operator=(const ValueReflection&) = default;

  absl::Status Initialize(absl::Nonnull<const google::protobuf::DescriptorPool*> pool);

  absl::Status Initialize(absl::Nonnull<const google::protobuf::Descriptor*> descriptor);

  bool IsInitialized() const { return descriptor_ != nullptr; }

  absl::Nonnull<const google::protobuf::Descriptor*> GetDescriptor() const {
    ABSL_DCHECK(IsInitialized());
    return descriptor_;
  }

  absl::Nonnull<const google::protobuf::Descriptor*> GetStructDescriptor() const {
    ABSL_DCHECK(IsInitialized());
    return struct_value_field_->message_type();
  }

  absl::Nonnull<const google::protobuf::Descriptor*> GetListValueDescriptor() const {
    ABSL_DCHECK(IsInitialized());
    return list_value_field_->message_type();
  }

  google::protobuf::Value::KindCase GetKindCase(
      const google::protobuf::Message& message) const;

  bool GetBoolValue(const google::protobuf::Message& message) const;

  double GetNumberValue(const google::protobuf::Message& message) const;

  StringValue GetStringValue(
      const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND,
      std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  const google::protobuf::Message& GetListValue(
      const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  const google::protobuf::Message& GetStructValue(
      const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  void SetNullValue(absl::Nonnull<google::protobuf::Message*> message) const;

  void SetBoolValue(absl::Nonnull<google::protobuf::Message*> message, bool value) const;

  void SetNumberValue(absl::Nonnull<google::protobuf::Message*> message,
                      int64_t value) const;

  void SetNumberValue(absl::Nonnull<google::protobuf::Message*> message,
                      uint64_t value) const;

  void SetNumberValue(absl::Nonnull<google::protobuf::Message*> message,
                      double value) const;

  void SetStringValue(absl::Nonnull<google::protobuf::Message*> message,
                      absl::string_view value) const;

  void SetStringValue(absl::Nonnull<google::protobuf::Message*> message,
                      const absl::Cord& value) const;

  void SetStringValueFromBytes(absl::Nonnull<google::protobuf::Message*> message,
                               absl::string_view value) const;

  void SetStringValueFromBytes(absl::Nonnull<google::protobuf::Message*> message,
                               const absl::Cord& value) const;

  void SetStringValueFromDuration(absl::Nonnull<google::protobuf::Message*> message,
                                  absl::Duration duration) const;

  void SetStringValueFromTimestamp(absl::Nonnull<google::protobuf::Message*> message,
                                   absl::Time time) const;

  absl::Nonnull<google::protobuf::Message*> MutableListValue(
      absl::Nonnull<google::protobuf::Message*> message) const;

  absl::Nonnull<google::protobuf::Message*> MutableStructValue(
      absl::Nonnull<google::protobuf::Message*> message) const;

  Unique<google::protobuf::Message> ReleaseListValue(
      absl::Nonnull<google::protobuf::Message*> message) const;

  Unique<google::protobuf::Message> ReleaseStructValue(
      absl::Nonnull<google::protobuf::Message*> message) const;

 private:
  absl::Nullable<const google::protobuf::Descriptor*> descriptor_ = nullptr;
  absl::Nullable<const google::protobuf::OneofDescriptor*> kind_field_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> null_value_field_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> bool_value_field_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> number_value_field_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> string_value_field_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> list_value_field_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> struct_value_field_ = nullptr;
  google::protobuf::FieldDescriptor::CppStringType string_value_field_string_type_;
};

absl::StatusOr<ValueReflection> GetValueReflection(
    absl::Nonnull<const google::protobuf::Descriptor*> descriptor
        ABSL_ATTRIBUTE_LIFETIME_BOUND);

// `GetValueReflectionOrDie()` is the same as `GetValueReflection`
// except that it aborts if `descriptor` is not a well formed descriptor of
// `google.protobuf.Value`. This should only be used in places where it is
// guaranteed that the aforementioned prerequisites are met.
ValueReflection GetValueReflectionOrDie(
    absl::Nonnull<const google::protobuf::Descriptor*> descriptor
        ABSL_ATTRIBUTE_LIFETIME_BOUND);

class ListValueReflection final {
 public:
  static constexpr google::protobuf::Descriptor::WellKnownType kWellKnownType =
      google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE;

  using GeneratedMessageType = google::protobuf::ListValue;

  static int ValuesSize(const GeneratedMessageType& message) {
    return message.values_size();
  }

  static const google::protobuf::RepeatedPtrField<google::protobuf::Value>& Values(
      const GeneratedMessageType& message ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    return message.values();
  }

  static const google::protobuf::Value& Values(
      const GeneratedMessageType& message ABSL_ATTRIBUTE_LIFETIME_BOUND,
      int index) {
    return message.values(index);
  }

  static google::protobuf::RepeatedPtrField<google::protobuf::Value>& MutableValues(
      absl::Nonnull<GeneratedMessageType*> message
          ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    return *message->mutable_values();
  }

  static absl::Nonnull<google::protobuf::Value*> AddValues(
      absl::Nonnull<GeneratedMessageType*> message
          ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    return message->add_values();
  }

  static void ReserveValues(absl::Nonnull<GeneratedMessageType*> message,
                            int capacity) {
    if (capacity > 0) {
      message->mutable_values()->Reserve(capacity);
    }
  }

  absl::Status Initialize(absl::Nonnull<const google::protobuf::DescriptorPool*> pool);

  absl::Status Initialize(absl::Nonnull<const google::protobuf::Descriptor*> descriptor);

  bool IsInitialized() const { return descriptor_ != nullptr; }

  absl::Nonnull<const google::protobuf::Descriptor*> GetDescriptor() const {
    ABSL_DCHECK(IsInitialized());
    return descriptor_;
  }

  absl::Nonnull<const google::protobuf::Descriptor*> GetValueDescriptor() const {
    ABSL_DCHECK(IsInitialized());
    return values_field_->message_type();
  }

  absl::Nonnull<const google::protobuf::FieldDescriptor*> GetValuesDescriptor() const {
    ABSL_DCHECK(IsInitialized());
    return values_field_;
  }

  int ValuesSize(const google::protobuf::Message& message) const;

  google::protobuf::RepeatedFieldRef<google::protobuf::Message> Values(
      const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  const google::protobuf::Message& Values(const google::protobuf::Message& message
                                    ABSL_ATTRIBUTE_LIFETIME_BOUND,
                                int index) const;

  google::protobuf::MutableRepeatedFieldRef<google::protobuf::Message> MutableValues(
      absl::Nonnull<google::protobuf::Message*> message
          ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  absl::Nonnull<google::protobuf::Message*> AddValues(
      absl::Nonnull<google::protobuf::Message*> message) const;

  void ReserveValues(absl::Nonnull<google::protobuf::Message*> message,
                     int capacity) const;

 private:
  absl::Nullable<const google::protobuf::Descriptor*> descriptor_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> values_field_ = nullptr;
};

absl::StatusOr<ListValueReflection> GetListValueReflection(
    absl::Nonnull<const google::protobuf::Descriptor*> descriptor
        ABSL_ATTRIBUTE_LIFETIME_BOUND);

// `GetListValueReflectionOrDie()` is the same as `GetListValueReflection`
// except that it aborts if `descriptor` is not a well formed descriptor of
// `google.protobuf.ListValue`. This should only be used in places where it is
// guaranteed that the aforementioned prerequisites are met.
ListValueReflection GetListValueReflectionOrDie(
    absl::Nonnull<const google::protobuf::Descriptor*> descriptor
        ABSL_ATTRIBUTE_LIFETIME_BOUND);

class StructReflection final {
 public:
  static constexpr google::protobuf::Descriptor::WellKnownType kWellKnownType =
      google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT;

  using GeneratedMessageType = google::protobuf::Struct;

  static int FieldsSize(const GeneratedMessageType& message) {
    return message.fields_size();
  }

  static auto BeginFields(
      const GeneratedMessageType& message ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    return message.fields().begin();
  }

  static auto EndFields(
      const GeneratedMessageType& message ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    return message.fields().end();
  }

  static bool ContainsField(const GeneratedMessageType& message,
                            absl::string_view name) {
    return message.fields().contains(name);
  }

  static absl::Nullable<const google::protobuf::Value*> FindField(
      const GeneratedMessageType& message ABSL_ATTRIBUTE_LIFETIME_BOUND,
      absl::string_view name) {
    if (auto it = message.fields().find(name); it != message.fields().end()) {
      return &it->second;
    }
    return nullptr;
  }

  static absl::Nonnull<google::protobuf::Value*> InsertField(
      absl::Nonnull<GeneratedMessageType*> message
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      absl::string_view name) {
    return &(*message->mutable_fields())[name];
  }

  static bool DeleteField(absl::Nonnull<GeneratedMessageType*> message,
                          absl::string_view name) {
    return message->mutable_fields()->erase(name) > 0;
  }

  absl::Status Initialize(absl::Nonnull<const google::protobuf::DescriptorPool*> pool);

  absl::Status Initialize(absl::Nonnull<const google::protobuf::Descriptor*> descriptor);

  bool IsInitialized() const { return descriptor_ != nullptr; }

  absl::Nonnull<const google::protobuf::Descriptor*> GetDescriptor() const {
    ABSL_DCHECK(IsInitialized());
    return descriptor_;
  }

  absl::Nonnull<const google::protobuf::Descriptor*> GetValueDescriptor() const {
    ABSL_DCHECK(IsInitialized());
    return fields_value_field_->message_type();
  }

  absl::Nonnull<const google::protobuf::FieldDescriptor*> GetFieldsDescriptor() const {
    ABSL_DCHECK(IsInitialized());
    return fields_field_;
  }

  int FieldsSize(const google::protobuf::Message& message) const;

  google::protobuf::MapIterator BeginFields(
      const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  google::protobuf::MapIterator EndFields(
      const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  bool ContainsField(const google::protobuf::Message& message,
                     absl::string_view name) const;

  absl::Nullable<const google::protobuf::Message*> FindField(
      const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND,
      absl::string_view name) const;

  absl::Nonnull<google::protobuf::Message*> InsertField(
      absl::Nonnull<google::protobuf::Message*> message ABSL_ATTRIBUTE_LIFETIME_BOUND,
      absl::string_view name) const;

  bool DeleteField(absl::Nonnull<google::protobuf::Message*> message
                       ABSL_ATTRIBUTE_LIFETIME_BOUND,
                   absl::string_view name) const;

 private:
  absl::Nullable<const google::protobuf::Descriptor*> descriptor_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> fields_field_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> fields_key_field_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> fields_value_field_ = nullptr;
};

absl::StatusOr<StructReflection> GetStructReflection(
    absl::Nonnull<const google::protobuf::Descriptor*> descriptor
        ABSL_ATTRIBUTE_LIFETIME_BOUND);

// `GetStructReflectionOrDie()` is the same as `GetStructReflection`
// except that it aborts if `descriptor` is not a well formed descriptor of
// `google.protobuf.Struct`. This should only be used in places where it is
// guaranteed that the aforementioned prerequisites are met.
StructReflection GetStructReflectionOrDie(
    absl::Nonnull<const google::protobuf::Descriptor*> descriptor
        ABSL_ATTRIBUTE_LIFETIME_BOUND);

class FieldMaskReflection final {
 public:
  static constexpr google::protobuf::Descriptor::WellKnownType kWellKnownType =
      google::protobuf::Descriptor::WELLKNOWNTYPE_FIELDMASK;

  using GeneratedMessageType = google::protobuf::FieldMask;

  static int PathsSize(const GeneratedMessageType& message) {
    return message.paths_size();
  }

  static absl::string_view Paths(const GeneratedMessageType& message
                                     ABSL_ATTRIBUTE_LIFETIME_BOUND,
                                 int index) {
    return message.paths(index);
  }

  absl::Status Initialize(absl::Nonnull<const google::protobuf::DescriptorPool*> pool);

  absl::Status Initialize(absl::Nonnull<const google::protobuf::Descriptor*> descriptor);

  bool IsInitialized() const { return descriptor_ != nullptr; }

  absl::Nonnull<const google::protobuf::Descriptor*> GetDescriptor() const {
    ABSL_DCHECK(IsInitialized());
    return descriptor_;
  }

  int PathsSize(const google::protobuf::Message& message) const;

  StringValue Paths(
      const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND, int index,
      std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

 private:
  absl::Nullable<const google::protobuf::Descriptor*> descriptor_ = nullptr;
  absl::Nullable<const google::protobuf::FieldDescriptor*> paths_field_ = nullptr;
  google::protobuf::FieldDescriptor::CppStringType paths_field_string_type_;
};

absl::StatusOr<FieldMaskReflection> GetFieldMaskReflection(
    absl::Nonnull<const google::protobuf::Descriptor*> descriptor
        ABSL_ATTRIBUTE_LIFETIME_BOUND);

using ListValuePtr = Unique<google::protobuf::Message>;

using ListValueConstRef = std::reference_wrapper<const google::protobuf::Message>;

using StructPtr = Unique<google::protobuf::Message>;

using StructConstRef = std::reference_wrapper<const google::protobuf::Message>;

// Variant holding `std::reference_wrapper<const
// google::protobuf::Message>` or `Unique<google::protobuf::Message>`, either of which is an
// instance of `google.protobuf.ListValue` which is either a generated message
// or dynamic message.
class ListValue final : public absl::variant<ListValueConstRef, ListValuePtr> {
  using absl::variant<ListValueConstRef, ListValuePtr>::variant;
};

// Older versions of GCC do not deal with inheriting from variant correctly when
// using `visit`, so we cheat by upcasting.
inline const absl::variant<ListValueConstRef, ListValuePtr>& AsVariant(
    const ListValue& value) {
  return static_cast<const absl::variant<ListValueConstRef, ListValuePtr>&>(
      value);
}
inline absl::variant<ListValueConstRef, ListValuePtr>& AsVariant(
    ListValue& value) {
  return static_cast<absl::variant<ListValueConstRef, ListValuePtr>&>(value);
}
inline const absl::variant<ListValueConstRef, ListValuePtr>&& AsVariant(
    const ListValue&& value) {
  return static_cast<const absl::variant<ListValueConstRef, ListValuePtr>&&>(
      value);
}
inline absl::variant<ListValueConstRef, ListValuePtr>&& AsVariant(
    ListValue&& value) {
  return static_cast<absl::variant<ListValueConstRef, ListValuePtr>&&>(value);
}

// Variant holding `std::reference_wrapper<const
// google::protobuf::Message>` or `Unique<google::protobuf::Message>`, either of which is an
// instance of `google.protobuf.Struct` which is either a generated message or
// dynamic message.
class Struct final : public absl::variant<StructConstRef, StructPtr> {
 public:
  using absl::variant<StructConstRef, StructPtr>::variant;
};

// Older versions of GCC do not deal with inheriting from variant correctly when
// using `visit`, so we cheat by upcasting.
inline const absl::variant<StructConstRef, StructPtr>& AsVariant(
    const Struct& value) {
  return static_cast<const absl::variant<StructConstRef, StructPtr>&>(value);
}
inline absl::variant<StructConstRef, StructPtr>& AsVariant(Struct& value) {
  return static_cast<absl::variant<StructConstRef, StructPtr>&>(value);
}
inline const absl::variant<StructConstRef, StructPtr>&& AsVariant(
    const Struct&& value) {
  return static_cast<const absl::variant<StructConstRef, StructPtr>&&>(value);
}
inline absl::variant<StructConstRef, StructPtr>&& AsVariant(Struct&& value) {
  return static_cast<absl::variant<StructConstRef, StructPtr>&&>(value);
}

// Variant capable of representing any unwrapped well known type or message.
using Value = absl::variant<absl::monostate, std::nullptr_t, bool, int32_t,
                            int64_t, uint32_t, uint64_t, float, double,
                            StringValue, BytesValue, absl::Duration, absl::Time,
                            ListValue, Struct, Unique<google::protobuf::Message>>;

// Unpacks the given instance of `google.protobuf.Any`.
absl::StatusOr<Unique<google::protobuf::Message>> UnpackAnyFrom(
    absl::Nullable<google::protobuf::Arena*> arena ABSL_ATTRIBUTE_LIFETIME_BOUND,
    AnyReflection& reflection, const google::protobuf::Message& message,
    absl::Nonnull<const google::protobuf::DescriptorPool*> pool
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    absl::Nonnull<google::protobuf::MessageFactory*> factory
        ABSL_ATTRIBUTE_LIFETIME_BOUND);

// Unpacks the given instance of `google.protobuf.Any` if it is resolvable.
absl::StatusOr<Unique<google::protobuf::Message>> UnpackAnyIfResolveable(
    absl::Nullable<google::protobuf::Arena*> arena ABSL_ATTRIBUTE_LIFETIME_BOUND,
    AnyReflection& reflection, const google::protobuf::Message& message,
    absl::Nonnull<const google::protobuf::DescriptorPool*> pool
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    absl::Nonnull<google::protobuf::MessageFactory*> factory
        ABSL_ATTRIBUTE_LIFETIME_BOUND);

// Performs any necessary unwrapping of a well known message type. If no
// unwrapping is necessary, the resulting `Value` holds the alternative
// `absl::monostate`.
absl::StatusOr<Value> AdaptFromMessage(
    absl::Nullable<google::protobuf::Arena*> arena ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    absl::Nonnull<const google::protobuf::DescriptorPool*> pool
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    absl::Nonnull<google::protobuf::MessageFactory*> factory
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND);

class Reflection final {
 public:
  Reflection() = default;
  Reflection(const Reflection&) = default;
  Reflection& operator=(const Reflection&) = default;

  absl::Status Initialize(absl::Nonnull<const google::protobuf::DescriptorPool*> pool);

  // At the moment we only use this class for verifying well known types in
  // descriptor pools. We could eagerly initialize it and cache it somewhere to
  // make things faster.

  BoolValueReflection& BoolValue() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return bool_value_;
  }

  Int32ValueReflection& Int32Value() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return int32_value_;
  }

  Int64ValueReflection& Int64Value() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return int64_value_;
  }

  UInt32ValueReflection& UInt32Value() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return uint32_value_;
  }

  UInt64ValueReflection& UInt64Value() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return uint64_value_;
  }

  FloatValueReflection& FloatValue() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return float_value_;
  }

  DoubleValueReflection& DoubleValue() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return double_value_;
  }

  BytesValueReflection& BytesValue() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return bytes_value_;
  }

  StringValueReflection& StringValue() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return string_value_;
  }

  AnyReflection& Any() ABSL_ATTRIBUTE_LIFETIME_BOUND { return any_; }

  DurationReflection& Duration() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return duration_;
  }

  TimestampReflection& Timestamp() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return timestamp_;
  }

  ValueReflection& Value() ABSL_ATTRIBUTE_LIFETIME_BOUND { return value_; }

  ListValueReflection& ListValue() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return list_value_;
  }

  StructReflection& Struct() ABSL_ATTRIBUTE_LIFETIME_BOUND { return struct_; }

  FieldMaskReflection& FieldMask() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return field_mask_;
  }

  const BoolValueReflection& BoolValue() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return bool_value_;
  }

  const Int32ValueReflection& Int32Value() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return int32_value_;
  }

  const Int64ValueReflection& Int64Value() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return int64_value_;
  }

  const UInt32ValueReflection& UInt32Value() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return uint32_value_;
  }

  const UInt64ValueReflection& UInt64Value() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return uint64_value_;
  }

  const FloatValueReflection& FloatValue() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return float_value_;
  }

  const DoubleValueReflection& DoubleValue() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return double_value_;
  }

  const BytesValueReflection& BytesValue() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return bytes_value_;
  }

  const StringValueReflection& StringValue() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return string_value_;
  }

  const AnyReflection& Any() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return any_;
  }

  const DurationReflection& Duration() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return duration_;
  }

  const TimestampReflection& Timestamp() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return timestamp_;
  }

  const ValueReflection& Value() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return value_;
  }

  const ListValueReflection& ListValue() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return list_value_;
  }

  const StructReflection& Struct() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return struct_;
  }

  const FieldMaskReflection& FieldMask() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return field_mask_;
  }

 private:
  NullValueReflection& NullValue() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return null_value_;
  }

  const NullValueReflection& NullValue() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return null_value_;
  }

  NullValueReflection null_value_;
  BoolValueReflection bool_value_;
  Int32ValueReflection int32_value_;
  Int64ValueReflection int64_value_;
  UInt32ValueReflection uint32_value_;
  UInt64ValueReflection uint64_value_;
  FloatValueReflection float_value_;
  DoubleValueReflection double_value_;
  BytesValueReflection bytes_value_;
  StringValueReflection string_value_;
  AnyReflection any_;
  DurationReflection duration_;
  TimestampReflection timestamp_;
  ValueReflection value_;
  ListValueReflection list_value_;
  StructReflection struct_;
  FieldMaskReflection field_mask_;
};

}  // namespace cel::well_known_types

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_WELL_KNOWN_TYPES_H_
