// Copyright 2023 Google LLC
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

#include "eval/compiler/regex_precompilation_optimization.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "cel/expr/checked.pb.h"
#include "cel/expr/syntax.pb.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "common/ast/ast_impl.h"
#include "eval/compiler/cel_expression_builder_flat_impl.h"
#include "eval/compiler/constant_folding.h"
#include "eval/compiler/flat_expr_builder.h"
#include "eval/compiler/flat_expr_builder_extensions.h"
#include "eval/compiler/resolver.h"
#include "eval/eval/evaluator_core.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_type_registry.h"
#include "eval/public/cel_value.h"
#include "internal/testing.h"
#include "parser/parser.h"
#include "runtime/internal/issue_collector.h"
#include "runtime/internal/runtime_env.h"
#include "runtime/internal/runtime_env_testing.h"
#include "runtime/runtime_issue.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {
namespace {

using ::cel::RuntimeIssue;
using ::cel::runtime_internal::IssueCollector;
using ::cel::runtime_internal::NewTestingRuntimeEnv;
using ::cel::runtime_internal::RuntimeEnv;
using ::google::api::expr::parser::Parse;
using ::testing::ElementsAre;

namespace exprpb = cel::expr;

class RegexPrecompilationExtensionTest : public testing::TestWithParam<bool> {
 public:
  RegexPrecompilationExtensionTest()
      : env_(NewTestingRuntimeEnv()),
        builder_(env_),
        type_registry_(*builder_.GetTypeRegistry()),
        function_registry_(*builder_.GetRegistry()),
        resolver_("", function_registry_.InternalGetRegistry(),
                  type_registry_.InternalGetModernRegistry(),
                  type_registry_.GetTypeProvider()),
        issue_collector_(RuntimeIssue::Severity::kError) {
    if (EnableRecursivePlanning()) {
      options_.max_recursion_depth = -1;
      options_.enable_recursive_tracing = true;
    }
    options_.enable_regex = true;
    options_.regex_max_program_size = 100;
    options_.enable_regex_precompilation = true;
    runtime_options_ = ConvertToRuntimeOptions(options_);
  }

  void SetUp() override {
    ASSERT_OK(RegisterBuiltinFunctions(&function_registry_, options_));
  }

  bool EnableRecursivePlanning() { return GetParam(); }

 protected:
  CelEvaluationListener RecordStringValues() {
    return [this](int64_t, const CelValue& value, google::protobuf::Arena*) {
      if (value.IsString()) {
        string_values_.push_back(std::string(value.StringOrDie().value()));
      }
      return absl::OkStatus();
    };
  }

  absl_nonnull std::shared_ptr<RuntimeEnv> env_;
  CelExpressionBuilderFlatImpl builder_;
  CelTypeRegistry& type_registry_;
  CelFunctionRegistry& function_registry_;
  InterpreterOptions options_;
  cel::RuntimeOptions runtime_options_;
  Resolver resolver_;
  IssueCollector issue_collector_;
  std::vector<std::string> string_values_;
};

TEST_P(RegexPrecompilationExtensionTest, SmokeTest) {
  ProgramOptimizerFactory factory =
      CreateRegexPrecompilationExtension(options_.regex_max_program_size);
  ExecutionPath path;
  ProgramBuilder program_builder;
  cel::ast_internal::AstImpl ast_impl;
  ast_impl.set_is_checked(true);
  std::shared_ptr<google::protobuf::Arena> arena;
  PlannerContext context(env_, resolver_, runtime_options_,
                         type_registry_.GetTypeProvider(), issue_collector_,
                         program_builder, arena);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ProgramOptimizer> optimizer,
                       factory(context, ast_impl));
}

TEST_P(RegexPrecompilationExtensionTest, OptimizeableExpression) {
  builder_.flat_expr_builder().AddProgramOptimizer(
      CreateRegexPrecompilationExtension(options_.regex_max_program_size));

  ASSERT_OK_AND_ASSIGN(exprpb::ParsedExpr parsed_expr,
                       Parse("input.matches(r'[a-zA-Z]+[0-9]*')"));

  // Fake reference information for the matches call.
  exprpb::CheckedExpr expr;
  expr.mutable_expr()->Swap(parsed_expr.mutable_expr());
  expr.mutable_source_info()->Swap(parsed_expr.mutable_source_info());
  (*expr.mutable_reference_map())[2].add_overload_id("matches_string");

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelExpression> plan,
                       builder_.CreateExpression(&expr));

  Activation activation;
  google::protobuf::Arena arena;
  activation.InsertValue("input", CelValue::CreateStringView("input123"));

  ASSERT_OK(plan->Trace(activation, &arena, RecordStringValues()));
  EXPECT_THAT(string_values_, ElementsAre("input123"));
}

TEST_P(RegexPrecompilationExtensionTest, OptimizeParsedExpr) {
  builder_.flat_expr_builder().AddProgramOptimizer(
      CreateRegexPrecompilationExtension(options_.regex_max_program_size));

  ASSERT_OK_AND_ASSIGN(exprpb::ParsedExpr expr,
                       Parse("input.matches(r'[a-zA-Z]+[0-9]*')"));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<CelExpression> plan,
      builder_.CreateExpression(&expr.expr(), &expr.source_info()));

  Activation activation;
  google::protobuf::Arena arena;
  activation.InsertValue("input", CelValue::CreateStringView("input123"));

  ASSERT_OK(plan->Trace(activation, &arena, RecordStringValues()));
  EXPECT_THAT(string_values_, ElementsAre("input123"));
}

TEST_P(RegexPrecompilationExtensionTest, DoesNotOptimizeNonConstRegex) {
  builder_.flat_expr_builder().AddProgramOptimizer(
      CreateRegexPrecompilationExtension(options_.regex_max_program_size));

  ASSERT_OK_AND_ASSIGN(exprpb::ParsedExpr parsed_expr,
                       Parse("input.matches(input_re)"));

  // Fake reference information for the matches call.
  exprpb::CheckedExpr expr;
  expr.mutable_expr()->Swap(parsed_expr.mutable_expr());
  expr.mutable_source_info()->Swap(parsed_expr.mutable_source_info());
  (*expr.mutable_reference_map())[2].add_overload_id("matches_string");

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelExpression> plan,
                       builder_.CreateExpression(&expr));

  Activation activation;
  google::protobuf::Arena arena;
  activation.InsertValue("input", CelValue::CreateStringView("input123"));
  activation.InsertValue("input_re", CelValue::CreateStringView("input_re"));

  ASSERT_OK(plan->Trace(activation, &arena, RecordStringValues()));
  EXPECT_THAT(string_values_, ElementsAre("input123", "input_re"));
}

TEST_P(RegexPrecompilationExtensionTest, DoesNotOptimizeCompoundExpr) {
  builder_.flat_expr_builder().AddProgramOptimizer(
      CreateRegexPrecompilationExtension(options_.regex_max_program_size));

  ASSERT_OK_AND_ASSIGN(exprpb::ParsedExpr parsed_expr,
                       Parse("input.matches('abc' + 'def')"));

  // Fake reference information for the matches call.
  exprpb::CheckedExpr expr;
  expr.mutable_expr()->Swap(parsed_expr.mutable_expr());
  expr.mutable_source_info()->Swap(parsed_expr.mutable_source_info());
  (*expr.mutable_reference_map())[2].add_overload_id("matches_string");

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelExpression> plan,
                       builder_.CreateExpression(&expr));

  Activation activation;
  google::protobuf::Arena arena;
  activation.InsertValue("input", CelValue::CreateStringView("input123"));

  ASSERT_OK(plan->Trace(activation, &arena, RecordStringValues()));
  EXPECT_THAT(string_values_, ElementsAre("input123", "abc", "def", "abcdef"));
}

class RegexConstFoldInteropTest : public RegexPrecompilationExtensionTest {
 public:
  RegexConstFoldInteropTest() : RegexPrecompilationExtensionTest() {
    builder_.flat_expr_builder().AddProgramOptimizer(
        cel::runtime_internal::CreateConstantFoldingOptimizer());
  }

 protected:
  google::protobuf::Arena arena_;
};

TEST_P(RegexConstFoldInteropTest, StringConstantOptimizeable) {
  builder_.flat_expr_builder().AddProgramOptimizer(
      CreateRegexPrecompilationExtension(options_.regex_max_program_size));

  ASSERT_OK_AND_ASSIGN(exprpb::ParsedExpr parsed_expr,
                       Parse("input.matches('abc' + 'def')"));

  // Fake reference information for the matches call.
  exprpb::CheckedExpr expr;
  expr.mutable_expr()->Swap(parsed_expr.mutable_expr());
  expr.mutable_source_info()->Swap(parsed_expr.mutable_source_info());
  (*expr.mutable_reference_map())[2].add_overload_id("matches_string");

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelExpression> plan,
                       builder_.CreateExpression(&expr));
  Activation activation;
  google::protobuf::Arena arena;
  activation.InsertValue("input", CelValue::CreateStringView("input123"));

  ASSERT_OK(plan->Trace(activation, &arena, RecordStringValues()));
  EXPECT_THAT(string_values_, ElementsAre("input123"));
}

TEST_P(RegexConstFoldInteropTest, WrongTypeNotOptimized) {
  builder_.flat_expr_builder().AddProgramOptimizer(
      CreateRegexPrecompilationExtension(options_.regex_max_program_size));

  ASSERT_OK_AND_ASSIGN(exprpb::ParsedExpr parsed_expr,
                       Parse("input.matches(123 + 456)"));

  // Fake reference information for the matches call.
  exprpb::CheckedExpr expr;
  expr.mutable_expr()->Swap(parsed_expr.mutable_expr());
  expr.mutable_source_info()->Swap(parsed_expr.mutable_source_info());
  (*expr.mutable_reference_map())[2].add_overload_id("matches_string");

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelExpression> plan,
                       builder_.CreateExpression(&expr));

  Activation activation;
  google::protobuf::Arena arena;
  activation.InsertValue("input", CelValue::CreateStringView("input123"));

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       plan->Trace(activation, &arena, RecordStringValues()));
  EXPECT_THAT(string_values_, ElementsAre("input123"));
  EXPECT_TRUE(result.IsError());
  EXPECT_TRUE(CheckNoMatchingOverloadError(result));
}

INSTANTIATE_TEST_SUITE_P(RegexPrecompilationExtensionTest,
                         RegexPrecompilationExtensionTest, testing::Bool());

INSTANTIATE_TEST_SUITE_P(RegexConstFoldInteropTest, RegexConstFoldInteropTest,
                         testing::Bool());

}  // namespace
}  // namespace google::api::expr::runtime
