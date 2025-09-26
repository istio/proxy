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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPE_KIND_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPE_KIND_H_

#include <cstdint>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/strings/string_view.h"
#include "common/kind.h"

namespace cel {

// `TypeKind` is a subset of `Kind`, representing all valid `Kind` for `Type`.
// All `TypeKind` are valid `Kind`, but it is not guaranteed that all `Kind` are
// valid `TypeKind`.
enum class TypeKind : std::underlying_type_t<Kind> {
  kNull = static_cast<uint8_t>(Kind::kNull),
  kBool = static_cast<uint8_t>(Kind::kBool),
  kInt = static_cast<uint8_t>(Kind::kInt),
  kUint = static_cast<uint8_t>(Kind::kUint),
  kDouble = static_cast<uint8_t>(Kind::kDouble),
  kString = static_cast<uint8_t>(Kind::kString),
  kBytes = static_cast<uint8_t>(Kind::kBytes),
  kStruct = static_cast<uint8_t>(Kind::kStruct),
  kDuration = static_cast<uint8_t>(Kind::kDuration),
  kTimestamp = static_cast<uint8_t>(Kind::kTimestamp),
  kList = static_cast<uint8_t>(Kind::kList),
  kMap = static_cast<uint8_t>(Kind::kMap),
  kUnknown = static_cast<uint8_t>(Kind::kUnknown),
  kType = static_cast<uint8_t>(Kind::kType),
  kError = static_cast<uint8_t>(Kind::kError),
  kAny = static_cast<uint8_t>(Kind::kAny),
  kDyn = static_cast<uint8_t>(Kind::kDyn),
  kOpaque = static_cast<uint8_t>(Kind::kOpaque),

  kBoolWrapper = static_cast<uint8_t>(Kind::kBoolWrapper),
  kIntWrapper = static_cast<uint8_t>(Kind::kIntWrapper),
  kUintWrapper = static_cast<uint8_t>(Kind::kUintWrapper),
  kDoubleWrapper = static_cast<uint8_t>(Kind::kDoubleWrapper),
  kStringWrapper = static_cast<uint8_t>(Kind::kStringWrapper),
  kBytesWrapper = static_cast<uint8_t>(Kind::kBytesWrapper),

  kTypeParam = static_cast<uint8_t>(Kind::kTypeParam),
  kFunction = static_cast<uint8_t>(Kind::kFunction),
  kEnum = static_cast<uint8_t>(Kind::kEnum),

  // Legacy aliases, deprecated do not use.
  kNullType = kNull,
  kInt64 = kInt,
  kUint64 = kUint,
  kMessage = kStruct,
  kUnknownSet = kUnknown,
  kCelType = kType,

  // INTERNAL: Do not exceed 63. Implementation details rely on the fact that
  // we can store `Kind` using 6 bits.
  kNotForUseWithExhaustiveSwitchStatements =
      static_cast<uint8_t>(Kind::kNotForUseWithExhaustiveSwitchStatements),
};

constexpr Kind TypeKindToKind(TypeKind kind) {
  return static_cast<Kind>(static_cast<std::underlying_type_t<TypeKind>>(kind));
}

constexpr bool KindIsTypeKind(Kind kind ABSL_ATTRIBUTE_UNUSED) {
  // Currently all Kind are valid TypeKind.
  return true;
}

constexpr bool operator==(Kind lhs, TypeKind rhs) {
  return lhs == TypeKindToKind(rhs);
}

constexpr bool operator==(TypeKind lhs, Kind rhs) {
  return TypeKindToKind(lhs) == rhs;
}

constexpr bool operator!=(Kind lhs, TypeKind rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(TypeKind lhs, Kind rhs) {
  return !operator==(lhs, rhs);
}

inline absl::string_view TypeKindToString(TypeKind kind) {
  // All TypeKind are valid Kind.
  return KindToString(TypeKindToKind(kind));
}

constexpr TypeKind KindToTypeKind(Kind kind) {
  ABSL_ASSERT(KindIsTypeKind(kind));
  return static_cast<TypeKind>(static_cast<std::underlying_type_t<Kind>>(kind));
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPE_KIND_H_
