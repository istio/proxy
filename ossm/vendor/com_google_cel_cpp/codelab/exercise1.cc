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

#include "codelab/exercise1.h"

#include <memory>
#include <string>

#include "google/api/expr/v1alpha1/syntax.pb.h"
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

using ::google::api::expr::runtime::Activation;
using ::google::api::expr::runtime::CelValue;

// Convert the CelResult to a C++ string if it is string typed. Otherwise,
// return invalid argument error. This takes a copy to avoid lifecycle concerns
// (the evaluator may represent strings as stringviews backed by the input
// expression).
absl::StatusOr<std::string> ConvertResult(const CelValue& value) {
  if (CelValue::StringHolder inner_value; value.GetValue(&inner_value)) {
    return std::string(inner_value.value());
  } else {
    return absl::InvalidArgumentError(absl::StrCat(
        "expected string result got '", CelValue::TypeName(value.type()), "'"));
  }
}
}  // namespace

absl::StatusOr<std::string> ParseAndEvaluate(absl::string_view cel_expr) {
  // === Start Codelab ===
  // Parse the expression using ::google::api::expr::parser::Parse;
  // This will return a google::api::expr::v1alpha1::ParsedExpr message.

  // Setup a default environment for building expressions.
  // std::unique_ptr<CelExpressionBuilder> builder =
  //     CreateCelExpressionBuilder(options);

  // Register standard functions.
  // CEL_RETURN_IF_ERROR(
  //     RegisterBuiltinFunctions(builder->GetRegistry(), options));

  // The evaluator uses a proto Arena for incidental allocations during
  // evaluation.
  google::protobuf::Arena arena;
  // The activation provides variables and functions that are bound into the
  // expression environment. In this example, there's no context expected, so
  // we just provide an empty one to the evaluator.
  Activation activation;

  // Using the CelExpressionBuilder and the ParseExpr, create an execution plan
  // (google::api::expr::runtime::CelExpression), evaluate, and return the
  // result. Use the provided helper function ConvertResult to copy the value
  // for return.
  return absl::UnimplementedError("Not yet implemented");
  // === End Codelab ===
}

}  // namespace google::api::expr::codelab
