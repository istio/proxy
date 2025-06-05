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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_KIND_H_
#define THIRD_PARTY_CEL_CPP_COMMON_KIND_H_

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"

namespace cel {

enum class Kind /* : uint8_t */ {
  // Must match legacy CelValue::Type.
  kNull = 0,
  kBool,
  kInt,
  kUint,
  kDouble,
  kString,
  kBytes,
  kStruct,
  kDuration,
  kTimestamp,
  kList,
  kMap,
  kUnknown,
  kType,
  kError,
  kAny,

  // New kinds not present in legacy CelValue.
  kDyn,
  kOpaque,

  kBoolWrapper,
  kIntWrapper,
  kUintWrapper,
  kDoubleWrapper,
  kStringWrapper,
  kBytesWrapper,

  kTypeParam,
  kFunction,
  kEnum,

  // Legacy aliases, deprecated do not use.
  kNullType = kNull,
  kInt64 = kInt,
  kUint64 = kUint,
  kMessage = kStruct,
  kUnknownSet = kUnknown,
  kCelType = kType,

  // INTERNAL: Do not exceed 63. Implementation details rely on the fact that
  // we can store `Kind` using 6 bits.
  kNotForUseWithExhaustiveSwitchStatements = 63,
};

ABSL_ATTRIBUTE_PURE_FUNCTION absl::string_view KindToString(Kind kind);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_KIND_H_
