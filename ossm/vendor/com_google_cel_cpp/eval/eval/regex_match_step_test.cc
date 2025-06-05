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

#include "eval/eval/regex_match_step.h"

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/arena.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_options.h"
#include "internal/testing.h"
#include "parser/parser.h"

namespace google::api::expr::runtime {
namespace {

using ::absl_testing::StatusIs;
using google::api::expr::v1alpha1::CheckedExpr;
using google::api::expr::v1alpha1::Reference;
using ::testing::Eq;
using ::testing::HasSubstr;

Reference MakeMatchesStringOverload() {
  Reference reference;
  reference.add_overload_id("matches_string");
  return reference;
}

TEST(RegexMatchStep, Precompiled) {
  google::protobuf::Arena arena;
  Activation activation;
  ASSERT_OK_AND_ASSIGN(auto parsed_expr, parser::Parse("foo.matches('hello')"));
  CheckedExpr checked_expr;
  *checked_expr.mutable_expr() = parsed_expr.expr();
  *checked_expr.mutable_source_info() = parsed_expr.source_info();
  checked_expr.mutable_reference_map()->insert(
      {checked_expr.expr().id(), MakeMatchesStringOverload()});
  InterpreterOptions options;
  options.enable_regex_precompilation = true;
  auto expr_builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(expr_builder->GetRegistry(), options));
  ASSERT_OK_AND_ASSIGN(auto expr,
                       expr_builder->CreateExpression(&checked_expr));
  activation.InsertValue("foo", CelValue::CreateStringView("hello world!"));
  ASSERT_OK_AND_ASSIGN(auto result, expr->Evaluate(activation, &arena));
  EXPECT_TRUE(result.IsBool());
  EXPECT_TRUE(result.BoolOrDie());
}

TEST(RegexMatchStep, PrecompiledInvalidRegex) {
  google::protobuf::Arena arena;
  Activation activation;
  ASSERT_OK_AND_ASSIGN(auto parsed_expr, parser::Parse("foo.matches('(')"));
  CheckedExpr checked_expr;
  *checked_expr.mutable_expr() = parsed_expr.expr();
  *checked_expr.mutable_source_info() = parsed_expr.source_info();
  checked_expr.mutable_reference_map()->insert(
      {checked_expr.expr().id(), MakeMatchesStringOverload()});
  InterpreterOptions options;
  options.enable_regex_precompilation = true;
  auto expr_builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(expr_builder->GetRegistry(), options));
  EXPECT_THAT(expr_builder->CreateExpression(&checked_expr),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid_argument")));
}

TEST(RegexMatchStep, PrecompiledInvalidProgramTooLarge) {
  google::protobuf::Arena arena;
  Activation activation;
  ASSERT_OK_AND_ASSIGN(auto parsed_expr, parser::Parse("foo.matches('hello')"));
  CheckedExpr checked_expr;
  *checked_expr.mutable_expr() = parsed_expr.expr();
  *checked_expr.mutable_source_info() = parsed_expr.source_info();
  checked_expr.mutable_reference_map()->insert(
      {checked_expr.expr().id(), MakeMatchesStringOverload()});
  InterpreterOptions options;
  options.regex_max_program_size = 1;
  options.enable_regex_precompilation = true;
  auto expr_builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(expr_builder->GetRegistry(), options));
  EXPECT_THAT(expr_builder->CreateExpression(&checked_expr),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       Eq("exceeded RE2 max program size")));
}

}  // namespace
}  // namespace google::api::expr::runtime
