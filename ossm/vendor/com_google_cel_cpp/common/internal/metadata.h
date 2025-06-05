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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_METADATA_H_
#define THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_METADATA_H_

#include <cstdint>

#include "google/protobuf/arena.h"

namespace cel::common_internal {

// `google::protobuf::Arena` has a minimum alignment of 8. `ReferenceCount` has a minimum
// alignment that is guaranteed to be greater than or equal to `google::protobuf::Arena`.
inline constexpr uintptr_t kMetadataOwnerNone = 0;
inline constexpr uintptr_t kMetadataOwnerReferenceCountBit = uintptr_t{1} << 0;
inline constexpr uintptr_t kMetadataOwnerArenaBit = uintptr_t{1} << 1;
inline constexpr uintptr_t kMetadataOwnerBits = alignof(google::protobuf::Arena) - 1;
inline constexpr uintptr_t kMetadataOwnerPointerMask = ~kMetadataOwnerBits;

// Ensure kMetadataOwnerBits encompasses kMetadataOwnerReferenceCountBit and
// kMetadataOwnerArenaBit.
static_assert((kMetadataOwnerBits | kMetadataOwnerReferenceCountBit) ==
              kMetadataOwnerBits);
static_assert((kMetadataOwnerBits | kMetadataOwnerArenaBit) ==
              kMetadataOwnerBits);

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_METADATA_H_
