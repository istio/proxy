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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_AST_PROTO_H_
#define THIRD_PARTY_CEL_CPP_COMMON_AST_PROTO_H_

#include <memory>

#include "cel/expr/checked.pb.h"
#include "cel/expr/syntax.pb.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/ast.h"

namespace cel {

// Creates a runtime AST from a parsed-only protobuf AST.
// May return a non-ok Status if the AST is malformed (e.g. unset required
// fields).
absl::StatusOr<std::unique_ptr<Ast>> CreateAstFromParsedExpr(
    const cel::expr::Expr& expr,
    const cel::expr::SourceInfo* source_info = nullptr);
absl::StatusOr<std::unique_ptr<Ast>> CreateAstFromParsedExpr(
    const cel::expr::ParsedExpr& parsed_expr);

absl::Status AstToParsedExpr(const Ast& ast,
                             cel::expr::ParsedExpr* absl_nonnull out);

// Creates a runtime AST from a checked protobuf AST.
// May return a non-ok Status if the AST is malformed (e.g. unset required
// fields).
absl::StatusOr<std::unique_ptr<Ast>> CreateAstFromCheckedExpr(
    const cel::expr::CheckedExpr& checked_expr);

absl::Status AstToCheckedExpr(const Ast& ast,
                              cel::expr::CheckedExpr* absl_nonnull out);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_AST_PROTO_H_
