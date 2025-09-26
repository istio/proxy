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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_LEGACY_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_LEGACY_VALUE_H_

#include <cstdint>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "common/value.h"
#include "eval/public/cel_value.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"

namespace cel {

absl::Status ModernValue(google::protobuf::Arena* arena,
                         google::api::expr::runtime::CelValue legacy_value,
                         Value& result);
inline absl::StatusOr<Value> ModernValue(
    google::protobuf::Arena* arena, google::api::expr::runtime::CelValue legacy_value) {
  Value result;
  CEL_RETURN_IF_ERROR(ModernValue(arena, legacy_value, result));
  return result;
}

absl::StatusOr<google::api::expr::runtime::CelValue> LegacyValue(
    google::protobuf::Arena* arena, const Value& modern_value);

namespace common_internal {

// Convert a `cel::Value` to `google::api::expr::runtime::CelValue`, using
// `arena` to make memory allocations if necessary. `stable` indicates whether
// `cel::Value` is in a location where it will not be moved, so that inline
// string/bytes storage can be referenced.
google::api::expr::runtime::CelValue UnsafeLegacyValue(
    const Value& value, bool stable, google::protobuf::Arena* absl_nonnull arena);

}  // namespace common_internal

}  // namespace cel

namespace cel::interop_internal {

absl::StatusOr<Value> FromLegacyValue(
    google::protobuf::Arena* arena,
    const google::api::expr::runtime::CelValue& legacy_value,
    bool unchecked = false);

absl::StatusOr<google::api::expr::runtime::CelValue> ToLegacyValue(
    google::protobuf::Arena* arena, const Value& value, bool unchecked = false);

inline NullValue CreateNullValue() { return NullValue{}; }

inline BoolValue CreateBoolValue(bool value) { return BoolValue{value}; }

inline IntValue CreateIntValue(int64_t value) { return IntValue{value}; }

inline UintValue CreateUintValue(uint64_t value) { return UintValue{value}; }

inline DoubleValue CreateDoubleValue(double value) {
  return DoubleValue{value};
}

inline ListValue CreateLegacyListValue(
    const google::api::expr::runtime::CelList* value) {
  return common_internal::LegacyListValue(value);
}

inline MapValue CreateLegacyMapValue(
    const google::api::expr::runtime::CelMap* value) {
  return common_internal::LegacyMapValue(value);
}

inline Value CreateDurationValue(absl::Duration value, bool unchecked = false) {
  return DurationValue{value};
}

inline TimestampValue CreateTimestampValue(absl::Time value) {
  return TimestampValue{value};
}

Value LegacyValueToModernValueOrDie(
    google::protobuf::Arena* arena, const google::api::expr::runtime::CelValue& value,
    bool unchecked = false);
std::vector<Value> LegacyValueToModernValueOrDie(
    google::protobuf::Arena* arena,
    absl::Span<const google::api::expr::runtime::CelValue> values,
    bool unchecked = false);

google::api::expr::runtime::CelValue ModernValueToLegacyValueOrDie(
    google::protobuf::Arena* arena, const Value& value, bool unchecked = false);

TypeValue CreateTypeValueFromView(google::protobuf::Arena* arena,
                                  absl::string_view input);

}  // namespace cel::interop_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_LEGACY_VALUE_H_
