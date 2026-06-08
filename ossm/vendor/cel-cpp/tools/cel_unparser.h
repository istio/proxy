// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Provides an unparsing utility that converts an AST back into
// a human readable format.
//
// Input to the unparser is the proto AST (Expr, CheckedExpr, or ParsedExpr).
// The unparser does not do any checks to see if the ParsedExpr is syntactically
// or semantically correct but does checks enough to prevent its crash and might
// return errors in such cases.

#ifndef THIRD_PARTY_CEL_CPP_TOOLS_UNPARSER_H_
#define THIRD_PARTY_CEL_CPP_TOOLS_UNPARSER_H_

#include <string>

#include "cel/expr/checked.pb.h"
#include "cel/expr/syntax.pb.h"
#include "absl/base/attributes.h"
#include "absl/status/statusor.h"

namespace google::api::expr {

// Unparses the given expression into a human readable cel expression.
ABSL_DEPRECATED(
    "Use Unparse(ParsedExpr) to ensure proper unparsing of all CEL "
    "expressions. Note, ParserOptions.add_macro_calls must be set to true "
    "for full fidelity unparsing.")
absl::StatusOr<std::string> Unparse(
    const cel::expr::Expr& expr,
    const cel::expr::SourceInfo* source_info = nullptr);

// Unparses the ParsedExpr value to a human-readable string.
//
// For the best results ensure that the expression is parsed with
// ParserOptions.add_macro_calls = true.
absl::StatusOr<std::string> Unparse(
    const cel::expr::ParsedExpr& parsed_expr);

// Unparses the CheckedExpr value to a human-readable string.
//
// For the best results ensure that the expression is parsed with
// ParserOptions.add_macro_calls = true.
absl::StatusOr<std::string> Unparse(
    const cel::expr::CheckedExpr& checked_expr);

}  // namespace google::api::expr

#endif  // THIRD_PARTY_CEL_CPP_TOOLS_UNPARSER_H_
