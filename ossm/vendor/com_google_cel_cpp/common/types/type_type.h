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

// IWYU pragma: private, include "common/type.h"
// IWYU pragma: friend "common/type.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPE_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPE_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "common/type_kind.h"
#include "google/protobuf/arena.h"

namespace cel {

class Type;
class TypeParameters;

namespace common_internal {
struct TypeTypeData;
}  // namespace common_internal

// `TypeType` is a special type which represents the type of a type.
class TypeType final {
 public:
  static constexpr TypeKind kKind = TypeKind::kType;
  static constexpr absl::string_view kName = "type";

  TypeType(google::protobuf::Arena* absl_nonnull arena, const Type& parameter);

  TypeType() = default;
  TypeType(const TypeType&) = default;
  TypeType(TypeType&&) = default;
  TypeType& operator=(const TypeType&) = default;
  TypeType& operator=(TypeType&&) = default;

  static TypeKind kind() { return kKind; }

  static absl::string_view name() { return kName; }

  TypeParameters GetParameters() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  std::string DebugString() const;

  Type GetType() const;

 private:
  explicit TypeType(const common_internal::TypeTypeData* absl_nullable data)
      : data_(data) {}

  const common_internal::TypeTypeData* absl_nullable data_ = nullptr;
};

inline constexpr bool operator==(const TypeType&, const TypeType&) {
  return true;
}

inline constexpr bool operator!=(const TypeType& lhs, const TypeType& rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, const TypeType&) {
  // TypeType is really a singleton and all instances are equal. Nothing to
  // hash.
  return std::move(state);
}

inline std::ostream& operator<<(std::ostream& out, const TypeType& type) {
  return out << type.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPE_TYPE_H_
