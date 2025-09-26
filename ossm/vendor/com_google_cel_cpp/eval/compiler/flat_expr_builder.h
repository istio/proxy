/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef THIRD_PARTY_CEL_CPP_EVAL_COMPILER_FLAT_EXPR_BUILDER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_COMPILER_FLAT_EXPR_BUILDER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/ast.h"
#include "base/type_provider.h"
#include "common/value.h"
#include "eval/compiler/flat_expr_builder_extensions.h"
#include "eval/eval/evaluator_core.h"
#include "runtime/function_registry.h"
#include "runtime/internal/runtime_env.h"
#include "runtime/runtime_issue.h"
#include "runtime/runtime_options.h"
#include "runtime/type_registry.h"

namespace google::api::expr::runtime {

// CelExpressionBuilder implementation.
// Builds instances of CelExpressionFlatImpl.
class FlatExprBuilder {
 public:
  FlatExprBuilder(
      absl_nonnull std::shared_ptr<const cel::runtime_internal::RuntimeEnv> env,
      const cel::RuntimeOptions& options, bool use_legacy_type_provider = false)
      : env_(std::move(env)),
        options_(options),
        container_(options.container),
        function_registry_(env_->function_registry),
        type_registry_(env_->type_registry),
        use_legacy_type_provider_(use_legacy_type_provider) {}

  FlatExprBuilder(
      absl_nonnull std::shared_ptr<const cel::runtime_internal::RuntimeEnv> env,
      const cel::FunctionRegistry& function_registry,
      const cel::TypeRegistry& type_registry,
      const cel::RuntimeOptions& options, bool use_legacy_type_provider = false)
      : env_(std::move(env)),
        options_(options),
        container_(options.container),
        function_registry_(function_registry),
        type_registry_(type_registry),
        use_legacy_type_provider_(use_legacy_type_provider) {}

  void AddAstTransform(std::unique_ptr<AstTransform> transform) {
    ast_transforms_.push_back(std::move(transform));
  }

  void AddProgramOptimizer(ProgramOptimizerFactory optimizer) {
    program_optimizers_.push_back(std::move(optimizer));
  }

  void set_container(std::string container) {
    container_ = std::move(container);
  }

  absl::string_view container() const { return container_; }

  // TODO(uncreated-issue/45): Add overload for cref AST. At the moment, all the users
  // can pass ownership of a freshly converted AST.
  absl::StatusOr<FlatExpression> CreateExpressionImpl(
      std::unique_ptr<cel::Ast> ast,
      std::vector<cel::RuntimeIssue>* issues) const;

  const cel::runtime_internal::RuntimeEnv& env() const { return *env_; }

  const cel::RuntimeOptions& options() const { return options_; }

  // Called by `cel::extensions::EnableOptionalTypes` to indicate that special
  // `optional_type` handling is needed.
  void enable_optional_types() { enable_optional_types_ = true; }

  bool optional_types_enabled() const { return enable_optional_types_; }

 private:
  const cel::TypeProvider& GetTypeProvider() const;

  const absl_nonnull std::shared_ptr<const cel::runtime_internal::RuntimeEnv>
      env_;

  cel::RuntimeOptions options_;
  std::string container_;
  bool enable_optional_types_ = false;
  // TODO(uncreated-issue/45): evaluate whether we should use a shared_ptr here to
  // allow built expressions to keep the registries alive.
  const cel::FunctionRegistry& function_registry_;
  const cel::TypeRegistry& type_registry_;
  bool use_legacy_type_provider_;
  std::vector<std::unique_ptr<AstTransform>> ast_transforms_;
  std::vector<ProgramOptimizerFactory> program_optimizers_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_COMPILER_FLAT_EXPR_BUILDER_H_
