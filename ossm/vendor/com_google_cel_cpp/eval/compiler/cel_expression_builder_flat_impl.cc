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

#include "eval/compiler/cel_expression_builder_flat_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "cel/expr/checked.pb.h"
#include "cel/expr/syntax.pb.h"
#include "absl/base/macros.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/ast.h"
#include "common/native_type.h"
#include "eval/eval/cel_expression_flat_impl.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/public/cel_expression.h"
#include "extensions/protobuf/ast_converters.h"
#include "internal/status_macros.h"
#include "runtime/runtime_issue.h"

namespace google::api::expr::runtime {

using ::cel::Ast;
using ::cel::RuntimeIssue;
using ::cel::expr::CheckedExpr;
using ::cel::expr::Expr;  // NOLINT: adjusted in OSS
using ::cel::expr::SourceInfo;

absl::StatusOr<std::unique_ptr<CelExpression>>
CelExpressionBuilderFlatImpl::CreateExpression(
    const Expr* expr, const SourceInfo* source_info,
    std::vector<absl::Status>* warnings) const {
  ABSL_ASSERT(expr != nullptr);
  CEL_ASSIGN_OR_RETURN(
      std::unique_ptr<Ast> converted_ast,
      cel::extensions::CreateAstFromParsedExpr(*expr, source_info));
  return CreateExpressionImpl(std::move(converted_ast), warnings);
}

absl::StatusOr<std::unique_ptr<CelExpression>>
CelExpressionBuilderFlatImpl::CreateExpression(
    const Expr* expr, const SourceInfo* source_info) const {
  return CreateExpression(expr, source_info,
                          /*warnings=*/nullptr);
}

absl::StatusOr<std::unique_ptr<CelExpression>>
CelExpressionBuilderFlatImpl::CreateExpression(
    const CheckedExpr* checked_expr,
    std::vector<absl::Status>* warnings) const {
  ABSL_ASSERT(checked_expr != nullptr);
  CEL_ASSIGN_OR_RETURN(
      std::unique_ptr<Ast> converted_ast,
      cel::extensions::CreateAstFromCheckedExpr(*checked_expr));

  return CreateExpressionImpl(std::move(converted_ast), warnings);
}

absl::StatusOr<std::unique_ptr<CelExpression>>
CelExpressionBuilderFlatImpl::CreateExpression(
    const CheckedExpr* checked_expr) const {
  return CreateExpression(checked_expr, /*warnings=*/nullptr);
}

absl::StatusOr<std::unique_ptr<CelExpression>>
CelExpressionBuilderFlatImpl::CreateExpressionImpl(
    std::unique_ptr<Ast> converted_ast,
    std::vector<absl::Status>* warnings) const {
  std::vector<RuntimeIssue> issues;
  auto* issues_ptr = (warnings != nullptr) ? &issues : nullptr;

  CEL_ASSIGN_OR_RETURN(FlatExpression impl,
                       flat_expr_builder_.CreateExpressionImpl(
                           std::move(converted_ast), issues_ptr));

  if (issues_ptr != nullptr) {
    for (const auto& issue : issues) {
      warnings->push_back(issue.ToStatus());
    }
  }
  if (flat_expr_builder_.options().max_recursion_depth != 0 &&
      !impl.subexpressions().empty() &&
      // mainline expression is exactly one recursive step.
      impl.subexpressions().front().size() == 1 &&
      impl.subexpressions().front().front()->GetNativeTypeId() ==
          cel::NativeTypeId::For<WrappedDirectStep>()) {
    return CelExpressionRecursiveImpl::Create(env_, std::move(impl));
  }

  return std::make_unique<CelExpressionFlatImpl>(env_, std::move(impl));
}

}  // namespace google::api::expr::runtime
