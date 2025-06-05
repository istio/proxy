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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_UINT_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_UINT_VALUE_H_

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <type_traits>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "common/json.h"
#include "common/type.h"
#include "common/value_kind.h"

namespace cel {

class Value;
class ValueManager;
class UintValue;
class TypeManager;

// `UintValue` represents values of the primitive `uint` type.
class UintValue final {
 public:
  static constexpr ValueKind kKind = ValueKind::kUint;

  explicit UintValue(uint64_t value) noexcept : value_(value) {}

  template <typename T,
            typename = std::enable_if_t<std::conjunction_v<
                std::is_integral<T>, std::negation<std::is_same<T, bool>>,
                std::is_convertible<T, int64_t>>>>
  UintValue& operator=(T value) noexcept {
    value_ = value;
    return *this;
  }

  UintValue() = default;
  UintValue(const UintValue&) = default;
  UintValue(UintValue&&) = default;
  UintValue& operator=(const UintValue&) = default;
  UintValue& operator=(UintValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  absl::string_view GetTypeName() const { return UintType::kName; }

  std::string DebugString() const;

  // `SerializeTo` serializes this value and appends it to `value`.
  absl::Status SerializeTo(AnyToJsonConverter&, absl::Cord& value) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter&) const;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const { return NativeValue() == 0; }

  constexpr uint64_t NativeValue() const {
    return static_cast<uint64_t>(*this);
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr operator uint64_t() const noexcept { return value_; }

  friend void swap(UintValue& lhs, UintValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
  }

 private:
  uint64_t value_ = 0;
};

template <typename H>
H AbslHashValue(H state, UintValue value) {
  return H::combine(std::move(state), value.NativeValue());
}

constexpr bool operator==(UintValue lhs, UintValue rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

constexpr bool operator!=(UintValue lhs, UintValue rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, UintValue value) {
  return out << value.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_UINT_VALUE_H_
