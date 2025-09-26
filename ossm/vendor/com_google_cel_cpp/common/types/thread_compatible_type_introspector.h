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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_THREAD_COMPATIBLE_TYPE_INTROSPECTOR_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_THREAD_COMPATIBLE_TYPE_INTROSPECTOR_H_

#include "common/type_introspector.h"

namespace cel::common_internal {

// `ThreadCompatibleTypeIntrospector` is a basic implementation of
// `TypeIntrospector` which is thread compatible. By default this implementation
// just returns `NOT_FOUND` for most methods.
class ThreadCompatibleTypeIntrospector : public virtual TypeIntrospector {
 public:
  ThreadCompatibleTypeIntrospector() = default;
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_THREAD_COMPATIBLE_TYPE_INTROSPECTOR_H_
