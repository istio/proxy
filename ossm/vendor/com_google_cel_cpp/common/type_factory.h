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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPE_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPE_FACTORY_H_

#include "common/memory.h"

namespace cel {

namespace common_internal {
class PiecewiseValueManager;
}

// `TypeFactory` is the preferred way for constructing compound types such as
// lists, maps, structs, and opaques. It caches types and avoids constructing
// them multiple times.
class TypeFactory {
 public:
  virtual ~TypeFactory() = default;

  // Returns a `MemoryManagerRef` which is used to manage memory for internal
  // data structures as well as created types.
  virtual MemoryManagerRef GetMemoryManager() const = 0;

 protected:
  friend class common_internal::PiecewiseValueManager;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPE_FACTORY_H_
