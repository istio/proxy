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

#ifndef THIRD_PARTY_CEL_CPP_BASE_INTERNAL_MEMORY_MANAGER_TESTING_H_
#define THIRD_PARTY_CEL_CPP_BASE_INTERNAL_MEMORY_MANAGER_TESTING_H_

#include <string>
#include <utility>

#include "internal/testing.h"

namespace cel::base_internal {

enum class MemoryManagerTestMode {
  kGlobal = 0,
  kArena,
};

std::string MemoryManagerTestModeToString(MemoryManagerTestMode mode);

template <typename S>
void AbslStringify(S& sink, MemoryManagerTestMode mode) {
  sink.Append(MemoryManagerTestModeToString(mode));
}

inline auto MemoryManagerTestModeAll() {
  return testing::Values(MemoryManagerTestMode::kGlobal,
                         MemoryManagerTestMode::kArena);
}

inline std::string MemoryManagerTestModeName(
    const testing::TestParamInfo<MemoryManagerTestMode>& info) {
  return MemoryManagerTestModeToString(info.param);
}

inline std::string MemoryManagerTestModeTupleName(
    const testing::TestParamInfo<std::tuple<MemoryManagerTestMode>>& info) {
  return MemoryManagerTestModeToString(std::get<0>(info.param));
}

}  // namespace cel::base_internal

#endif  // THIRD_PARTY_CEL_CPP_BASE_INTERNAL_MEMORY_MANAGER_TESTING_H_
