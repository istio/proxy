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

#include "eval/public/logical_function_registrar.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cel/expr/syntax.pb.h"
#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "eval/public/activation.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/portable_cel_function_adapter.h"
#include "eval/public/testing/matchers.h"
#include "internal/testing.h"
#include "parser/parser.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {
namespace {

using cel::expr::Expr;
using cel::expr::SourceInfo;

using ::absl_testing::StatusIs;
using ::testing::HasSubstr;

struct TestCase {
  std::string test_name;
  std::string expr;
  absl::StatusOr<CelValue> result = CelValue::CreateBool(true);
};

const CelError* ExampleError() {
  static absl::NoDestructor<absl::Status> error(
      absl::InternalError("test example error"));

  return &*error;
}

void ExpectResult(const TestCase& test_case) {
  auto parsed_expr = parser::Parse(test_case.expr);
  ASSERT_OK(parsed_expr);
  const Expr& expr_ast = parsed_expr->expr();
  const SourceInfo& source_info = parsed_expr->source_info();
  InterpreterOptions options;
  options.short_circuiting = true;
  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterLogicalFunctions(builder->GetRegistry(), options));
  ASSERT_OK(builder->GetRegistry()->Register(
      PortableUnaryFunctionAdapter<CelValue, CelValue::StringHolder>::Create(
          "toBool", false,
          [](google::protobuf::Arena*, CelValue::StringHolder holder) -> CelValue {
            if (holder.value() == "true") {
              return CelValue::CreateBool(true);
            } else if (holder.value() == "false") {
              return CelValue::CreateBool(false);
            }
            return CelValue::CreateError(ExampleError());
          })));
  ASSERT_OK_AND_ASSIGN(auto cel_expression,
                       builder->CreateExpression(&expr_ast, &source_info));

  Activation activation;

  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(auto value,
                       cel_expression->Evaluate(activation, &arena));
  if (!test_case.result.ok()) {
    EXPECT_TRUE(value.IsError());
    EXPECT_THAT(*value.ErrorOrDie(),
                StatusIs(test_case.result.status().code(),
                         HasSubstr(test_case.result.status().message())));
    return;
  }
  EXPECT_THAT(value, test::EqualsCelValue(*test_case.result));
}

using BuiltinFuncParamsTest = testing::TestWithParam<TestCase>;
TEST_P(BuiltinFuncParamsTest, StandardFunctions) { ExpectResult(GetParam()); }

INSTANTIATE_TEST_SUITE_P(
    BuiltinFuncParamsTest, BuiltinFuncParamsTest,
    testing::ValuesIn<TestCase>({
        // Legacy duration and timestamp arithmetic tests.
        {"LogicalNotOfTrue", "!true", CelValue::CreateBool(false)},
        {"LogicalNotOfFalse", "!false", CelValue::CreateBool(true)},
        // Not strictly false is an internal function for implementing logical
        // shortcutting in comprehensions.
        {"NotStrictlyFalseTrue", "[true, true, true].all(x, x)",
         CelValue::CreateBool(true)},
        // List creation is eager so use an extension function to introduce an
        // error.
        {"NotStrictlyFalseErrorShortcircuit",
         "['true', 'false', 'error'].all(x, toBool(x))",
         CelValue::CreateBool(false)},
        {"NotStrictlyFalseError", "['true', 'true', 'error'].all(x, toBool(x))",
         CelValue::CreateError(ExampleError())},
        {"NotStrictlyFalseFalse", "[false, false, false].all(x, x)",
         CelValue::CreateBool(false)},
    }),
    [](const testing::TestParamInfo<BuiltinFuncParamsTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace google::api::expr::runtime
