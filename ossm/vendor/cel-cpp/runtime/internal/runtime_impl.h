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
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/statusor.h"
#include "base/ast.h"
#include "base/type_provider.h"
#include "common/native_type.h"
#include "eval/compiler/flat_expr_builder.h"
#include "internal/well_known_types.h"
#include "runtime/function_registry.h"
#include "runtime/internal/runtime_env.h"
#include "runtime/runtime.h"
#include "runtime/runtime_options.h"
#include "runtime/type_registry.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::runtime_internal {

class RuntimeImpl : public Runtime {
 public:
  using Environment = RuntimeEnv;

  RuntimeImpl(absl_nonnull std::shared_ptr<Environment> environment,
              const RuntimeOptions& options)
      : environment_(std::move(environment)),
        expr_builder_(environment_, options) {
    ABSL_DCHECK(environment_->well_known_types.IsInitialized());
  }

  TypeRegistry& type_registry() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return environment_->type_registry;
  }
  const TypeRegistry& type_registry() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return environment_->type_registry;
  }

  FunctionRegistry& function_registry() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return environment_->function_registry;
  }
  const FunctionRegistry& function_registry() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return environment_->function_registry;
  }

  const well_known_types::Reflection& well_known_types() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return environment_->well_known_types;
  }

  Environment& environment() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return *environment_;
  }
  const Environment& environment() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return *environment_;
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

  const google::protobuf::DescriptorPool* absl_nonnull GetDescriptorPool()
      const override {
    return environment_->descriptor_pool.get();
  }

  google::protobuf::MessageFactory* absl_nonnull GetMessageFactory() const override {
    return environment_->MutableMessageFactory();
  }

  // exposed for extensions access
  google::api::expr::runtime::FlatExprBuilder& expr_builder()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
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
