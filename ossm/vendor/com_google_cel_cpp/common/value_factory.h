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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUE_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUE_FACTORY_H_

#include <cstdint>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "common/json.h"
#include "common/type.h"
#include "common/type_factory.h"
#include "common/unknown.h"
#include "common/value.h"

namespace cel {

namespace common_internal {
class PiecewiseValueManager;
}

// `ValueFactory` is the preferred way for constructing values.
class ValueFactory : public virtual TypeFactory {
 public:
  // `CreateValueFromJson` constructs a new `Value` that is equivalent to the
  // JSON value `json`.
  ABSL_DEPRECATED("Avoid using Json/JsonArray/JsonObject")
  Value CreateValueFromJson(Json json);

  // `CreateListValueFromJsonArray` constructs a new `ListValue` that is
  // equivalent to the JSON array `JsonArray`.
  ABSL_DEPRECATED("Use ParsedJsonListValue instead")
  ListValue CreateListValueFromJsonArray(JsonArray json);

  // `CreateMapValueFromJsonObject` constructs a new `MapValue` that is
  // equivalent to the JSON object `JsonObject`.
  ABSL_DEPRECATED("Use ParsedJsonMapValue instead")
  MapValue CreateMapValueFromJsonObject(JsonObject json);

  // `GetDynListType` gets a view of the `ListType` type `list(dyn)`.
  ListValue GetZeroDynListValue();

  // `GetDynDynMapType` gets a view of the `MapType` type `map(dyn, dyn)`.
  MapValue GetZeroDynDynMapValue();

  // `GetDynDynMapType` gets a view of the `MapType` type `map(string, dyn)`.
  MapValue GetZeroStringDynMapValue();

  // `GetDynOptionalType` gets a view of the `OptionalType` type
  // `optional(dyn)`.
  OptionalValue GetZeroDynOptionalValue();

  NullValue GetNullValue() { return NullValue{}; }

  ErrorValue CreateErrorValue(absl::Status status) {
    return ErrorValue{std::move(status)};
  }

  BoolValue CreateBoolValue(bool value) { return BoolValue{value}; }

  IntValue CreateIntValue(int64_t value) { return IntValue{value}; }

  UintValue CreateUintValue(uint64_t value) { return UintValue{value}; }

  DoubleValue CreateDoubleValue(double value) { return DoubleValue{value}; }

  BytesValue GetBytesValue() { return BytesValue(); }

  absl::StatusOr<BytesValue> CreateBytesValue(const char* value) {
    return CreateBytesValue(absl::string_view(value));
  }

  absl::StatusOr<BytesValue> CreateBytesValue(absl::string_view value) {
    return CreateBytesValue(std::string(value));
  }

  absl::StatusOr<BytesValue> CreateBytesValue(std::string value);

  absl::StatusOr<BytesValue> CreateBytesValue(absl::Cord value) {
    return BytesValue(std::move(value));
  }

  template <typename Releaser>
  absl::StatusOr<BytesValue> CreateBytesValue(absl::string_view value,
                                              Releaser&& releaser) {
    return BytesValue(
        absl::MakeCordFromExternal(value, std::forward<Releaser>(releaser)));
  }

  StringValue GetStringValue() { return StringValue(); }

  absl::StatusOr<StringValue> CreateStringValue(const char* value) {
    return CreateStringValue(absl::string_view(value));
  }

  absl::StatusOr<StringValue> CreateStringValue(absl::string_view value) {
    return CreateStringValue(std::string(value));
  }

  absl::StatusOr<StringValue> CreateStringValue(std::string value);

  absl::StatusOr<StringValue> CreateStringValue(absl::Cord value);

  template <typename Releaser>
  absl::StatusOr<StringValue> CreateStringValue(absl::string_view value,
                                                Releaser&& releaser) {
    return StringValue(
        absl::MakeCordFromExternal(value, std::forward<Releaser>(releaser)));
  }

  StringValue CreateUncheckedStringValue(const char* value) {
    return CreateUncheckedStringValue(absl::string_view(value));
  }

  StringValue CreateUncheckedStringValue(absl::string_view value) {
    return CreateUncheckedStringValue(std::string(value));
  }

  StringValue CreateUncheckedStringValue(std::string value);

  StringValue CreateUncheckedStringValue(absl::Cord value) {
    return StringValue(std::move(value));
  }

  template <typename Releaser>
  StringValue CreateUncheckedStringValue(absl::string_view value,
                                         Releaser&& releaser) {
    return StringValue(
        absl::MakeCordFromExternal(value, std::forward<Releaser>(releaser)));
  }

  absl::StatusOr<DurationValue> CreateDurationValue(absl::Duration value);

  DurationValue CreateUncheckedDurationValue(absl::Duration value) {
    return DurationValue{value};
  }

  absl::StatusOr<TimestampValue> CreateTimestampValue(absl::Time value);

  TimestampValue CreateUncheckedTimestampValue(absl::Time value) {
    return TimestampValue{value};
  }

  TypeValue CreateTypeValue(const Type& type) { return TypeValue{Type(type)}; }

  UnknownValue CreateUnknownValue() {
    return CreateUnknownValue(AttributeSet(), FunctionResultSet());
  }

  UnknownValue CreateUnknownValue(AttributeSet attribute_set) {
    return CreateUnknownValue(std::move(attribute_set), FunctionResultSet());
  }

  UnknownValue CreateUnknownValue(FunctionResultSet function_result_set) {
    return CreateUnknownValue(AttributeSet(), std::move(function_result_set));
  }

  UnknownValue CreateUnknownValue(AttributeSet attribute_set,
                                  FunctionResultSet function_result_set) {
    return UnknownValue{
        Unknown{std::move(attribute_set), std::move(function_result_set)}};
  }

 protected:
  friend class common_internal::PiecewiseValueManager;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUE_FACTORY_H_
