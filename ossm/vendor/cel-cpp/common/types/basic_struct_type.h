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

// IWYU pragma: private, include "common/type.h"
// IWYU pragma: friend "common/types/struct_type.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_BASIC_STRUCT_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_BASIC_STRUCT_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/log/absl_check.h"
#include "absl/strings/string_view.h"
#include "common/type_kind.h"

namespace cel {

class Type;
class TypeParameters;

// Returns true if the given type name is one of the well known message types
// that CEL treats specially.
//
// For familiarity with textproto, these types may be created using the struct
// creation syntax, even though they are not considered a struct type in CEL.
bool IsWellKnownMessageType(absl::string_view name);

namespace common_internal {

class BasicStructType;
class BasicStructTypeField;

// Constructs `BasicStructType` from a type name. The type name must not be one
// of the well known message types we treat specially, if it is behavior is
// undefined. The name must also outlive the resulting type.
BasicStructType MakeBasicStructType(
    absl::string_view name ABSL_ATTRIBUTE_LIFETIME_BOUND);

class BasicStructType final {
 public:
  static constexpr TypeKind kKind = TypeKind::kStruct;

  BasicStructType() = default;
  BasicStructType(const BasicStructType&) = default;
  BasicStructType(BasicStructType&&) = default;
  BasicStructType& operator=(const BasicStructType&) = default;
  BasicStructType& operator=(BasicStructType&&) = default;

  static TypeKind kind() { return kKind; }

  absl::string_view name() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(*this);
    return name_;
  }

  static TypeParameters GetParameters();

  std::string DebugString() const {
    return std::string(static_cast<bool>(*this) ? name() : absl::string_view());
  }

  explicit operator bool() const { return !name_.empty(); }

 private:
  friend BasicStructType MakeBasicStructType(
      absl::string_view name ABSL_ATTRIBUTE_LIFETIME_BOUND);

  explicit BasicStructType(absl::string_view name ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : name_(name) {}

  absl::string_view name_;
};

inline bool operator==(BasicStructType lhs, BasicStructType rhs) {
  return static_cast<bool>(lhs) == static_cast<bool>(rhs) &&
         (!static_cast<bool>(lhs) || lhs.name() == rhs.name());
}

inline bool operator!=(BasicStructType lhs, BasicStructType rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, BasicStructType type) {
  ABSL_DCHECK(type);
  return H::combine(std::move(state), static_cast<bool>(type)
                                          ? type.name()
                                          : absl::string_view());
}

inline std::ostream& operator<<(std::ostream& out, BasicStructType type) {
  return out << type.DebugString();
}

inline BasicStructType MakeBasicStructType(
    absl::string_view name ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  ABSL_DCHECK(!IsWellKnownMessageType(name)) << name;
  return BasicStructType(name);
}

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_BASIC_STRUCT_TYPE_H_
