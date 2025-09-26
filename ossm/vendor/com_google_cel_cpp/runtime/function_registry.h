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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_FUNCTION_REGISTRY_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_FUNCTION_REGISTRY_H_

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/node_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/function_descriptor.h"
#include "common/kind.h"
#include "runtime/function.h"
#include "runtime/function_overload_reference.h"
#include "runtime/function_provider.h"

namespace cel {

// FunctionRegistry manages binding builtin or custom CEL functions to
// implementations.
//
// The registry is consulted during program planning to tie overload candidates
// to the CEL function in the AST getting planned.
//
// The registry takes ownership of the cel::Function objects -- the registry
// must outlive any program planned using it.
//
// This class is move-only.
class FunctionRegistry {
 public:
  // Represents a single overload for a lazily provided function.
  struct LazyOverload {
    const cel::FunctionDescriptor& descriptor;
    const cel::runtime_internal::FunctionProvider& provider;
  };

  FunctionRegistry() = default;

  // Move-only
  FunctionRegistry(FunctionRegistry&&) = default;
  FunctionRegistry& operator=(FunctionRegistry&&) = default;

  // Register a function implementation for the given descriptor.
  // Function registration should be performed prior to CelExpression creation.
  absl::Status Register(const cel::FunctionDescriptor& descriptor,
                        std::unique_ptr<cel::Function> implementation);

  // Register a lazily provided function.
  // Internally, the registry binds a FunctionProvider that provides an overload
  // at evaluation time by resolving against the overloads provided by an
  // implementation of cel::ActivationInterface.
  absl::Status RegisterLazyFunction(const cel::FunctionDescriptor& descriptor);

  // Find subset of cel::Function implementations that match overload conditions
  // As types may not be available during expression compilation,
  // further narrowing of this subset will happen at evaluation stage.
  //
  // name - the name of CEL function (as distinct from overload ID);
  // receiver_style - indicates whether function has receiver style;
  // types - argument types. If  type is not known during compilation,
  // cel::Kind::kAny should be passed.
  //
  // Results refer to underlying registry entries by reference. Results are
  // invalid after the registry is deleted.
  std::vector<cel::FunctionOverloadReference> FindStaticOverloads(
      absl::string_view name, bool receiver_style,
      absl::Span<const cel::Kind> types) const;

  std::vector<cel::FunctionOverloadReference> FindStaticOverloadsByArity(
      absl::string_view name, bool receiver_style, size_t arity) const;

  // Find subset of cel::Function providers that match overload conditions.
  // As types may not be available during expression compilation,
  // further narrowing of this subset will happen at evaluation stage.
  //
  // name - the name of CEL function (as distinct from overload ID);
  // receiver_style - indicates whether function has receiver style;
  // types - argument types. If  type is not known during compilation,
  // cel::Kind::kAny should be passed.
  //
  // Results refer to underlying registry entries by reference. Results are
  // invalid after the registry is deleted.
  std::vector<LazyOverload> FindLazyOverloads(
      absl::string_view name, bool receiver_style,
      absl::Span<const cel::Kind> types) const;

  std::vector<LazyOverload> FindLazyOverloadsByArity(absl::string_view name,
                                                     bool receiver_style,
                                                     size_t arity) const;

  // Retrieve list of registered function descriptors. This includes both
  // static and lazy functions.
  absl::node_hash_map<std::string, std::vector<const cel::FunctionDescriptor*>>
  ListFunctions() const;

 private:
  struct StaticFunctionEntry {
    StaticFunctionEntry(const cel::FunctionDescriptor& descriptor,
                        std::unique_ptr<cel::Function> impl)
        : descriptor(std::make_unique<cel::FunctionDescriptor>(descriptor)),
          implementation(std::move(impl)) {}

    // Extra indirection needed to preserve pointer stability for the
    // descriptors.
    std::unique_ptr<cel::FunctionDescriptor> descriptor;
    std::unique_ptr<cel::Function> implementation;
  };

  struct LazyFunctionEntry {
    LazyFunctionEntry(
        const cel::FunctionDescriptor& descriptor,
        std::unique_ptr<cel::runtime_internal::FunctionProvider> provider)
        : descriptor(std::make_unique<cel::FunctionDescriptor>(descriptor)),
          function_provider(std::move(provider)) {}

    // Extra indirection needed to preserve pointer stability for the
    // descriptors.
    std::unique_ptr<cel::FunctionDescriptor> descriptor;
    std::unique_ptr<cel::runtime_internal::FunctionProvider> function_provider;
  };

  struct RegistryEntry {
    std::vector<StaticFunctionEntry> static_overloads;
    std::vector<LazyFunctionEntry> lazy_overloads;
  };

  // Returns whether the descriptor is registered either as a lazy function or
  // as a static function.
  bool DescriptorRegistered(const cel::FunctionDescriptor& descriptor) const;

  // Returns true if after adding this function, the rule "a non-strict
  // function should have only a single overload" will be preserved.
  bool ValidateNonStrictOverload(
      const cel::FunctionDescriptor& descriptor) const;

  // indexed by function name (not type checker overload id).
  absl::flat_hash_map<std::string, RegistryEntry> functions_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_FUNCTION_REGISTRY_H_
