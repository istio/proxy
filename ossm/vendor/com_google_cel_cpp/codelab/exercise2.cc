// Copyright 2021 Google LLC
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

#include "codelab/exercise2.h"

#include <memory>
#include <string>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/rpc/context/attribute_context.pb.h"
#include "google/protobuf/arena.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "internal/status_macros.h"
#include "parser/parser.h"

namespace google::api::expr::codelab {
namespace {

using ::google::api::expr::v1alpha1::ParsedExpr;
using ::google::api::expr::parser::Parse;
using ::google::api::expr::runtime::Activation;
using ::google::api::expr::runtime::CelError;
using ::google::api::expr::runtime::CelExpression;
using ::google::api::expr::runtime::CelExpressionBuilder;
using ::google::api::expr::runtime::CelValue;
using ::google::api::expr::runtime::CreateCelExpressionBuilder;
using ::google::api::expr::runtime::InterpreterOptions;
using ::google::api::expr::runtime::RegisterBuiltinFunctions;
using ::google::rpc::context::AttributeContext;

// Parse a cel expression and evaluate it against the given activation and
// arena.
absl::StatusOr<bool> ParseAndEvaluate(absl::string_view cel_expr,
                                      const Activation& activation,
                                      google::protobuf::Arena* arena) {
  CEL_ASSIGN_OR_RETURN(ParsedExpr parsed_expr, Parse(cel_expr));

  // Setup a default environment for building expressions.
  InterpreterOptions options;
  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(options);
  CEL_RETURN_IF_ERROR(
      RegisterBuiltinFunctions(builder->GetRegistry(), options));

  CEL_ASSIGN_OR_RETURN(std::unique_ptr<CelExpression> expression_plan,
                       builder->CreateExpression(&parsed_expr.expr(),
                                                 &parsed_expr.source_info()));

  CEL_ASSIGN_OR_RETURN(CelValue result,
                       expression_plan->Evaluate(activation, arena));

  if (bool value; result.GetValue(&value)) {
    return value;
  } else if (const CelError * value; result.GetValue(&value)) {
    return *value;
  } else {
    return absl::InvalidArgumentError(absl::StrCat(
        "expected 'bool' result got '", result.DebugString(), "'"));
  }
}
}  // namespace

absl::StatusOr<bool> ParseAndEvaluate(absl::string_view cel_expr,
                                      bool bool_var) {
  Activation activation;
  google::protobuf::Arena arena;
  // === Start Codelab ===
  // Update the activation to bind the bool argument to 'bool_var'
  // === End Codelab ===

  return ParseAndEvaluate(cel_expr, activation, &arena);
}

absl::StatusOr<bool> ParseAndEvaluate(absl::string_view cel_expr,
                                      const AttributeContext& context) {
  Activation activation;
  google::protobuf::Arena arena;
  // === Start Codelab ===
  // Update the activation to bind the AttributeContext.
  // === End Codelab ===

  return ParseAndEvaluate(cel_expr, activation, &arena);
}

}  // namespace google::api::expr::codelab
