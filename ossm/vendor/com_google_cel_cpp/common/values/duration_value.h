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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_DURATION_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_DURATION_VALUE_H_

#include <ostream>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "common/json.h"
#include "common/type.h"
#include "common/value_kind.h"

namespace cel {

class Value;
class ValueManager;
class DurationValue;
class TypeManager;

// `DurationValue` represents values of the primitive `duration` type.
class DurationValue final {
 public:
  static constexpr ValueKind kKind = ValueKind::kDuration;

  explicit DurationValue(absl::Duration value) noexcept : value_(value) {}

  DurationValue& operator=(absl::Duration value) noexcept {
    value_ = value;
    return *this;
  }

  DurationValue() = default;
  DurationValue(const DurationValue&) = default;
  DurationValue(DurationValue&&) = default;
  DurationValue& operator=(const DurationValue&) = default;
  DurationValue& operator=(DurationValue&&) = default;

  ValueKind kind() const { return kKind; }

  absl::string_view GetTypeName() const { return DurationType::kName; }

  std::string DebugString() const;

  absl::Status SerializeTo(AnyToJsonConverter&, absl::Cord& value) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter&) const;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const { return NativeValue() == absl::ZeroDuration(); }

  absl::Duration NativeValue() const {
    return static_cast<absl::Duration>(*this);
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  operator absl::Duration() const noexcept { return value_; }

  friend void swap(DurationValue& lhs, DurationValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
  }

 private:
  absl::Duration value_ = absl::ZeroDuration();
};

inline bool operator==(DurationValue lhs, DurationValue rhs) {
  return static_cast<absl::Duration>(lhs) == static_cast<absl::Duration>(rhs);
}

inline bool operator!=(DurationValue lhs, DurationValue rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, DurationValue value) {
  return out << value.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_DURATION_VALUE_H_
