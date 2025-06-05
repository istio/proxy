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

#include "common/type_reflector.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "common/any.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "common/values/piecewise_value_manager.h"
#include "common/values/thread_compatible_type_reflector.h"
#include "internal/deserialize.h"
#include "internal/overflow.h"
#include "internal/status_macros.h"

namespace cel {

namespace {

// Exception to `ValueBuilder` which also functions as a deserializer.
class WellKnownValueBuilder : public ValueBuilder {
 public:
  virtual absl::Status Deserialize(const absl::Cord& serialized_value) = 0;
};

class BoolValueBuilder final : public WellKnownValueBuilder {
 public:
  explicit BoolValueBuilder(const TypeReflector& type_reflector,
                            ValueFactory& value_factory) {}

  absl::Status SetFieldByName(absl::string_view name, Value value) override {
    if (name != "value") {
      return NoSuchFieldError(name).NativeValue();
    }
    return SetValue(std::move(value));
  }

  absl::Status SetFieldByNumber(int64_t number, Value value) override {
    if (number != 1) {
      return NoSuchFieldError(absl::StrCat(number)).NativeValue();
    }
    return SetValue(std::move(value));
  }

  Value Build() && override { return BoolValue(value_); }

  absl::Status Deserialize(const absl::Cord& serialized_value) override {
    CEL_ASSIGN_OR_RETURN(value_,
                         internal::DeserializeBoolValue(serialized_value));
    return absl::OkStatus();
  }

 private:
  absl::Status SetValue(Value value) {
    if (auto bool_value = As<BoolValue>(value); bool_value.has_value()) {
      value_ = bool_value->NativeValue();
      return absl::OkStatus();
    }
    return TypeConversionError(value.GetTypeName(), "bool").NativeValue();
  }

  bool value_ = false;
};

class Int32ValueBuilder final : public WellKnownValueBuilder {
 public:
  explicit Int32ValueBuilder(const TypeReflector& type_reflector,
                             ValueFactory& value_factory) {}

  absl::Status SetFieldByName(absl::string_view name, Value value) override {
    if (name != "value") {
      return NoSuchFieldError(name).NativeValue();
    }
    return SetValue(std::move(value));
  }

  absl::Status SetFieldByNumber(int64_t number, Value value) override {
    if (number != 1) {
      return NoSuchFieldError(absl::StrCat(number)).NativeValue();
    }
    return SetValue(std::move(value));
  }

  Value Build() && override { return IntValue(value_); }

  absl::Status Deserialize(const absl::Cord& serialized_value) override {
    CEL_ASSIGN_OR_RETURN(value_,
                         internal::DeserializeInt32Value(serialized_value));
    return absl::OkStatus();
  }

 private:
  absl::Status SetValue(Value value) {
    if (auto int_value = As<IntValue>(value); int_value.has_value()) {
      CEL_ASSIGN_OR_RETURN(
          value_, internal::CheckedInt64ToInt32(int_value->NativeValue()));
      return absl::OkStatus();
    }
    return TypeConversionError(value.GetTypeName(), "int").NativeValue();
  }

  int64_t value_ = 0;
};

class Int64ValueBuilder final : public WellKnownValueBuilder {
 public:
  explicit Int64ValueBuilder(const TypeReflector& type_reflector,
                             ValueFactory& value_factory) {}

  absl::Status SetFieldByName(absl::string_view name, Value value) override {
    if (name != "value") {
      return NoSuchFieldError(name).NativeValue();
    }
    return SetValue(std::move(value));
  }

  absl::Status SetFieldByNumber(int64_t number, Value value) override {
    if (number != 1) {
      return NoSuchFieldError(absl::StrCat(number)).NativeValue();
    }
    return SetValue(std::move(value));
  }

  Value Build() && override { return IntValue(value_); }

  absl::Status Deserialize(const absl::Cord& serialized_value) override {
    CEL_ASSIGN_OR_RETURN(value_,
                         internal::DeserializeInt64Value(serialized_value));
    return absl::OkStatus();
  }

 private:
  absl::Status SetValue(Value value) {
    if (auto int_value = As<IntValue>(value); int_value.has_value()) {
      value_ = int_value->NativeValue();
      return absl::OkStatus();
    }
    return TypeConversionError(value.GetTypeName(), "int").NativeValue();
  }

  int64_t value_ = 0;
};

class UInt32ValueBuilder final : public WellKnownValueBuilder {
 public:
  explicit UInt32ValueBuilder(const TypeReflector& type_reflector,
                              ValueFactory& value_factory) {}

  absl::Status SetFieldByName(absl::string_view name, Value value) override {
    if (name != "value") {
      return NoSuchFieldError(name).NativeValue();
    }
    return SetValue(std::move(value));
  }

  absl::Status SetFieldByNumber(int64_t number, Value value) override {
    if (number != 1) {
      return NoSuchFieldError(absl::StrCat(number)).NativeValue();
    }
    return SetValue(std::move(value));
  }

  Value Build() && override { return UintValue(value_); }

  absl::Status Deserialize(const absl::Cord& serialized_value) override {
    CEL_ASSIGN_OR_RETURN(value_,
                         internal::DeserializeUInt32Value(serialized_value));
    return absl::OkStatus();
  }

 private:
  absl::Status SetValue(Value value) {
    if (auto uint_value = As<UintValue>(value); uint_value.has_value()) {
      CEL_ASSIGN_OR_RETURN(
          value_, internal::CheckedUint64ToUint32(uint_value->NativeValue()));
      return absl::OkStatus();
    }
    return TypeConversionError(value.GetTypeName(), "uint").NativeValue();
  }

  uint64_t value_ = 0;
};

class UInt64ValueBuilder final : public WellKnownValueBuilder {
 public:
  explicit UInt64ValueBuilder(const TypeReflector& type_reflector,
                              ValueFactory& value_factory) {}

  absl::Status SetFieldByName(absl::string_view name, Value value) override {
    if (name != "value") {
      return NoSuchFieldError(name).NativeValue();
    }
    return SetValue(std::move(value));
  }

  absl::Status SetFieldByNumber(int64_t number, Value value) override {
    if (number != 1) {
      return NoSuchFieldError(absl::StrCat(number)).NativeValue();
    }
    return SetValue(std::move(value));
  }

  Value Build() && override { return UintValue(value_); }

  absl::Status Deserialize(const absl::Cord& serialized_value) override {
    CEL_ASSIGN_OR_RETURN(value_,
                         internal::DeserializeUInt64Value(serialized_value));
    return absl::OkStatus();
  }

 private:
  absl::Status SetValue(Value value) {
    if (auto uint_value = As<UintValue>(value); uint_value.has_value()) {
      value_ = uint_value->NativeValue();
      return absl::OkStatus();
    }
    return TypeConversionError(value.GetTypeName(), "uint").NativeValue();
  }

  uint64_t value_ = 0;
};

class FloatValueBuilder final : public WellKnownValueBuilder {
 public:
  explicit FloatValueBuilder(const TypeReflector& type_reflector,
                             ValueFactory& value_factory) {}

  absl::Status SetFieldByName(absl::string_view name, Value value) override {
    if (name != "value") {
      return NoSuchFieldError(name).NativeValue();
    }
    return SetValue(std::move(value));
  }

  absl::Status SetFieldByNumber(int64_t number, Value value) override {
    if (number != 1) {
      return NoSuchFieldError(absl::StrCat(number)).NativeValue();
    }
    return SetValue(std::move(value));
  }

  Value Build() && override { return DoubleValue(value_); }

  absl::Status Deserialize(const absl::Cord& serialized_value) override {
    CEL_ASSIGN_OR_RETURN(value_,
                         internal::DeserializeFloatValue(serialized_value));
    return absl::OkStatus();
  }

 private:
  absl::Status SetValue(Value value) {
    if (auto double_value = As<DoubleValue>(value); double_value.has_value()) {
      // Ensure we truncate to `float`.
      value_ = static_cast<float>(double_value->NativeValue());
      return absl::OkStatus();
    }
    return TypeConversionError(value.GetTypeName(), "double").NativeValue();
  }

  double value_ = 0;
};

class DoubleValueBuilder final : public WellKnownValueBuilder {
 public:
  explicit DoubleValueBuilder(const TypeReflector& type_reflector,
                              ValueFactory& value_factory) {}

  absl::Status SetFieldByName(absl::string_view name, Value value) override {
    if (name != "value") {
      return NoSuchFieldError(name).NativeValue();
    }
    return SetValue(std::move(value));
  }

  absl::Status SetFieldByNumber(int64_t number, Value value) override {
    if (number != 1) {
      return NoSuchFieldError(absl::StrCat(number)).NativeValue();
    }
    return SetValue(std::move(value));
  }

  Value Build() && override { return DoubleValue(value_); }

  absl::Status Deserialize(const absl::Cord& serialized_value) override {
    CEL_ASSIGN_OR_RETURN(value_,
                         internal::DeserializeDoubleValue(serialized_value));
    return absl::OkStatus();
  }

 private:
  absl::Status SetValue(Value value) {
    if (auto double_value = As<DoubleValue>(value); double_value.has_value()) {
      value_ = double_value->NativeValue();
      return absl::OkStatus();
    }
    return TypeConversionError(value.GetTypeName(), "double").NativeValue();
  }

  double value_ = 0;
};

class StringValueBuilder final : public WellKnownValueBuilder {
 public:
  explicit StringValueBuilder(const TypeReflector& type_reflector,
                              ValueFactory& value_factory) {}

  absl::Status SetFieldByName(absl::string_view name, Value value) override {
    if (name != "value") {
      return NoSuchFieldError(name).NativeValue();
    }
    return SetValue(std::move(value));
  }

  absl::Status SetFieldByNumber(int64_t number, Value value) override {
    if (number != 1) {
      return NoSuchFieldError(absl::StrCat(number)).NativeValue();
    }
    return SetValue(std::move(value));
  }

  Value Build() && override { return StringValue(std::move(value_)); }

  absl::Status Deserialize(const absl::Cord& serialized_value) override {
    CEL_ASSIGN_OR_RETURN(value_,
                         internal::DeserializeStringValue(serialized_value));
    return absl::OkStatus();
  }

 private:
  absl::Status SetValue(Value value) {
    if (auto string_value = As<StringValue>(value); string_value.has_value()) {
      value_ = string_value->NativeCord();
      return absl::OkStatus();
    }
    return TypeConversionError(value.GetTypeName(), "string").NativeValue();
  }

  absl::Cord value_;
};

class BytesValueBuilder final : public WellKnownValueBuilder {
 public:
  explicit BytesValueBuilder(const TypeReflector& type_reflector,
                             ValueFactory& value_factory) {}

  absl::Status SetFieldByName(absl::string_view name, Value value) override {
    if (name != "value") {
      return NoSuchFieldError(name).NativeValue();
    }
    return SetValue(std::move(value));
  }

  absl::Status SetFieldByNumber(int64_t number, Value value) override {
    if (number != 1) {
      return NoSuchFieldError(absl::StrCat(number)).NativeValue();
    }
    return SetValue(std::move(value));
  }

  Value Build() && override { return BytesValue(std::move(value_)); }

  absl::Status Deserialize(const absl::Cord& serialized_value) override {
    CEL_ASSIGN_OR_RETURN(value_,
                         internal::DeserializeBytesValue(serialized_value));
    return absl::OkStatus();
  }

 private:
  absl::Status SetValue(Value value) {
    if (auto bytes_value = As<BytesValue>(value); bytes_value.has_value()) {
      value_ = bytes_value->NativeCord();
      return absl::OkStatus();
    }
    return TypeConversionError(value.GetTypeName(), "bytes").NativeValue();
  }

  absl::Cord value_;
};

class DurationValueBuilder final : public WellKnownValueBuilder {
 public:
  explicit DurationValueBuilder(const TypeReflector& type_reflector,
                                ValueFactory& value_factory) {}

  absl::Status SetFieldByName(absl::string_view name, Value value) override {
    if (name == "seconds") {
      return SetSeconds(std::move(value));
    }
    if (name == "nanos") {
      return SetNanos(std::move(value));
    }
    return NoSuchFieldError(name).NativeValue();
  }

  absl::Status SetFieldByNumber(int64_t number, Value value) override {
    if (number == 1) {
      return SetSeconds(std::move(value));
    }
    if (number == 2) {
      return SetNanos(std::move(value));
    }
    return NoSuchFieldError(absl::StrCat(number)).NativeValue();
  }

  Value Build() && override {
    return DurationValue(absl::Seconds(seconds_) + absl::Nanoseconds(nanos_));
  }

  absl::Status Deserialize(const absl::Cord& serialized_value) override {
    CEL_ASSIGN_OR_RETURN(auto value,
                         internal::DeserializeDuration(serialized_value));
    seconds_ = absl::IDivDuration(value, absl::Seconds(1), &value);
    nanos_ = static_cast<int32_t>(
        absl::IDivDuration(value, absl::Nanoseconds(1), &value));
    return absl::OkStatus();
  }

 private:
  absl::Status SetSeconds(Value value) {
    if (auto int_value = As<IntValue>(value); int_value.has_value()) {
      seconds_ = int_value->NativeValue();
      return absl::OkStatus();
    }
    return TypeConversionError(value.GetTypeName(), "int").NativeValue();
  }

  absl::Status SetNanos(Value value) {
    if (auto int_value = As<IntValue>(value); int_value.has_value()) {
      CEL_ASSIGN_OR_RETURN(
          nanos_, internal::CheckedInt64ToInt32(int_value->NativeValue()));
      return absl::OkStatus();
    }
    return TypeConversionError(value.GetTypeName(), "int").NativeValue();
  }

  int64_t seconds_ = 0;
  int32_t nanos_ = 0;
};

class TimestampValueBuilder final : public WellKnownValueBuilder {
 public:
  explicit TimestampValueBuilder(const TypeReflector& type_reflector,
                                 ValueFactory& value_factory) {}

  absl::Status SetFieldByName(absl::string_view name, Value value) override {
    if (name == "seconds") {
      return SetSeconds(std::move(value));
    }
    if (name == "nanos") {
      return SetNanos(std::move(value));
    }
    return NoSuchFieldError(name).NativeValue();
  }

  absl::Status SetFieldByNumber(int64_t number, Value value) override {
    if (number == 1) {
      return SetSeconds(std::move(value));
    }
    if (number == 2) {
      return SetNanos(std::move(value));
    }
    return NoSuchFieldError(absl::StrCat(number)).NativeValue();
  }

  Value Build() && override {
    return TimestampValue(absl::UnixEpoch() + absl::Seconds(seconds_) +
                          absl::Nanoseconds(nanos_));
  }

  absl::Status Deserialize(const absl::Cord& serialized_value) override {
    CEL_ASSIGN_OR_RETURN(auto value,
                         internal::DeserializeTimestamp(serialized_value));
    auto duration = value - absl::UnixEpoch();
    seconds_ = absl::IDivDuration(duration, absl::Seconds(1), &duration);
    nanos_ = static_cast<int32_t>(
        absl::IDivDuration(duration, absl::Nanoseconds(1), &duration));
    return absl::OkStatus();
  }

 private:
  absl::Status SetSeconds(Value value) {
    if (auto int_value = As<IntValue>(value); int_value.has_value()) {
      seconds_ = int_value->NativeValue();
      return absl::OkStatus();
    }
    return TypeConversionError(value.GetTypeName(), "int").NativeValue();
  }

  absl::Status SetNanos(Value value) {
    if (auto int_value = As<IntValue>(value); int_value.has_value()) {
      CEL_ASSIGN_OR_RETURN(
          nanos_, internal::CheckedInt64ToInt32(int_value->NativeValue()));
      return absl::OkStatus();
    }
    return TypeConversionError(value.GetTypeName(), "int").NativeValue();
  }

  int64_t seconds_ = 0;
  int32_t nanos_ = 0;
};

class JsonValueBuilder final : public WellKnownValueBuilder {
 public:
  explicit JsonValueBuilder(const TypeReflector& type_reflector,
                            ValueFactory& value_factory)
      : type_reflector_(type_reflector), value_factory_(value_factory) {}

  absl::Status SetFieldByName(absl::string_view name, Value value) override {
    if (name == "null_value") {
      return SetNullValue();
    }
    if (name == "number_value") {
      return SetNumberValue(std::move(value));
    }
    if (name == "string_value") {
      return SetStringValue(std::move(value));
    }
    if (name == "bool_value") {
      return SetBoolValue(std::move(value));
    }
    if (name == "struct_value") {
      return SetStructValue(std::move(value));
    }
    if (name == "list_value") {
      return SetListValue(std::move(value));
    }
    return NoSuchFieldError(name).NativeValue();
  }

  absl::Status SetFieldByNumber(int64_t number, Value value) override {
    switch (number) {
      case 1:
        return SetNullValue();
      case 2:
        return SetNumberValue(std::move(value));
      case 3:
        return SetStringValue(std::move(value));
      case 4:
        return SetBoolValue(std::move(value));
      case 5:
        return SetStructValue(std::move(value));
      case 6:
        return SetListValue(std::move(value));
      default:
        return NoSuchFieldError(absl::StrCat(number)).NativeValue();
    }
  }

  Value Build() && override {
    return value_factory_.CreateValueFromJson(std::move(json_));
  }

  absl::Status Deserialize(const absl::Cord& serialized_value) override {
    CEL_ASSIGN_OR_RETURN(json_, internal::DeserializeValue(serialized_value));
    return absl::OkStatus();
  }

 private:
  absl::Status SetNullValue() {
    json_ = kJsonNull;
    return absl::OkStatus();
  }

  absl::Status SetNumberValue(Value value) {
    if (auto double_value = As<DoubleValue>(value); double_value.has_value()) {
      json_ = double_value->NativeValue();
      return absl::OkStatus();
    }
    return TypeConversionError(value.GetTypeName(), "double").NativeValue();
  }

  absl::Status SetStringValue(Value value) {
    if (auto string_value = As<StringValue>(value); string_value.has_value()) {
      json_ = string_value->NativeCord();
      return absl::OkStatus();
    }
    return TypeConversionError(value.GetTypeName(), "string").NativeValue();
  }

  absl::Status SetBoolValue(Value value) {
    if (auto bool_value = As<BoolValue>(value); bool_value.has_value()) {
      json_ = bool_value->NativeValue();
      return absl::OkStatus();
    }
    return TypeConversionError(value.GetTypeName(), "bool").NativeValue();
  }

  absl::Status SetStructValue(Value value) {
    if (auto map_value = As<MapValue>(value); map_value.has_value()) {
      common_internal::PiecewiseValueManager value_manager(type_reflector_,
                                                           value_factory_);
      CEL_ASSIGN_OR_RETURN(json_, map_value->ConvertToJson(value_manager));
      return absl::OkStatus();
    }
    if (auto struct_value = As<StructValue>(value); struct_value.has_value()) {
      common_internal::PiecewiseValueManager value_manager(type_reflector_,
                                                           value_factory_);
      CEL_ASSIGN_OR_RETURN(json_, struct_value->ConvertToJson(value_manager));
      return absl::OkStatus();
    }
    return TypeConversionError(value.GetTypeName(), "google.protobuf.Struct")
        .NativeValue();
  }

  absl::Status SetListValue(Value value) {
    if (auto list_value = As<ListValue>(value); list_value.has_value()) {
      common_internal::PiecewiseValueManager value_manager(type_reflector_,
                                                           value_factory_);
      CEL_ASSIGN_OR_RETURN(json_, list_value->ConvertToJson(value_manager));
      return absl::OkStatus();
    }
    return TypeConversionError(value.GetTypeName(), "google.protobuf.ListValue")
        .NativeValue();
  }

  const TypeReflector& type_reflector_;
  ValueFactory& value_factory_;
  Json json_;
};

class JsonArrayValueBuilder final : public WellKnownValueBuilder {
 public:
  explicit JsonArrayValueBuilder(const TypeReflector& type_reflector,
                                 ValueFactory& value_factory)
      : type_reflector_(type_reflector), value_factory_(value_factory) {}

  absl::Status SetFieldByName(absl::string_view name, Value value) override {
    if (name == "values") {
      return SetValues(std::move(value));
    }
    return NoSuchFieldError(name).NativeValue();
  }

  absl::Status SetFieldByNumber(int64_t number, Value value) override {
    if (number == 1) {
      return SetValues(std::move(value));
    }
    return NoSuchFieldError(absl::StrCat(number)).NativeValue();
  }

  Value Build() && override {
    return value_factory_.CreateListValueFromJsonArray(std::move(array_));
  }

  absl::Status Deserialize(const absl::Cord& serialized_value) override {
    CEL_ASSIGN_OR_RETURN(array_,
                         internal::DeserializeListValue(serialized_value));
    return absl::OkStatus();
  }

 private:
  absl::Status SetValues(Value value) {
    if (auto list_value = As<ListValue>(value); list_value.has_value()) {
      common_internal::PiecewiseValueManager value_manager(type_reflector_,
                                                           value_factory_);
      CEL_ASSIGN_OR_RETURN(array_,
                           list_value->ConvertToJsonArray(value_manager));
      return absl::OkStatus();
    }
    return TypeConversionError(value.GetTypeName(), "list(dyn)").NativeValue();
  }

  const TypeReflector& type_reflector_;
  ValueFactory& value_factory_;
  JsonArray array_;
};

class JsonObjectValueBuilder final : public WellKnownValueBuilder {
 public:
  explicit JsonObjectValueBuilder(const TypeReflector& type_reflector,
                                  ValueFactory& value_factory)
      : type_reflector_(type_reflector), value_factory_(value_factory) {}

  absl::Status SetFieldByName(absl::string_view name, Value value) override {
    if (name == "fields") {
      return SetFields(std::move(value));
    }
    return NoSuchFieldError(name).NativeValue();
  }

  absl::Status SetFieldByNumber(int64_t number, Value value) override {
    if (number == 1) {
      return SetFields(std::move(value));
    }
    return NoSuchFieldError(absl::StrCat(number)).NativeValue();
  }

  Value Build() && override {
    return value_factory_.CreateMapValueFromJsonObject(std::move(object_));
  }

  absl::Status Deserialize(const absl::Cord& serialized_value) override {
    CEL_ASSIGN_OR_RETURN(object_,
                         internal::DeserializeStruct(serialized_value));
    return absl::OkStatus();
  }

 private:
  absl::Status SetFields(Value value) {
    if (auto map_value = As<MapValue>(value); map_value.has_value()) {
      common_internal::PiecewiseValueManager value_manager(type_reflector_,
                                                           value_factory_);
      CEL_ASSIGN_OR_RETURN(object_,
                           map_value->ConvertToJsonObject(value_manager));
      return absl::OkStatus();
    }
    if (auto struct_value = As<StructValue>(value); struct_value.has_value()) {
      common_internal::PiecewiseValueManager value_manager(type_reflector_,
                                                           value_factory_);
      CEL_ASSIGN_OR_RETURN(auto json_value,
                           struct_value->ConvertToJson(value_manager));
      if (absl::holds_alternative<JsonObject>(json_value)) {
        object_ = absl::get<JsonObject>(std::move(json_value));
        return absl::OkStatus();
      }
    }
    return TypeConversionError(value.GetTypeName(), "map(string, dyn)")
        .NativeValue();
  }

  const TypeReflector& type_reflector_;
  ValueFactory& value_factory_;
  JsonObject object_;
};

class AnyValueBuilder final : public WellKnownValueBuilder {
 public:
  explicit AnyValueBuilder(const TypeReflector& type_reflector,
                           ValueFactory& value_factory)
      : type_reflector_(type_reflector), value_factory_(value_factory) {}

  absl::Status SetFieldByName(absl::string_view name, Value value) override {
    if (name == "type_url") {
      return SetTypeUrl(std::move(value));
    }
    if (name == "value") {
      return SetValue(std::move(value));
    }
    return NoSuchFieldError(name).NativeValue();
  }

  absl::Status SetFieldByNumber(int64_t number, Value value) override {
    if (number == 1) {
      return SetTypeUrl(std::move(value));
    }
    if (number == 2) {
      return SetValue(std::move(value));
    }
    return NoSuchFieldError(absl::StrCat(number)).NativeValue();
  }

  Value Build() && override {
    auto status_or_value =
        type_reflector_.DeserializeValue(value_factory_, type_url_, value_);
    if (!status_or_value.ok()) {
      return ErrorValue(std::move(status_or_value).status());
    }
    if (!(*status_or_value).has_value()) {
      return NoSuchTypeError(type_url_);
    }
    return std::move(*std::move(*status_or_value));
  }

  absl::Status Deserialize(const absl::Cord& serialized_value) override {
    CEL_ASSIGN_OR_RETURN(auto any, internal::DeserializeAny(serialized_value));
    type_url_ = any.type_url();
    value_ = GetAnyValueAsCord(any);
    return absl::OkStatus();
  }

 private:
  absl::Status SetTypeUrl(Value value) {
    if (auto string_value = As<StringValue>(value); string_value.has_value()) {
      type_url_ = string_value->NativeString();
      return absl::OkStatus();
    }
    return TypeConversionError(value.GetTypeName(), "string").NativeValue();
  }

  absl::Status SetValue(Value value) {
    if (auto bytes_value = As<BytesValue>(value); bytes_value.has_value()) {
      value_ = bytes_value->NativeCord();
      return absl::OkStatus();
    }
    return TypeConversionError(value.GetTypeName(), "bytes").NativeValue();
  }

  const TypeReflector& type_reflector_;
  ValueFactory& value_factory_;
  std::string type_url_;
  absl::Cord value_;
};

using WellKnownValueBuilderProvider =
    std::unique_ptr<WellKnownValueBuilder> (*)(MemoryManagerRef,
                                               const TypeReflector&,
                                               ValueFactory&);

template <typename T>
std::unique_ptr<WellKnownValueBuilder> WellKnownValueBuilderProviderFor(
    MemoryManagerRef memory_manager, const TypeReflector& type_reflector,
    ValueFactory& value_factory) {
  return std::make_unique<T>(type_reflector, value_factory);
}

using WellKnownValueBuilderMap =
    absl::flat_hash_map<absl::string_view, WellKnownValueBuilderProvider>;

const WellKnownValueBuilderMap& GetWellKnownValueBuilderMap() {
  static const WellKnownValueBuilderMap* builders =
      []() -> WellKnownValueBuilderMap* {
    WellKnownValueBuilderMap* builders = new WellKnownValueBuilderMap();
    builders->insert_or_assign(
        "google.protobuf.BoolValue",
        &WellKnownValueBuilderProviderFor<BoolValueBuilder>);
    builders->insert_or_assign(
        "google.protobuf.Int32Value",
        &WellKnownValueBuilderProviderFor<Int32ValueBuilder>);
    builders->insert_or_assign(
        "google.protobuf.Int64Value",
        &WellKnownValueBuilderProviderFor<Int64ValueBuilder>);
    builders->insert_or_assign(
        "google.protobuf.UInt32Value",
        &WellKnownValueBuilderProviderFor<UInt32ValueBuilder>);
    builders->insert_or_assign(
        "google.protobuf.UInt64Value",
        &WellKnownValueBuilderProviderFor<UInt64ValueBuilder>);
    builders->insert_or_assign(
        "google.protobuf.FloatValue",
        &WellKnownValueBuilderProviderFor<FloatValueBuilder>);
    builders->insert_or_assign(
        "google.protobuf.DoubleValue",
        &WellKnownValueBuilderProviderFor<DoubleValueBuilder>);
    builders->insert_or_assign(
        "google.protobuf.StringValue",
        &WellKnownValueBuilderProviderFor<StringValueBuilder>);
    builders->insert_or_assign(
        "google.protobuf.BytesValue",
        &WellKnownValueBuilderProviderFor<BytesValueBuilder>);
    builders->insert_or_assign(
        "google.protobuf.Duration",
        &WellKnownValueBuilderProviderFor<DurationValueBuilder>);
    builders->insert_or_assign(
        "google.protobuf.Timestamp",
        &WellKnownValueBuilderProviderFor<TimestampValueBuilder>);
    builders->insert_or_assign(
        "google.protobuf.Value",
        &WellKnownValueBuilderProviderFor<JsonValueBuilder>);
    builders->insert_or_assign(
        "google.protobuf.ListValue",
        &WellKnownValueBuilderProviderFor<JsonArrayValueBuilder>);
    builders->insert_or_assign(
        "google.protobuf.Struct",
        &WellKnownValueBuilderProviderFor<JsonObjectValueBuilder>);
    builders->insert_or_assign(
        "google.protobuf.Any",
        &WellKnownValueBuilderProviderFor<AnyValueBuilder>);
    return builders;
  }();
  return *builders;
}

class ValueBuilderForStruct final : public ValueBuilder {
 public:
  explicit ValueBuilderForStruct(StructValueBuilderPtr delegate)
      : delegate_(std::move(delegate)) {}

  absl::Status SetFieldByName(absl::string_view name, Value value) override {
    return delegate_->SetFieldByName(name, std::move(value));
  }

  absl::Status SetFieldByNumber(int64_t number, Value value) override {
    return delegate_->SetFieldByNumber(number, std::move(value));
  }

  Value Build() && override {
    auto status_or_value = std::move(*delegate_).Build();
    if (!status_or_value.ok()) {
      return ErrorValue(status_or_value.status());
    }
    return std::move(status_or_value).value();
  }

 private:
  StructValueBuilderPtr delegate_;
};

}  // namespace

absl::StatusOr<absl::Nullable<ValueBuilderPtr>> TypeReflector::NewValueBuilder(
    ValueFactory& value_factory, absl::string_view name) const {
  const auto& well_known_value_builders = GetWellKnownValueBuilderMap();
  if (auto well_known_value_builder = well_known_value_builders.find(name);
      well_known_value_builder != well_known_value_builders.end()) {
    return (*well_known_value_builder->second)(value_factory.GetMemoryManager(),
                                               *this, value_factory);
  }
  CEL_ASSIGN_OR_RETURN(
      auto maybe_builder,
      NewStructValueBuilder(value_factory,
                            common_internal::MakeBasicStructType(name)));
  if (maybe_builder != nullptr) {
    return std::make_unique<ValueBuilderForStruct>(std::move(maybe_builder));
  }
  return nullptr;
}

absl::StatusOr<absl::optional<Value>> TypeReflector::DeserializeValue(
    ValueFactory& value_factory, absl::string_view type_url,
    const absl::Cord& value) const {
  if (absl::StartsWith(type_url, kTypeGoogleApisComPrefix)) {
    const auto& well_known_value_builders = GetWellKnownValueBuilderMap();
    if (auto well_known_value_builder = well_known_value_builders.find(
            absl::StripPrefix(type_url, kTypeGoogleApisComPrefix));
        well_known_value_builder != well_known_value_builders.end()) {
      auto deserializer = (*well_known_value_builder->second)(
          value_factory.GetMemoryManager(), *this, value_factory);
      CEL_RETURN_IF_ERROR(deserializer->Deserialize(value));
      return std::move(*deserializer).Build();
    }
  }
  return DeserializeValueImpl(value_factory, type_url, value);
}

absl::StatusOr<absl::optional<Value>> TypeReflector::DeserializeValueImpl(
    ValueFactory&, absl::string_view, const absl::Cord&) const {
  return absl::nullopt;
}

absl::StatusOr<absl::Nullable<StructValueBuilderPtr>>
TypeReflector::NewStructValueBuilder(ValueFactory&, const StructType&) const {
  return nullptr;
}

absl::StatusOr<bool> TypeReflector::FindValue(ValueFactory&, absl::string_view,
                                              Value&) const {
  return false;
}

TypeReflector& TypeReflector::LegacyBuiltin() {
  static absl::NoDestructor<common_internal::LegacyTypeReflector> instance;
  return *instance;
}

TypeReflector& TypeReflector::ModernBuiltin() {
  static absl::NoDestructor<TypeReflector> instance;
  return *instance;
}

Shared<TypeReflector> NewThreadCompatibleTypeReflector(
    MemoryManagerRef memory_manager) {
  return memory_manager
      .MakeShared<common_internal::ThreadCompatibleTypeReflector>();
}

}  // namespace cel
