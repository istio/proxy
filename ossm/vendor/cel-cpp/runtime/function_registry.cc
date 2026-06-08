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

#include "runtime/function_registry.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/node_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "common/function_descriptor.h"
#include "common/kind.h"
#include "runtime/activation_interface.h"
#include "runtime/function.h"
#include "runtime/function_overload_reference.h"
#include "runtime/function_provider.h"

namespace cel {
namespace {

// Impl for simple provider that looks up functions in an activation function
// registry.
class ActivationFunctionProviderImpl
    : public cel::runtime_internal::FunctionProvider {
 public:
  ActivationFunctionProviderImpl() = default;

  absl::StatusOr<absl::optional<cel::FunctionOverloadReference>> GetFunction(
      const cel::FunctionDescriptor& descriptor,
      const cel::ActivationInterface& activation) const override {
    std::vector<cel::FunctionOverloadReference> overloads =
        activation.FindFunctionOverloads(descriptor.name());

    absl::optional<cel::FunctionOverloadReference> matching_overload =
        absl::nullopt;

    for (const auto& overload : overloads) {
      if (overload.descriptor.ShapeMatches(descriptor)) {
        if (matching_overload.has_value()) {
          return absl::Status(absl::StatusCode::kInvalidArgument,
                              "Couldn't resolve function.");
        }
        matching_overload.emplace(overload);
      }
    }

    return matching_overload;
  }
};

// Create a CelFunctionProvider that just looks up the functions inserted in the
// Activation. This is a convenience implementation for a simple, common
// use-case.
std::unique_ptr<cel::runtime_internal::FunctionProvider>
CreateActivationFunctionProvider() {
  return std::make_unique<ActivationFunctionProviderImpl>();
}

}  // namespace

absl::Status FunctionRegistry::Register(
    const cel::FunctionDescriptor& descriptor,
    std::unique_ptr<cel::Function> implementation) {
  if (DescriptorRegistered(descriptor)) {
    return absl::Status(
        absl::StatusCode::kAlreadyExists,
        "CelFunction with specified parameters already registered");
  }
  if (!ValidateNonStrictOverload(descriptor)) {
    return absl::Status(absl::StatusCode::kAlreadyExists,
                        "Only one overload is allowed for non-strict function");
  }

  auto& overloads = functions_[descriptor.name()];
  overloads.static_overloads.push_back(
      StaticFunctionEntry(descriptor, std::move(implementation)));
  return absl::OkStatus();
}

absl::Status FunctionRegistry::RegisterLazyFunction(
    const cel::FunctionDescriptor& descriptor) {
  if (DescriptorRegistered(descriptor)) {
    return absl::Status(
        absl::StatusCode::kAlreadyExists,
        "CelFunction with specified parameters already registered");
  }
  if (!ValidateNonStrictOverload(descriptor)) {
    return absl::Status(absl::StatusCode::kAlreadyExists,
                        "Only one overload is allowed for non-strict function");
  }
  auto& overloads = functions_[descriptor.name()];

  overloads.lazy_overloads.push_back(
      LazyFunctionEntry(descriptor, CreateActivationFunctionProvider()));

  return absl::OkStatus();
}

std::vector<cel::FunctionOverloadReference>
FunctionRegistry::FindStaticOverloads(absl::string_view name,
                                      bool receiver_style,
                                      absl::Span<const cel::Kind> types) const {
  std::vector<cel::FunctionOverloadReference> matched_funcs;

  auto overloads = functions_.find(name);
  if (overloads == functions_.end()) {
    return matched_funcs;
  }

  for (const auto& overload : overloads->second.static_overloads) {
    if (overload.descriptor->ShapeMatches(receiver_style, types)) {
      matched_funcs.push_back({*overload.descriptor, *overload.implementation});
    }
  }

  return matched_funcs;
}

std::vector<cel::FunctionOverloadReference>
FunctionRegistry::FindStaticOverloadsByArity(absl::string_view name,
                                             bool receiver_style,
                                             size_t arity) const {
  std::vector<cel::FunctionOverloadReference> matched_funcs;

  auto overloads = functions_.find(name);
  if (overloads == functions_.end()) {
    return matched_funcs;
  }

  for (const auto& overload : overloads->second.static_overloads) {
    if (overload.descriptor->receiver_style() == receiver_style &&
        overload.descriptor->types().size() == arity) {
      matched_funcs.push_back({*overload.descriptor, *overload.implementation});
    }
  }

  return matched_funcs;
}

std::vector<FunctionRegistry::LazyOverload> FunctionRegistry::FindLazyOverloads(
    absl::string_view name, bool receiver_style,
    absl::Span<const cel::Kind> types) const {
  std::vector<FunctionRegistry::LazyOverload> matched_funcs;

  auto overloads = functions_.find(name);
  if (overloads == functions_.end()) {
    return matched_funcs;
  }

  for (const auto& entry : overloads->second.lazy_overloads) {
    if (entry.descriptor->ShapeMatches(receiver_style, types)) {
      matched_funcs.push_back({*entry.descriptor, *entry.function_provider});
    }
  }

  return matched_funcs;
}

std::vector<FunctionRegistry::LazyOverload>
FunctionRegistry::FindLazyOverloadsByArity(absl::string_view name,
                                           bool receiver_style,
                                           size_t arity) const {
  std::vector<FunctionRegistry::LazyOverload> matched_funcs;

  auto overloads = functions_.find(name);
  if (overloads == functions_.end()) {
    return matched_funcs;
  }

  for (const auto& entry : overloads->second.lazy_overloads) {
    if (entry.descriptor->receiver_style() == receiver_style &&
        entry.descriptor->types().size() == arity) {
      matched_funcs.push_back({*entry.descriptor, *entry.function_provider});
    }
  }

  return matched_funcs;
}

absl::node_hash_map<std::string, std::vector<const cel::FunctionDescriptor*>>
FunctionRegistry::ListFunctions() const {
  absl::node_hash_map<std::string, std::vector<const cel::FunctionDescriptor*>>
      descriptor_map;

  for (const auto& entry : functions_) {
    std::vector<const cel::FunctionDescriptor*> descriptors;
    const RegistryEntry& function_entry = entry.second;
    descriptors.reserve(function_entry.static_overloads.size() +
                        function_entry.lazy_overloads.size());
    for (const auto& entry : function_entry.static_overloads) {
      descriptors.push_back(entry.descriptor.get());
    }
    for (const auto& entry : function_entry.lazy_overloads) {
      descriptors.push_back(entry.descriptor.get());
    }
    descriptor_map[entry.first] = std::move(descriptors);
  }

  return descriptor_map;
}

bool FunctionRegistry::DescriptorRegistered(
    const cel::FunctionDescriptor& descriptor) const {
  auto overloads = functions_.find(descriptor.name());
  if (overloads == functions_.end()) {
    return false;
  }
  const RegistryEntry& entry = overloads->second;
  for (const auto& static_ovl : entry.static_overloads) {
    if (static_ovl.descriptor->ShapeMatches(descriptor)) {
      return true;
    }
  }
  for (const auto& lazy_ovl : entry.lazy_overloads) {
    if (lazy_ovl.descriptor->ShapeMatches(descriptor)) {
      return true;
    }
  }
  return false;
}

bool FunctionRegistry::ValidateNonStrictOverload(
    const cel::FunctionDescriptor& descriptor) const {
  auto overloads = functions_.find(descriptor.name());
  if (overloads == functions_.end()) {
    return true;
  }
  const RegistryEntry& entry = overloads->second;
  if (!descriptor.is_strict()) {
    // If the newly added overload is a non-strict function, we require that
    // there are no other overloads, which is not possible here.
    return false;
  }
  // If the newly added overload is a strict function, we need to make sure
  // that no previous overloads are registered non-strict. If the list of
  // overload is not empty, we only need to check the first overload. This is
  // because if the first overload is strict, other overloads must also be
  // strict by the rule.
  return (entry.static_overloads.empty() ||
          entry.static_overloads[0].descriptor->is_strict()) &&
         (entry.lazy_overloads.empty() ||
          entry.lazy_overloads[0].descriptor->is_strict());
}

}  // namespace cel
