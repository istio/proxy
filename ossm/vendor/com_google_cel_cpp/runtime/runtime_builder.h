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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_RUNTIME_BUILDER_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_RUNTIME_BUILDER_H_

#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "runtime/function_registry.h"
#include "runtime/runtime.h"
#include "runtime/runtime_options.h"
#include "runtime/type_registry.h"
#include "google/protobuf/descriptor.h"

namespace cel {

// Forward declare for friend access to avoid requiring a link dependency on the
// standard implementation and some extensions.
namespace runtime_internal {
class RuntimeFriendAccess;
}  // namespace runtime_internal

class RuntimeBuilder;
absl::StatusOr<RuntimeBuilder> CreateRuntimeBuilder(
    absl::Nonnull<const google::protobuf::DescriptorPool*>, const RuntimeOptions&);

// RuntimeBuilder provides mutable accessors to configure a new runtime.
//
// Instances of this class are consumed when built.
//
// This class is move-only.
class RuntimeBuilder {
 public:
  // Move-only
  RuntimeBuilder(const RuntimeBuilder&) = delete;
  RuntimeBuilder& operator=(const RuntimeBuilder&) = delete;
  RuntimeBuilder(RuntimeBuilder&&) = default;
  RuntimeBuilder& operator=(RuntimeBuilder&&) = default;

  TypeRegistry& type_registry() { return *type_registry_; }
  FunctionRegistry& function_registry() { return *function_registry_; }

  // Return the built runtime.
  // The builder is left in an undefined state after this call and cannot be
  // reused.
  absl::StatusOr<std::unique_ptr<const Runtime>> Build() && {
    return std::move(runtime_);
  }

 private:
  friend class runtime_internal::RuntimeFriendAccess;
  friend absl::StatusOr<RuntimeBuilder> CreateRuntimeBuilder(
      absl::Nonnull<const google::protobuf::DescriptorPool*>, const RuntimeOptions&);

  // Constructor for a new runtime builder.
  //
  // It's assumed that the type registry and function registry are managed by
  // the runtime.
  //
  // CEL users should use one of the factory functions for a new builder.
  // See standard_runtime_builder_factory.h and runtime_builder_factory.h
  RuntimeBuilder(TypeRegistry& type_registry,
                 FunctionRegistry& function_registry,
                 std::unique_ptr<Runtime> runtime)
      : type_registry_(&type_registry),
        function_registry_(&function_registry),
        runtime_(std::move(runtime)) {}

  Runtime& runtime() { return *runtime_; }

  TypeRegistry* type_registry_;
  FunctionRegistry* function_registry_;
  std::unique_ptr<Runtime> runtime_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_RUNTIME_BUILDER_H_
