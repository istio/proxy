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

#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/value.h"
#include "internal/number.h"
#include "internal/serialize.h"
#include "internal/status_macros.h"

namespace cel {

namespace {

std::string DoubleDebugString(double value) {
  if (std::isfinite(value)) {
    if (std::floor(value) != value) {
      // The double is not representable as a whole number, so use
      // absl::StrCat which will add decimal places.
      return absl::StrCat(value);
    }
    // absl::StrCat historically would represent 0.0 as 0, and we want the
    // decimal places so ZetaSQL correctly assumes the type as double
    // instead of int64_t.
    std::string stringified = absl::StrCat(value);
    if (!absl::StrContains(stringified, '.')) {
      absl::StrAppend(&stringified, ".0");
    } else {
      // absl::StrCat has a decimal now? Use it directly.
    }
    return stringified;
  }
  if (std::isnan(value)) {
    return "nan";
  }
  if (std::signbit(value)) {
    return "-infinity";
  }
  return "+infinity";
}

}  // namespace

std::string DoubleValue::DebugString() const {
  return DoubleDebugString(NativeValue());
}

absl::Status DoubleValue::SerializeTo(AnyToJsonConverter&,
                                      absl::Cord& value) const {
  return internal::SerializeDoubleValue(NativeValue(), value);
}

absl::StatusOr<Json> DoubleValue::ConvertToJson(AnyToJsonConverter&) const {
  return NativeValue();
}

absl::Status DoubleValue::Equal(ValueManager&, const Value& other,
                                Value& result) const {
  if (auto other_value = As<DoubleValue>(other); other_value.has_value()) {
    result = BoolValue{NativeValue() == other_value->NativeValue()};
    return absl::OkStatus();
  }
  if (auto other_value = As<IntValue>(other); other_value.has_value()) {
    result = BoolValue{internal::Number::FromDouble(NativeValue()) ==
                       internal::Number::FromInt64(other_value->NativeValue())};
    return absl::OkStatus();
  }
  if (auto other_value = As<UintValue>(other); other_value.has_value()) {
    result =
        BoolValue{internal::Number::FromDouble(NativeValue()) ==
                  internal::Number::FromUint64(other_value->NativeValue())};
    return absl::OkStatus();
  }
  result = BoolValue{false};
  return absl::OkStatus();
}

absl::StatusOr<Value> DoubleValue::Equal(ValueManager& value_manager,
                                         const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

}  // namespace cel
