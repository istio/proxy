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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_BOOL_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_BOOL_VALUE_H_

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
class BoolValue;
class TypeManager;

// `BoolValue` represents values of the primitive `bool` type.
class BoolValue final {
 public:
  static constexpr ValueKind kKind = ValueKind::kBool;

  BoolValue() = default;
  BoolValue(const BoolValue&) = default;
  BoolValue(BoolValue&&) = default;
  BoolValue& operator=(const BoolValue&) = default;
  BoolValue& operator=(BoolValue&&) = default;

  explicit BoolValue(bool value) noexcept : value_(value) {}

  template <typename T, typename = std::enable_if_t<std::is_same_v<T, bool>>>
  BoolValue& operator=(T value) noexcept {
    value_ = value;
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  operator bool() const noexcept { return value_; }

  ValueKind kind() const { return kKind; }

  absl::string_view GetTypeName() const { return BoolType::kName; }

  std::string DebugString() const;

  // `SerializeTo` serializes this value and appends it to `value`.
  absl::Status SerializeTo(AnyToJsonConverter&, absl::Cord& value) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter&) const;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const { return NativeValue() == false; }

  bool NativeValue() const { return static_cast<bool>(*this); }

  friend void swap(BoolValue& lhs, BoolValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
  }

 private:
  bool value_ = false;
};

template <typename H>
H AbslHashValue(H state, BoolValue value) {
  return H::combine(std::move(state), value.NativeValue());
}

inline std::ostream& operator<<(std::ostream& out, BoolValue value) {
  return out << value.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_BOOL_VALUE_H_
