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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_INT_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_INT_VALUE_H_

#include <cstdint>
#include <ostream>
#include <string>
#include <type_traits>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/json.h"
#include "common/type.h"
#include "common/value_kind.h"

namespace cel {

class Value;
class ValueManager;
class IntValue;
class TypeManager;

// `IntValue` represents values of the primitive `int` type.
class IntValue final {
 public:
  static constexpr ValueKind kKind = ValueKind::kInt;

  explicit IntValue(int64_t value) noexcept : value_(value) {}

  template <typename T,
            typename = std::enable_if_t<std::conjunction_v<
                std::is_integral<T>, std::negation<std::is_same<T, bool>>,
                std::is_convertible<T, int64_t>>>>
  IntValue& operator=(T value) noexcept {
    value_ = value;
    return *this;
  }

  IntValue() = default;
  IntValue(const IntValue&) = default;
  IntValue(IntValue&&) = default;
  IntValue& operator=(const IntValue&) = default;
  IntValue& operator=(IntValue&&) = default;

  ValueKind kind() const { return kKind; }

  absl::string_view GetTypeName() const { return IntType::kName; }

  std::string DebugString() const;

  // `SerializeTo` serializes this value and appends it to `value`.
  absl::Status SerializeTo(AnyToJsonConverter&, absl::Cord& value) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter&) const;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const { return NativeValue() == 0; }

  int64_t NativeValue() const { return static_cast<int64_t>(*this); }

  // NOLINTNEXTLINE(google-explicit-constructor)
  operator int64_t() const noexcept { return value_; }

  friend void swap(IntValue& lhs, IntValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
  }

 private:
  int64_t value_ = 0;
};

template <typename H>
H AbslHashValue(H state, IntValue value) {
  return H::combine(std::move(state), value.NativeValue());
}

inline bool operator==(IntValue lhs, IntValue rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

inline bool operator!=(IntValue lhs, IntValue rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, IntValue value) {
  return out << value.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_INT_VALUE_H_
