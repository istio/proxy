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

// IWYU pragma: private

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_THREAD_COMPATIBLE_TYPE_MANAGER_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_THREAD_COMPATIBLE_TYPE_MANAGER_H_

#include <utility>

#include "common/memory.h"
#include "common/type_introspector.h"
#include "common/type_manager.h"

namespace cel::common_internal {

class ThreadCompatibleTypeManager : public virtual TypeManager {
 public:
  explicit ThreadCompatibleTypeManager(
      MemoryManagerRef memory_manager,
      Shared<TypeIntrospector> type_introspector)
      : memory_manager_(memory_manager),
        type_introspector_(std::move(type_introspector)) {}

  MemoryManagerRef GetMemoryManager() const final { return memory_manager_; }

 protected:
  TypeIntrospector& GetTypeIntrospector() const final {
    return *type_introspector_;
  }

 private:
  MemoryManagerRef memory_manager_;
  Shared<TypeIntrospector> type_introspector_;
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_THREAD_COMPATIBLE_TYPE_MANAGER_H_
