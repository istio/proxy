// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_BASE_INTERNAL_MESSAGE_WRAPPER_H_
#define THIRD_PARTY_CEL_CPP_BASE_INTERNAL_MESSAGE_WRAPPER_H_

#include <cstdint>

namespace cel::base_internal {

inline constexpr uintptr_t kMessageWrapperTagMask = 0b1;
inline constexpr uintptr_t kMessageWrapperPtrMask = ~kMessageWrapperTagMask;
inline constexpr int kMessageWrapperTagSize = 1;
inline constexpr uintptr_t kMessageWrapperTagTypeInfoValue = 0b0;
inline constexpr uintptr_t kMessageWrapperTagMessageValue = 0b1;

}  // namespace cel::base_internal

#endif  // THIRD_PARTY_CEL_CPP_BASE_INTERNAL_MESSAGE_WRAPPER_H_
