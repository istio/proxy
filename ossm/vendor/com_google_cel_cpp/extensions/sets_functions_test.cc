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

#include "extensions/sets_functions.h"

#include <memory>
#include <string>

#include "cel/expr/syntax.pb.h"
#include "absl/status/status_matchers.h"
#include "checker/standard_library.h"
#include "checker/validation_result.h"
#include "common/ast_proto.h"
#include "common/minimal_descriptor_pool.h"
#include "compiler/compiler_factory.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_function_adapter.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "internal/testing.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"

namespace cel::extensions {
namespace {

using ::google::api::expr::runtime::Activation;
using ::google::api::expr::runtime::CelExpressionBuilder;
using ::google::api::expr::runtime::CelValue;
using ::google::api::expr::runtime::CreateCelExpressionBuilder;
using ::google::api::expr::runtime::FunctionAdapter;
using ::google::api::expr::runtime::InterpreterOptions;

using ::absl_testing::IsOk;
using ::google::protobuf::Arena;

struct TestInfo {
  std::string expr;
};

class CelSetsFunctionsTest : public testing::TestWithParam<TestInfo> {};

TEST_P(CelSetsFunctionsTest, EndToEnd) {
  const TestInfo& test_info = GetParam();
  ASSERT_OK_AND_ASSIGN(auto compiler_builder,
                       NewCompilerBuilder(cel::GetMinimalDescriptorPool()));

  ASSERT_THAT(compiler_builder->AddLibrary(StandardCheckerLibrary()), IsOk());
  ASSERT_THAT(compiler_builder->AddLibrary(SetsCompilerLibrary()), IsOk());
  ASSERT_OK_AND_ASSIGN(auto compiler, compiler_builder->Build());

  ASSERT_OK_AND_ASSIGN(ValidationResult compiled,
                       compiler->Compile(test_info.expr));

  ASSERT_TRUE(compiled.IsValid()) << compiled.FormatError();

  cel::expr::CheckedExpr checked_expr;
  ASSERT_THAT(AstToCheckedExpr(*compiled.GetAst(), &checked_expr), IsOk());

  // Obtain CEL Expression builder.
  InterpreterOptions options;
  options.enable_heterogeneous_equality = true;
  options.enable_empty_wrapper_null_unboxing = true;
  options.enable_qualified_identifier_rewrites = true;
  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterSetsFunctions(builder->GetRegistry()->InternalGetRegistry(),
                                  cel::RuntimeOptions{}));
  ASSERT_OK(google::api::expr::runtime::RegisterBuiltinFunctions(
      builder->GetRegistry(), options));

  // Create CelExpression from AST (Expr object).
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder->CreateExpression(&checked_expr));
  Arena arena;
  Activation activation;
  // Run evaluation.
  ASSERT_OK_AND_ASSIGN(CelValue out, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(out.IsBool()) << test_info.expr << " -> " << out.DebugString();
  EXPECT_TRUE(out.BoolOrDie()) << test_info.expr << " -> " << out.DebugString();
}

INSTANTIATE_TEST_SUITE_P(
    CelSetsFunctionsTest, CelSetsFunctionsTest,
    testing::ValuesIn<TestInfo>({
        {"sets.contains([], [])"},
        {"sets.contains([1], [])"},
        {"sets.contains([1], [1])"},
        {"sets.contains([1], [1, 1])"},
        {"sets.contains([1, 1], [1])"},
        {"sets.contains([2, 1], [1])"},
        {"sets.contains([1], [1.0, 1u])"},
        {"sets.contains([1, 2], [2u, 2.0])"},
        {"sets.contains([1, 2u], [2, 2.0])"},
        {"!sets.contains([1], [2])"},
        {"!sets.contains([1], [1, 2])"},
        {"!sets.contains([1], [\"1\", 1])"},
        {"!sets.contains([1], [1.1, 2])"},
        {"sets.intersects([1], [1])"},
        {"sets.intersects([1], [1, 1])"},
        {"sets.intersects([1, 1], [1])"},
        {"sets.intersects([2, 1], [1])"},
        {"sets.intersects([1], [1, 2])"},
        {"sets.intersects([1], [1.0, 2])"},
        {"sets.intersects([1, 2], [2u, 2, 2.0])"},
        {"sets.intersects([1, 2], [1u, 2, 2.3])"},
        {"!sets.intersects([], [])"},
        {"!sets.intersects([1], [])"},
        {"!sets.intersects([1], [2])"},
        {"!sets.intersects([1], [\"1\", 2])"},
        {"!sets.intersects([1], [1.1, 2u])"},
        {"sets.equivalent([], [])"},
        {"sets.equivalent([1], [1])"},
        {"sets.equivalent([1], [1, 1])"},
        {"sets.equivalent([1, 1, 2], [2, 2, 1])"},
        {"sets.equivalent([1, 1], [1])"},
        {"sets.equivalent([1], [1u, 1.0])"},
        {"sets.equivalent([1], [1u, 1.0])"},
        {"sets.equivalent([1, 2, 3], [3u, 2.0, 1])"},
        {"!sets.equivalent([2, 1], [1])"},
        {"!sets.equivalent([1], [1, 2])"},
        {"!sets.equivalent([1, 2], [2u, 2, 2.0])"},
        {"!sets.equivalent([1, 2], [1u, 2, 2.3])"},

        {"sets.equivalent([false, true], [true, false])"},
        {"!sets.equivalent([true], [false])"},

        {"sets.equivalent(['foo', 'bar'], ['bar', 'foo'])"},
        {"!sets.equivalent(['foo'], ['bar'])"},

        {"sets.equivalent([b'foo', b'bar'], [b'bar', b'foo'])"},
        {"!sets.equivalent([b'foo'], [b'bar'])"},

        {"sets.equivalent([null], [null])"},
        {"!sets.equivalent([null], [])"},

        {"sets.equivalent([type(1), type(1u)], [type(1u), type(1)])"},
        {"!sets.equivalent([type(1)], [type(1u)])"},

        {"sets.equivalent([duration('0s'), duration('1s')], [duration('1s'), "
         "duration('0s')])"},
        {"!sets.equivalent([duration('0s')], [duration('1s')])"},

        {"sets.equivalent([timestamp('1970-01-01T00:00:00Z'), "
         "timestamp('1970-01-01T00:00:01Z')], "
         "[timestamp('1970-01-01T00:00:01Z'), "
         "timestamp('1970-01-01T00:00:00Z')])"},
        {"!sets.equivalent([timestamp('1970-01-01T00:00:00Z')], "
         "[timestamp('1970-01-01T00:00:01Z')])"},

        {"sets.equivalent([[false, true]], [[false, true]])"},
        {"!sets.equivalent([[false, true]], [[true, false]])"},

        {"sets.equivalent([{'foo': true, 'bar': false}], [{'bar': false, "
         "'foo': true}])"},
    }));

}  // namespace
}  // namespace cel::extensions
