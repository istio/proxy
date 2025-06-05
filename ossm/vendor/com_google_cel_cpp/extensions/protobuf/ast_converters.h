// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_AST_CONVERTERS_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_AST_CONVERTERS_H_

#include <memory>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/statusor.h"
#include "base/ast.h"
#include "base/ast_internal/expr.h"

namespace cel::extensions {
namespace internal {
// Utilities for converting protobuf CEL message types to their corresponding
// internal C++ representations.
absl::StatusOr<ast_internal::Expr> ConvertProtoExprToNative(
    const google::api::expr::v1alpha1::Expr& expr);
absl::StatusOr<ast_internal::SourceInfo> ConvertProtoSourceInfoToNative(
    const google::api::expr::v1alpha1::SourceInfo& source_info);
absl::StatusOr<ast_internal::Type> ConvertProtoTypeToNative(
    const google::api::expr::v1alpha1::Type& type);
absl::StatusOr<ast_internal::Reference> ConvertProtoReferenceToNative(
    const google::api::expr::v1alpha1::Reference& reference);

// Conversion utility for the protobuf constant CEL value representation.
absl::StatusOr<ast_internal::Constant> ConvertConstant(
    const google::api::expr::v1alpha1::Constant& constant);

}  // namespace internal

// Creates a runtime AST from a parsed-only protobuf AST.
// May return a non-ok Status if the AST is malformed (e.g. unset required
// fields).
absl::StatusOr<std::unique_ptr<Ast>> CreateAstFromParsedExpr(
    const google::api::expr::v1alpha1::Expr& expr,
    const google::api::expr::v1alpha1::SourceInfo* source_info = nullptr);
absl::StatusOr<std::unique_ptr<Ast>> CreateAstFromParsedExpr(
    const google::api::expr::v1alpha1::ParsedExpr& parsed_expr);

absl::StatusOr<google::api::expr::v1alpha1::ParsedExpr> CreateParsedExprFromAst(
    const Ast& ast);

// Creates a runtime AST from a checked protobuf AST.
// May return a non-ok Status if the AST is malformed (e.g. unset required
// fields).
absl::StatusOr<std::unique_ptr<Ast>> CreateAstFromCheckedExpr(
    const google::api::expr::v1alpha1::CheckedExpr& checked_expr);

absl::StatusOr<google::api::expr::v1alpha1::CheckedExpr> CreateCheckedExprFromAst(
    const Ast& ast);

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_AST_CONVERTERS_H_
