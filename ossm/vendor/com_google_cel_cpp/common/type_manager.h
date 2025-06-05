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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPE_MANAGER_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPE_MANAGER_H_

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_factory.h"
#include "common/type_introspector.h"

namespace cel {

// `TypeManager` is an additional layer on top of `TypeFactory` and
// `TypeIntrospector` which combines the two and adds additional functionality.
class TypeManager : public virtual TypeFactory {
 public:
  virtual ~TypeManager() = default;

  // See `TypeIntrospector::FindType`.
  absl::StatusOr<absl::optional<Type>> FindType(absl::string_view name) {
    return GetTypeIntrospector().FindType(*this, name);
  }

  // See `TypeIntrospector::FindStructTypeFieldByName`.
  absl::StatusOr<absl::optional<StructTypeField>> FindStructTypeFieldByName(
      absl::string_view type, absl::string_view name) {
    return GetTypeIntrospector().FindStructTypeFieldByName(*this, type, name);
  }

  // See `TypeIntrospector::FindStructTypeFieldByName`.
  absl::StatusOr<absl::optional<StructTypeField>> FindStructTypeFieldByName(
      const StructType& type, absl::string_view name) {
    return GetTypeIntrospector().FindStructTypeFieldByName(*this, type, name);
  }

 protected:
  virtual const TypeIntrospector& GetTypeIntrospector() const = 0;
};

// Creates a new `TypeManager` which is thread compatible.
Shared<TypeManager> NewThreadCompatibleTypeManager(
    MemoryManagerRef memory_manager,
    Shared<TypeIntrospector> type_introspector);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPE_MANAGER_H_
