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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_COMPILER_CEL_EXPRESSION_BUILDER_FLAT_IMPL_H_
#define THIRD_PARTY_CEL_CPP_EVAL_COMPILER_CEL_EXPRESSION_BUILDER_FLAT_IMPL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cel/expr/checked.pb.h"
#include "cel/expr/syntax.pb.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/ast.h"
#include "eval/compiler/flat_expr_builder.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_type_registry.h"
#include "runtime/internal/runtime_env.h"
#include "runtime/runtime_options.h"

namespace google::api::expr::runtime {

// CelExpressionBuilder implementation.
// Builds instances of CelExpressionFlatImpl.
class CelExpressionBuilderFlatImpl : public CelExpressionBuilder {
 public:
  CelExpressionBuilderFlatImpl(
      absl_nonnull std::shared_ptr<cel::runtime_internal::RuntimeEnv> env,
      const cel::RuntimeOptions& options)
      : env_(std::move(env)),
        flat_expr_builder_(env_, options, /*use_legacy_type_provider=*/true) {
    ABSL_DCHECK(env_->IsInitialized());
  }

  explicit CelExpressionBuilderFlatImpl(
      absl_nonnull std::shared_ptr<cel::runtime_internal::RuntimeEnv> env)
      : CelExpressionBuilderFlatImpl(std::move(env), cel::RuntimeOptions()) {}

  absl::StatusOr<std::unique_ptr<CelExpression>> CreateExpression(
      const cel::expr::Expr* expr,
      const cel::expr::SourceInfo* source_info) const override;

  absl::StatusOr<std::unique_ptr<CelExpression>> CreateExpression(
      const cel::expr::Expr* expr,
      const cel::expr::SourceInfo* source_info,
      std::vector<absl::Status>* warnings) const override;

  absl::StatusOr<std::unique_ptr<CelExpression>> CreateExpression(
      const cel::expr::CheckedExpr* checked_expr) const override;

  absl::StatusOr<std::unique_ptr<CelExpression>> CreateExpression(
      const cel::expr::CheckedExpr* checked_expr,
      std::vector<absl::Status>* warnings) const override;

  FlatExprBuilder& flat_expr_builder() { return flat_expr_builder_; }

  void set_container(std::string container) override {
    flat_expr_builder_.set_container(std::move(container));
  }

  // CelFunction registry. Extension function should be registered with it
  // prior to expression creation.
  CelFunctionRegistry* GetRegistry() const override {
    return &env_->legacy_function_registry;
  }

  // CEL Type registry. Provides a means to resolve the CEL built-in types to
  // CelValue instances, and to extend the set of types and enums known to
  // expressions by registering them ahead of time.
  CelTypeRegistry* GetTypeRegistry() const override {
    return &env_->legacy_type_registry;
  }

  absl::string_view container() const override {
    return flat_expr_builder_.container();
  }

 private:
  absl::StatusOr<std::unique_ptr<CelExpression>> CreateExpressionImpl(
      std::unique_ptr<cel::Ast> converted_ast,
      std::vector<absl::Status>* warnings) const;

  absl_nonnull std::shared_ptr<cel::runtime_internal::RuntimeEnv> env_;
  FlatExprBuilder flat_expr_builder_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_COMPILER_CEL_EXPRESSION_BUILDER_FLAT_IMPL_H_
