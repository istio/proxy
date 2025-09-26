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
#include <vector>

#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "base/type_provider.h"
#include "common/type.h"
#include "common/value.h"
#include "runtime/internal/legacy_runtime_type_provider.h"
#include "runtime/internal/runtime_type_provider.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

class TypeRegistry;

namespace runtime_internal {
const RuntimeTypeProvider& GetRuntimeTypeProvider(
    const TypeRegistry& type_registry);
const absl_nonnull std::shared_ptr<LegacyRuntimeTypeProvider>&
GetLegacyRuntimeTypeProvider(const TypeRegistry& type_registry);

// Returns a memoized table of fully qualified enum values.
//
// This is populated when first requested.
std::shared_ptr<const absl::flat_hash_map<std::string, Value>>
GetEnumValueTable(const TypeRegistry& type_registry);
}  // namespace runtime_internal

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

  TypeRegistry()
      : TypeRegistry(google::protobuf::DescriptorPool::generated_pool(),
                     google::protobuf::MessageFactory::generated_factory()) {}

  TypeRegistry(const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
               google::protobuf::MessageFactory* absl_nullable message_factory);

  // Neither moveable nor copyable.
  TypeRegistry(const TypeRegistry& other) = delete;
  TypeRegistry& operator=(TypeRegistry& other) = delete;
  TypeRegistry(TypeRegistry&& other) = delete;
  TypeRegistry& operator=(TypeRegistry&& other) = delete;

  // Registers a type such that it can be accessed by name, i.e. `type(foo) ==
  // my_type`. Where `my_type` is the type being registered.
  absl::Status RegisterType(const OpaqueType& type) {
    return type_provider_.RegisterType(type);
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
  const TypeProvider& GetComposedTypeProvider() const { return type_provider_; }

 private:
  friend const runtime_internal::RuntimeTypeProvider&
  runtime_internal::GetRuntimeTypeProvider(const TypeRegistry& type_registry);
  friend const
      absl_nonnull std::shared_ptr<runtime_internal::LegacyRuntimeTypeProvider>&
      runtime_internal::GetLegacyRuntimeTypeProvider(
          const TypeRegistry& type_registry);

  friend std::shared_ptr<const absl::flat_hash_map<std::string, Value>>
  runtime_internal::GetEnumValueTable(const TypeRegistry& type_registry);

  std::shared_ptr<const absl::flat_hash_map<std::string, Value>>
  GetEnumValueTable() const;

  runtime_internal::RuntimeTypeProvider type_provider_;
  absl_nonnull std::shared_ptr<runtime_internal::LegacyRuntimeTypeProvider>
      legacy_type_provider_;
  absl::flat_hash_map<std::string, Enumeration> enum_types_;

  // memoized fully qualified enumerator names.
  //
  // populated when requested.
  //
  // In almost all cases, this is built once and never updated, but we can't
  // guarantee that with the current CelExpressionBuilder API.
  //
  // The cases when invalidation may occur are likely already race conditions,
  // but we provide basic thread safety to avoid issues with sanitizers.
  mutable std::shared_ptr<const absl::flat_hash_map<std::string, Value>>
      enum_value_table_ ABSL_GUARDED_BY(enum_value_table_mutex_);
  mutable absl::Mutex enum_value_table_mutex_;
};

namespace runtime_internal {
inline const RuntimeTypeProvider& GetRuntimeTypeProvider(
    const TypeRegistry& type_registry) {
  return type_registry.type_provider_;
}
inline const absl_nonnull std::shared_ptr<LegacyRuntimeTypeProvider>&
GetLegacyRuntimeTypeProvider(const TypeRegistry& type_registry) {
  return type_registry.legacy_type_provider_;
}
inline std::shared_ptr<const absl::flat_hash_map<std::string, Value>>
GetEnumValueTable(const TypeRegistry& type_registry) {
  return type_registry.GetEnumValueTable();
}

}  // namespace runtime_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_TYPE_REGISTRY_H_
