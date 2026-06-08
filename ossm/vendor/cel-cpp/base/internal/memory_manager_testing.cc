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

#include "base/internal/memory_manager_testing.h"

#include <string>

namespace cel::base_internal {

std::string MemoryManagerTestModeToString(MemoryManagerTestMode mode) {
  switch (mode) {
    case MemoryManagerTestMode::kGlobal:
      return "Global";
    case MemoryManagerTestMode::kArena:
      return "Arena";
  }
}

}  // namespace cel::base_internal
