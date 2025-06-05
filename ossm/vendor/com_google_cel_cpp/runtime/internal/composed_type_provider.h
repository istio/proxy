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
#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_COMPOSED_TYPE_PROVIDER_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_COMPOSED_TYPE_PROVIDER_H_

#include <memory>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "base/type_provider.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_factory.h"

namespace cel::runtime_internal {

// Type provider implementation managed by the runtime type registry.
//
// Maintains ownership of client provided type provider implementations and
// delegates type resolution to them in order. To meet the requirements for use
// with TypeManager, this should not be updated after any call to ProvideType.
//
// The builtin type provider is implicitly consulted first in a type manager,
// so it is not represented here.
class ComposedTypeProvider : public TypeReflector {
 public:
  // Register an additional type provider.
  void AddTypeProvider(std::unique_ptr<TypeReflector> provider) {
    providers_.push_back(std::move(provider));
  }

  void set_use_legacy_container_builders(bool use_legacy_container_builders) {
    use_legacy_container_builders_ = use_legacy_container_builders;
  }

  // `NewListValueBuilder` returns a new `ListValueBuilderInterface` for the
  // corresponding `ListType` `type`.
  absl::StatusOr<absl::Nonnull<ListValueBuilderPtr>> NewListValueBuilder(
      ValueFactory& value_factory, const ListType& type) const override;

  // `NewMapValueBuilder` returns a new `MapValueBuilderInterface` for the
  // corresponding `MapType` `type`.
  absl::StatusOr<absl::Nonnull<MapValueBuilderPtr>> NewMapValueBuilder(
      ValueFactory& value_factory, const MapType& type) const override;

  absl::StatusOr<absl::Nullable<StructValueBuilderPtr>> NewStructValueBuilder(
      ValueFactory& value_factory, const StructType& type) const override;

  absl::StatusOr<bool> FindValue(ValueFactory& value_factory,
                                 absl::string_view name,
                                 Value& result) const override;

 protected:
  absl::StatusOr<absl::optional<Value>> DeserializeValueImpl(
      ValueFactory& value_factory, absl::string_view type_url,
      const absl::Cord& value) const override;

  absl::StatusOr<absl::optional<Type>> FindTypeImpl(
      TypeFactory& type_factory, absl::string_view name) const override;

  absl::StatusOr<absl::optional<StructTypeField>> FindStructTypeFieldByNameImpl(
      TypeFactory& type_factory, absl::string_view type,
      absl::string_view name) const override;

 private:
  std::vector<std::unique_ptr<TypeReflector>> providers_;
  bool use_legacy_container_builders_ = true;
};

}  // namespace cel::runtime_internal

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_COMPOSED_TYPE_PROVIDER_H_
