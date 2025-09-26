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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_OPTIONAL_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_OPTIONAL_TYPE_H_

#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/utility/utility.h"
#include "common/type_kind.h"
#include "common/types/opaque_type.h"
#include "google/protobuf/arena.h"

namespace cel {

class Type;
class TypeParameters;

class OptionalType final {
 public:
  static constexpr TypeKind kKind = TypeKind::kOpaque;
  static constexpr absl::string_view kName = "optional_type";

  // By default, this type is `optional(dyn)`. Unless you can help it, you
  // should choose a more specific optional type.
  OptionalType();

  OptionalType(google::protobuf::Arena* absl_nonnull arena, const Type& parameter)
      : OptionalType(
            absl::in_place,
            OpaqueType(arena, kName, absl::MakeConstSpan(&parameter, 1))) {}

  static TypeKind kind() { return kKind; }

  static absl::string_view name() { return kName; }

  std::string DebugString() const { return opaque_.DebugString(); }

  TypeParameters GetParameters() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Type GetParameter() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  explicit operator bool() const { return static_cast<bool>(opaque_); }

  template <typename H>
  friend H AbslHashValue(H state, const OptionalType& type) {
    return H::combine(std::move(state), type.opaque_);
  }

  friend bool operator==(const OptionalType& lhs, const OptionalType& rhs) {
    return lhs.opaque_ == rhs.opaque_;
  }

 private:
  friend class OpaqueType;

  OptionalType(absl::in_place_t, OpaqueType type) : opaque_(std::move(type)) {}

  OpaqueType opaque_;
};

inline bool operator!=(const OptionalType& lhs, const OptionalType& rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, const OptionalType& type) {
  return out << type.DebugString();
}

inline OpaqueType::OpaqueType(OptionalType type)
    : OpaqueType(std::move(type.opaque_)) {}

inline OpaqueType& OpaqueType::operator=(OptionalType type) {
  return *this = std::move(type.opaque_);
}

template <typename T>
inline std::enable_if_t<std::is_same_v<OptionalType, T>,
                        absl::optional<OptionalType>>
OpaqueType::As() const {
  return AsOptional();
}

template <typename T>
inline std::enable_if_t<std::is_same_v<OptionalType, T>, OptionalType>
OpaqueType::Get() const {
  return GetOptional();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_OPTIONAL_TYPE_H_
