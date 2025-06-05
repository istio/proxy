// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_TYPE_REGISTRY_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_TYPE_REGISTRY_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "eval/public/structs/legacy_type_provider.h"
#include "runtime/internal/composed_type_provider.h"
#include "runtime/type_registry.h"

namespace google::api::expr::runtime {

// CelTypeRegistry manages the set of registered types available for use within
// object literal construction, enum comparisons, and type testing.
//
// The CelTypeRegistry is intended to live for the duration of all CelExpression
// values created by a given CelExpressionBuilder and one is created by default
// within the standard CelExpressionBuilder.
//
// By default, all core CEL types and all linked protobuf message types are
// implicitly registered by way of the generated descriptor pool. A descriptor
// pool can be given to avoid accidentally exposing linked protobuf types to CEL
// which were intended to remain internal or to operate on hermetic descriptor
// pools.
class CelTypeRegistry {
 public:
  // Representation of an enum constant.
  using Enumerator = cel::TypeRegistry::Enumerator;

  // Representation of an enum.
  using Enumeration = cel::TypeRegistry::Enumeration;

  CelTypeRegistry();

  ~CelTypeRegistry() = default;

  // Register an enum whose values may be used within CEL expressions.
  //
  // Enum registration must be performed prior to CelExpression creation.
  void Register(const google::protobuf::EnumDescriptor* enum_descriptor);

  // Register an enum whose values may be used within CEL expressions.
  //
  // Enum registration must be performed prior to CelExpression creation.
  void RegisterEnum(absl::string_view name,
                    std::vector<Enumerator> enumerators);

  // Register a new type provider.
  //
  // Type providers are consulted in the order they are added.
  void RegisterTypeProvider(std::unique_ptr<LegacyTypeProvider> provider);

  // Get the first registered type provider.
  std::shared_ptr<const LegacyTypeProvider> GetFirstTypeProvider() const;

  // Returns the effective type provider that has been configured with the
  // registry.
  //
  // This is a composited type provider that should check in order:
  // - builtins (via TypeManager)
  // - custom enumerations
  // - registered extension type providers in the order registered.
  const cel::TypeProvider& GetTypeProvider() const {
    return modern_type_registry_.GetComposedTypeProvider();
  }

  // Register an additional type provider with the registry.
  //
  // A pointer to the registered provider is returned to support testing,
  // but users should prefer to use the composed type provider from
  // GetTypeProvider()
  void RegisterModernTypeProvider(std::unique_ptr<cel::TypeProvider> provider) {
    return modern_type_registry_.AddTypeProvider(std::move(provider));
  }

  // Find a type adapter given a fully qualified type name.
  // Adapter provides a generic interface for the reflection operations the
  // interpreter needs to provide.
  absl::optional<LegacyTypeAdapter> FindTypeAdapter(
      absl::string_view fully_qualified_type_name) const;

  // Return the registered enums configured within the type registry in the
  // internal format that can be identified as int constants at plan time.
  const absl::flat_hash_map<std::string, Enumeration>& resolveable_enums()
      const {
    return modern_type_registry_.resolveable_enums();
  }

  // Return the registered enums configured within the type registry.
  //
  // This is provided for validating registry setup, it should not be used
  // internally.
  //
  // Invalidated whenever registered enums are updated.
  absl::flat_hash_set<absl::string_view> ListResolveableEnums() const {
    const auto& enums = resolveable_enums();
    absl::flat_hash_set<absl::string_view> result;
    result.reserve(enums.size());

    for (const auto& entry : enums) {
      result.insert(entry.first);
    }

    return result;
  }

  // Accessor for underlying modern registry.
  //
  // This is exposed for migrating runtime internals, CEL users should not call
  // this.
  cel::TypeRegistry& InternalGetModernRegistry() {
    return modern_type_registry_;
  }

  const cel::TypeRegistry& InternalGetModernRegistry() const {
    return modern_type_registry_;
  }

 private:
  // Internal modern registry.
  cel::TypeRegistry modern_type_registry_;

  // TODO: This is needed to inspect the registered legacy type
  // providers for client tests. This can be removed when they are migrated to
  // use the modern APIs.
  std::vector<std::shared_ptr<const LegacyTypeProvider>> legacy_type_providers_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_TYPE_REGISTRY_H_
