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

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/ast.h"
#include "eval/compiler/flat_expr_builder.h"
#include "eval/public/cel_expression.h"
#include "runtime/runtime_options.h"

namespace google::api::expr::runtime {

// CelExpressionBuilder implementation.
// Builds instances of CelExpressionFlatImpl.
class CelExpressionBuilderFlatImpl : public CelExpressionBuilder {
 public:
  explicit CelExpressionBuilderFlatImpl(const cel::RuntimeOptions& options)
      : flat_expr_builder_(GetRegistry()->InternalGetRegistry(),
                           *GetTypeRegistry(), options) {}

  CelExpressionBuilderFlatImpl()
      : flat_expr_builder_(GetRegistry()->InternalGetRegistry(),
                           *GetTypeRegistry()) {}

  absl::StatusOr<std::unique_ptr<CelExpression>> CreateExpression(
      const google::api::expr::v1alpha1::Expr* expr,
      const google::api::expr::v1alpha1::SourceInfo* source_info) const override;

  absl::StatusOr<std::unique_ptr<CelExpression>> CreateExpression(
      const google::api::expr::v1alpha1::Expr* expr,
      const google::api::expr::v1alpha1::SourceInfo* source_info,
      std::vector<absl::Status>* warnings) const override;

  absl::StatusOr<std::unique_ptr<CelExpression>> CreateExpression(
      const google::api::expr::v1alpha1::CheckedExpr* checked_expr) const override;

  absl::StatusOr<std::unique_ptr<CelExpression>> CreateExpression(
      const google::api::expr::v1alpha1::CheckedExpr* checked_expr,
      std::vector<absl::Status>* warnings) const override;

  FlatExprBuilder& flat_expr_builder() { return flat_expr_builder_; }

  void set_container(std::string container) override {
    CelExpressionBuilder::set_container(container);
    flat_expr_builder_.set_container(std::move(container));
  }

 private:
  absl::StatusOr<std::unique_ptr<CelExpression>> CreateExpressionImpl(
      std::unique_ptr<cel::Ast> converted_ast,
      std::vector<absl::Status>* warnings) const;

  FlatExprBuilder flat_expr_builder_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_COMPILER_CEL_EXPRESSION_BUILDER_FLAT_IMPL_H_
