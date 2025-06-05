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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUE_MANAGER_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUE_MANAGER_H_

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_manager.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_factory.h"

namespace cel {

// `ValueManager` is an additional layer on top of `ValueFactory` and
// `TypeReflector` which combines the two and adds additional functionality.
class ValueManager : public virtual ValueFactory,
                     public virtual TypeManager,
                     public AnyToJsonConverter {
 public:
  const TypeReflector& type_provider() const { return GetTypeReflector(); }

  // See `TypeReflector::NewListValueBuilder`.
  absl::StatusOr<absl::Nonnull<ListValueBuilderPtr>> NewListValueBuilder(
      const ListType& type) {
    return GetTypeReflector().NewListValueBuilder(*this, type);
  }

  // See `TypeReflector::NewMapValueBuilder`.
  absl::StatusOr<absl::Nonnull<MapValueBuilderPtr>> NewMapValueBuilder(
      const MapType& type) {
    return GetTypeReflector().NewMapValueBuilder(*this, type);
  }

  // See `TypeReflector::NewStructValueBuilder`.
  absl::StatusOr<absl::Nullable<StructValueBuilderPtr>> NewStructValueBuilder(
      const StructType& type) {
    return GetTypeReflector().NewStructValueBuilder(*this, type);
  }

  // See `TypeReflector::NewValueBuilder`.
  absl::StatusOr<absl::Nullable<ValueBuilderPtr>> NewValueBuilder(
      absl::string_view name) {
    return GetTypeReflector().NewValueBuilder(*this, name);
  }

  // See `TypeReflector::FindValue`.
  absl::StatusOr<bool> FindValue(absl::string_view name, Value& result) {
    return GetTypeReflector().FindValue(*this, name, result);
  }

  // See `TypeReflector::DeserializeValue`.
  absl::StatusOr<absl::optional<Value>> DeserializeValue(
      absl::string_view type_url, const absl::Cord& value) {
    return GetTypeReflector().DeserializeValue(*this, type_url, value);
  }

  absl::StatusOr<Json> ConvertToJson(absl::string_view type_url,
                                     const absl::Cord& value) final;

 protected:
  virtual const TypeReflector& GetTypeReflector() const = 0;
};

// Creates a new `ValueManager` which is thread compatible.
Shared<ValueManager> NewThreadCompatibleValueManager(
    MemoryManagerRef memory_manager, Shared<TypeReflector> type_reflector);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUE_MANAGER_H_
