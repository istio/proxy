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

#include "eval/public/container_function_registrar.h"

#include <memory>
#include <string>

#include "eval/public/activation.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/equality_function_registrar.h"
#include "eval/public/testing/matchers.h"
#include "internal/testing.h"
#include "parser/parser.h"

namespace google::api::expr::runtime {
namespace {

using google::api::expr::v1alpha1::Expr;
using google::api::expr::v1alpha1::SourceInfo;
using ::testing::ValuesIn;

struct TestCase {
  std::string test_name;
  std::string expr;
  absl::StatusOr<CelValue> result = CelValue::CreateBool(true);
};

const CelList& CelNumberListExample() {
  static ContainerBackedListImpl* example =
      new ContainerBackedListImpl({CelValue::CreateInt64(1)});
  return *example;
}

void ExpectResult(const TestCase& test_case) {
  auto parsed_expr = parser::Parse(test_case.expr);
  ASSERT_OK(parsed_expr);
  const Expr& expr_ast = parsed_expr->expr();
  const SourceInfo& source_info = parsed_expr->source_info();
  InterpreterOptions options;
  options.enable_timestamp_duration_overflow_errors = true;
  options.enable_comprehension_list_append = true;
  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterContainerFunctions(builder->GetRegistry(), options));
  // Needed to avoid error - No overloads provided for FunctionStep creation.
  ASSERT_OK(RegisterEqualityFunctions(builder->GetRegistry(), options));
  ASSERT_OK_AND_ASSIGN(auto cel_expression,
                       builder->CreateExpression(&expr_ast, &source_info));

  Activation activation;

  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(auto value,
                       cel_expression->Evaluate(activation, &arena));
  EXPECT_THAT(value, test::EqualsCelValue(*test_case.result));
}

using ContainerFunctionParamsTest = testing::TestWithParam<TestCase>;
TEST_P(ContainerFunctionParamsTest, StandardFunctions) {
  ExpectResult(GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    ContainerFunctionParamsTest, ContainerFunctionParamsTest,
    ValuesIn<TestCase>(
        {{"FilterNumbers", "[1, 2, 3].filter(num, num == 1)",
          CelValue::CreateList(&CelNumberListExample())},
         {"ListConcatEmptyInputs", "[] + [] == []", CelValue::CreateBool(true)},
         {"ListConcatRightEmpty", "[1] + [] == [1]",
          CelValue::CreateBool(true)},
         {"ListConcatLeftEmpty", "[] + [1] == [1]", CelValue::CreateBool(true)},
         {"ListConcat", "[2] + [1] == [2, 1]", CelValue::CreateBool(true)},
         {"ListSize", "[1, 2, 3].size() == 3", CelValue::CreateBool(true)},
         {"MapSize", "{1: 2, 2: 4}.size() == 2", CelValue::CreateBool(true)},
         {"EmptyListSize", "size({}) == 0", CelValue::CreateBool(true)}}),
    [](const testing::TestParamInfo<ContainerFunctionParamsTest::ParamType>&
           info) { return info.param.test_name; });

}  // namespace
}  // namespace google::api::expr::runtime
