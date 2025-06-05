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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_TYPE_REGISTRY_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_TYPE_REGISTRY_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "base/type_provider.h"
#include "runtime/internal/composed_type_provider.h"

namespace cel {

// TypeRegistry manages composing TypeProviders used with a Runtime.
//
// It provides a single effective type provider to be used in a ValueManager.
class TypeRegistry {
 public:
  // Representation for a custom enum constant.
  struct Enumerator {
    std::string name;
    int64_t number;
  };

  struct Enumeration {
    std::string name;
    std::vector<Enumerator> enumerators;
  };

  TypeRegistry();

  // Move-only
  TypeRegistry(const TypeRegistry& other) = delete;
  TypeRegistry& operator=(TypeRegistry& other) = delete;
  TypeRegistry(TypeRegistry&& other) = default;
  TypeRegistry& operator=(TypeRegistry&& other) = default;

  void AddTypeProvider(std::unique_ptr<TypeProvider> provider) {
    impl_.AddTypeProvider(std::move(provider));
  }

  // Register a custom enum type.
  //
  // This adds the enum to the set consulted at plan time to identify constant
  // enum values.
  void RegisterEnum(absl::string_view enum_name,
                    std::vector<Enumerator> enumerators);

  const absl::flat_hash_map<std::string, Enumeration>& resolveable_enums()
      const {
    return enum_types_;
  }

  // Returns the effective type provider.
  const TypeProvider& GetComposedTypeProvider() const { return impl_; }
  void set_use_legacy_container_builders(bool use_legacy_container_builders) {
    impl_.set_use_legacy_container_builders(use_legacy_container_builders);
  }

 private:
  runtime_internal::ComposedTypeProvider impl_;
  absl::flat_hash_map<std::string, Enumeration> enum_types_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_TYPE_REGISTRY_H_
