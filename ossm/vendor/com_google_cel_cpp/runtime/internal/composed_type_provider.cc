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
#include "runtime/internal/composed_type_provider.h"

#include <memory>

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "internal/status_macros.h"

namespace cel::runtime_internal {

absl::StatusOr<absl::Nonnull<ListValueBuilderPtr>>
ComposedTypeProvider::NewListValueBuilder(ValueFactory& value_factory,
                                          const ListType& type) const {
  if (use_legacy_container_builders_) {
    return TypeReflector::LegacyBuiltin().NewListValueBuilder(value_factory,
                                                              type);
  }
  return TypeReflector::ModernBuiltin().NewListValueBuilder(value_factory,
                                                            type);
}

absl::StatusOr<absl::Nonnull<MapValueBuilderPtr>>
ComposedTypeProvider::NewMapValueBuilder(ValueFactory& value_factory,
                                         const MapType& type) const {
  if (use_legacy_container_builders_) {
    return TypeReflector::LegacyBuiltin().NewMapValueBuilder(value_factory,
                                                             type);
  }
  return TypeReflector::ModernBuiltin().NewMapValueBuilder(value_factory, type);
}

absl::StatusOr<absl::Nullable<StructValueBuilderPtr>>
ComposedTypeProvider::NewStructValueBuilder(ValueFactory& value_factory,
                                            const StructType& type) const {
  for (const std::unique_ptr<TypeReflector>& provider : providers_) {
    CEL_ASSIGN_OR_RETURN(auto builder,
                         provider->NewStructValueBuilder(value_factory, type));
    if (builder != nullptr) {
      return builder;
    }
  }
  return nullptr;
}

absl::StatusOr<bool> ComposedTypeProvider::FindValue(
    ValueFactory& value_factory, absl::string_view name, Value& result) const {
  for (const std::unique_ptr<TypeReflector>& provider : providers_) {
    CEL_ASSIGN_OR_RETURN(auto value,
                         provider->FindValue(value_factory, name, result));
    if (value) {
      return value;
    }
  }
  return false;
}

absl::StatusOr<absl::optional<Value>>
ComposedTypeProvider::DeserializeValueImpl(ValueFactory& value_factory,
                                           absl::string_view type_url,
                                           const absl::Cord& value) const {
  for (const std::unique_ptr<TypeReflector>& provider : providers_) {
    CEL_ASSIGN_OR_RETURN(auto result, provider->DeserializeValue(
                                          value_factory, type_url, value));
    if (result.has_value()) {
      return result;
    }
  }
  return absl::nullopt;
}

absl::StatusOr<absl::optional<Type>> ComposedTypeProvider::FindTypeImpl(
    TypeFactory& type_factory, absl::string_view name) const {
  for (const std::unique_ptr<TypeReflector>& provider : providers_) {
    CEL_ASSIGN_OR_RETURN(auto result, provider->FindType(type_factory, name));
    if (result.has_value()) {
      return result;
    }
  }
  return absl::nullopt;
}

absl::StatusOr<absl::optional<StructTypeField>>
ComposedTypeProvider::FindStructTypeFieldByNameImpl(
    TypeFactory& type_factory, absl::string_view type,
    absl::string_view name) const {
  for (const std::unique_ptr<TypeReflector>& provider : providers_) {
    CEL_ASSIGN_OR_RETURN(auto result, provider->FindStructTypeFieldByName(
                                          type_factory, type, name));
    if (result.has_value()) {
      return result;
    }
  }
  return absl::nullopt;
}

}  // namespace cel::runtime_internal
