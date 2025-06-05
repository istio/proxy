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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_RUNTIME_IMPL_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_RUNTIME_IMPL_H_

#include <memory>

#include "absl/status/statusor.h"
#include "base/ast.h"
#include "base/type_provider.h"
#include "common/native_type.h"
#include "eval/compiler/flat_expr_builder.h"
#include "internal/well_known_types.h"
#include "runtime/function_registry.h"
#include "runtime/runtime.h"
#include "runtime/runtime_options.h"
#include "runtime/type_registry.h"

namespace cel::runtime_internal {

class RuntimeImpl : public Runtime {
 public:
  struct Environment {
    TypeRegistry type_registry;
    FunctionRegistry function_registry;
    well_known_types::Reflection well_known_types;
  };

  explicit RuntimeImpl(const RuntimeOptions& options)
      : environment_(std::make_shared<Environment>()),
        expr_builder_(environment_->function_registry,
                      environment_->type_registry, options) {}

  TypeRegistry& type_registry() { return environment_->type_registry; }
  const TypeRegistry& type_registry() const {
    return environment_->type_registry;
  }

  FunctionRegistry& function_registry() {
    return environment_->function_registry;
  }
  const FunctionRegistry& function_registry() const {
    return environment_->function_registry;
  }

  well_known_types::Reflection& well_known_types() {
    return environment_->well_known_types;
  }
  const well_known_types::Reflection& well_known_types() const {
    return environment_->well_known_types;
  }

  // implement Runtime
  absl::StatusOr<std::unique_ptr<Program>> CreateProgram(
      std::unique_ptr<Ast> ast,
      const Runtime::CreateProgramOptions& options) const final;

  absl::StatusOr<std::unique_ptr<TraceableProgram>> CreateTraceableProgram(
      std::unique_ptr<Ast> ast,
      const Runtime::CreateProgramOptions& options) const override;

  const TypeProvider& GetTypeProvider() const override {
    return environment_->type_registry.GetComposedTypeProvider();
  }

  // exposed for extensions access
  google::api::expr::runtime::FlatExprBuilder& expr_builder() {
    return expr_builder_;
  }

 private:
  NativeTypeId GetNativeTypeId() const override {
    return NativeTypeId::For<RuntimeImpl>();
  }
  // Note: this is mutable, but should only be accessed in a const context after
  // building is complete.
  //
  // This is used to keep alive the registries while programs reference them.
  std::shared_ptr<Environment> environment_;
  google::api::expr::runtime::FlatExprBuilder expr_builder_;
};

// Exposed for testing to validate program is recursively planned.
//
// Uses dynamic_casts to test.
bool TestOnly_IsRecursiveImpl(const Program* program);

}  // namespace cel::runtime_internal

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_RUNTIME_IMPL_H_
