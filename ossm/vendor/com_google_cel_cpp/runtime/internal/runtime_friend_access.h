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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_RUNTIME_FRIEND_ACCESS_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_RUNTIME_FRIEND_ACCESS_H_

#include "common/native_type.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"

namespace cel::runtime_internal {

// Provide accessors for friend-visibility internal runtime details.
//
// CEL supported runtime extensions need implementation specific details to work
// correctly. We restrict access to prevent external usages since we don't
// guarantee stability on the implementation details.
class RuntimeFriendAccess {
 public:
  // Access underlying runtime instance.
  static Runtime& GetMutableRuntime(RuntimeBuilder& builder) {
    return builder.runtime();
  }

  // Return the internal type_id for the runtime instance for checked down
  // casting.
  static NativeTypeId RuntimeTypeId(Runtime& runtime) {
    return runtime.GetNativeTypeId();
  }
};

}  // namespace cel::runtime_internal

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_RUNTIME_EXTENSIONS_FRIEND_ACCESS_H_
