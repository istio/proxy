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

// IWYU pragma: private

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_LEGACY_TYPE_MANAGER_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_LEGACY_TYPE_MANAGER_H_

#include "common/memory.h"
#include "common/type_introspector.h"
#include "common/type_manager.h"

namespace cel::common_internal {

// `LegacyTypeManager` is an implementation which should be used when
// converting between `cel::Value` and `google::api::expr::runtime::CelValue`
// and only then.
class LegacyTypeManager : public virtual TypeManager {
 public:
  LegacyTypeManager(MemoryManagerRef memory_manager,
                    const TypeIntrospector& type_introspector)
      : memory_manager_(memory_manager),
        type_introspector_(type_introspector) {}

  MemoryManagerRef GetMemoryManager() const final { return memory_manager_; }

 protected:
  const TypeIntrospector& GetTypeIntrospector() const final {
    return type_introspector_;
  }

 private:
  MemoryManagerRef memory_manager_;
  const TypeIntrospector& type_introspector_;
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_LEGACY_TYPE_MANAGER_H_
