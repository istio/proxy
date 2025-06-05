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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_NULL_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_NULL_VALUE_H_

#include <cstddef>
#include <ostream>
#include <string>

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
class NullValue;
class TypeManager;

// `NullValue` represents values of the primitive `duration` type.

class NullValue final {
 public:
  static constexpr ValueKind kKind = ValueKind::kNull;

  NullValue() = default;
  NullValue(const NullValue&) = default;
  NullValue(NullValue&&) = default;
  NullValue& operator=(const NullValue&) = default;
  NullValue& operator=(NullValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  absl::string_view GetTypeName() const { return NullType::kName; }

  std::string DebugString() const { return "null"; }

  absl::Status SerializeTo(AnyToJsonConverter&, absl::Cord& value) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter&) const {
    return kJsonNull;
  }

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const { return true; }

  friend void swap(NullValue&, NullValue&) noexcept {}
};

inline bool operator==(NullValue, NullValue) { return true; }

inline bool operator!=(NullValue lhs, NullValue rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, const NullValue& value) {
  return out << value.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_NULL_VALUE_H_
