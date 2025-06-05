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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_TYPE_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_TYPE_VALUE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/json.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/value_kind.h"

namespace cel {

class Value;
class ValueManager;
class TypeValue;
class TypeManager;

// `TypeValue` represents values of the primitive `type` type.
class TypeValue final {
 public:
  static constexpr ValueKind kKind = ValueKind::kType;

  // NOLINTNEXTLINE(google-explicit-constructor)
  TypeValue(Type value) noexcept : value_(std::move(value)) {}

  TypeValue() = default;
  TypeValue(const TypeValue&) = default;
  TypeValue(TypeValue&&) = default;
  TypeValue& operator=(const TypeValue&) = default;
  TypeValue& operator=(TypeValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  absl::string_view GetTypeName() const { return TypeType::kName; }

  std::string DebugString() const { return value_.DebugString(); }

  // `SerializeTo` always returns `FAILED_PRECONDITION` as `TypeValue` is not
  // serializable.
  absl::Status SerializeTo(AnyToJsonConverter&, absl::Cord& value) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter&) const;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const { return false; }

  const Type& NativeValue() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return value_;
  }

  void swap(TypeValue& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

  absl::string_view name() const { return NativeValue().name(); }

 private:
  friend struct NativeTypeTraits<TypeValue>;

  Type value_;
};

inline void swap(TypeValue& lhs, TypeValue& rhs) noexcept { lhs.swap(rhs); }

inline std::ostream& operator<<(std::ostream& out, const TypeValue& value) {
  return out << value.DebugString();
}

template <>
struct NativeTypeTraits<TypeValue> final {
  static bool SkipDestructor(const TypeValue& value) {
    // Type is trivial.
    return true;
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_TYPE_VALUE_H_
