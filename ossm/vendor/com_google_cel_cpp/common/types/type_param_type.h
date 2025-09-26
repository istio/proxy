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
// IWYU pragma: friend "common/type.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPE_PARAM_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPE_PARAM_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "common/type_kind.h"

namespace cel {

class Type;
class TypeParameters;

class TypeParamType final {
 public:
  static constexpr TypeKind kKind = TypeKind::kTypeParam;

  explicit TypeParamType(absl::string_view name ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : name_(name) {}

  TypeParamType() = default;
  TypeParamType(const TypeParamType&) = default;
  TypeParamType(TypeParamType&&) = default;
  TypeParamType& operator=(const TypeParamType&) = default;
  TypeParamType& operator=(TypeParamType&&) = default;

  static TypeKind kind() { return kKind; }

  absl::string_view name() const ABSL_ATTRIBUTE_LIFETIME_BOUND { return name_; }

  static TypeParameters GetParameters();

  std::string DebugString() const { return std::string(name()); }

 private:
  absl::string_view name_;
};

inline bool operator==(const TypeParamType& lhs, const TypeParamType& rhs) {
  return lhs.name() == rhs.name();
}

inline bool operator!=(const TypeParamType& lhs, const TypeParamType& rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, const TypeParamType& type) {
  return H::combine(std::move(state), type.name());
}

inline std::ostream& operator<<(std::ostream& out, const TypeParamType& type) {
  return out << type.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPE_PARAM_TYPE_H_
