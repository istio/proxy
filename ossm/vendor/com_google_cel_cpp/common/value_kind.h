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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUE_KIND_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUE_KIND_H_

#include <type_traits>

#include "absl/base/macros.h"
#include "absl/strings/string_view.h"
#include "common/kind.h"

namespace cel {

// `ValueKind` is a subset of `Kind`, representing all valid `Kind` for `Value`.
// All `ValueKind` are valid `Kind`, but it is not guaranteed that all `Kind`
// are valid `ValueKind`.
enum class ValueKind : std::underlying_type_t<Kind> {
  kNull = static_cast<int>(Kind::kNull),
  kBool = static_cast<int>(Kind::kBool),
  kInt = static_cast<int>(Kind::kInt),
  kUint = static_cast<int>(Kind::kUint),
  kDouble = static_cast<int>(Kind::kDouble),
  kString = static_cast<int>(Kind::kString),
  kBytes = static_cast<int>(Kind::kBytes),
  kStruct = static_cast<int>(Kind::kStruct),
  kDuration = static_cast<int>(Kind::kDuration),
  kTimestamp = static_cast<int>(Kind::kTimestamp),
  kList = static_cast<int>(Kind::kList),
  kMap = static_cast<int>(Kind::kMap),
  kUnknown = static_cast<int>(Kind::kUnknown),
  kType = static_cast<int>(Kind::kType),
  kError = static_cast<int>(Kind::kError),
  kOpaque = static_cast<int>(Kind::kOpaque),

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
      static_cast<int>(Kind::kNotForUseWithExhaustiveSwitchStatements),
};

constexpr Kind ValueKindToKind(ValueKind kind) {
  return static_cast<Kind>(
      static_cast<std::underlying_type_t<ValueKind>>(kind));
}

constexpr bool KindIsValueKind(Kind kind) {
  return kind != Kind::kBoolWrapper && kind != Kind::kIntWrapper &&
         kind != Kind::kUintWrapper && kind != Kind::kDoubleWrapper &&
         kind != Kind::kStringWrapper && kind != Kind::kBytesWrapper &&
         kind != Kind::kDyn && kind != Kind::kAny && kind != Kind::kTypeParam &&
         kind != Kind::kFunction;
}

constexpr bool operator==(Kind lhs, ValueKind rhs) {
  return lhs == ValueKindToKind(rhs);
}

constexpr bool operator==(ValueKind lhs, Kind rhs) {
  return ValueKindToKind(lhs) == rhs;
}

constexpr bool operator!=(Kind lhs, ValueKind rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(ValueKind lhs, Kind rhs) {
  return !operator==(lhs, rhs);
}

inline absl::string_view ValueKindToString(ValueKind kind) {
  // All ValueKind are valid Kind.
  return KindToString(ValueKindToKind(kind));
}

constexpr ValueKind KindToValueKind(Kind kind) {
  ABSL_ASSERT(KindIsValueKind(kind));
  return static_cast<ValueKind>(
      static_cast<std::underlying_type_t<Kind>>(kind));
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUE_KIND_H_
