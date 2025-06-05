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

#include "eval/public/comparison_functions.h"

#include <memory>
#include <tuple>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/rpc/context/attribute_context.pb.h"
#include "google/protobuf/arena.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "eval/public/activation.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/testing/matchers.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "parser/parser.h"

namespace google::api::expr::runtime {
namespace {

using ::google::api::expr::v1alpha1::ParsedExpr;
using ::google::rpc::context::AttributeContext;
using ::testing::Combine;
using ::testing::ValuesIn;

MATCHER_P2(DefinesHomogenousOverload, name, argument_type,
           absl::StrCat(name, " for ", CelValue::TypeName(argument_type))) {
  const CelFunctionRegistry& registry = arg;
  return !registry
              .FindOverloads(name, /*receiver_style=*/false,
                             {argument_type, argument_type})
              .empty();
  return false;
}

struct ComparisonTestCase {
  absl::string_view expr;
  bool result;
  CelValue lhs = CelValue::CreateNull();
  CelValue rhs = CelValue::CreateNull();
};

class ComparisonFunctionTest
    : public testing::TestWithParam<std::tuple<ComparisonTestCase, bool>> {
 public:
  ComparisonFunctionTest() {
    options_.enable_heterogeneous_equality = std::get<1>(GetParam());
    options_.enable_empty_wrapper_null_unboxing = true;
    builder_ = CreateCelExpressionBuilder(options_);
  }

  CelFunctionRegistry& registry() { return *builder_->GetRegistry(); }

  absl::StatusOr<CelValue> Evaluate(absl::string_view expr, const CelValue& lhs,
                                    const CelValue& rhs) {
    CEL_ASSIGN_OR_RETURN(ParsedExpr parsed_expr, parser::Parse(expr));
    Activation activation;
    activation.InsertValue("lhs", lhs);
    activation.InsertValue("rhs", rhs);

    CEL_ASSIGN_OR_RETURN(auto expression,
                         builder_->CreateExpression(
                             &parsed_expr.expr(), &parsed_expr.source_info()));

    return expression->Evaluate(activation, &arena_);
  }

 protected:
  std::unique_ptr<CelExpressionBuilder> builder_;
  InterpreterOptions options_;
  google::protobuf::Arena arena_;
};

TEST_P(ComparisonFunctionTest, SmokeTest) {
  ComparisonTestCase test_case = std::get<0>(GetParam());
  google::protobuf::LinkMessageReflection<AttributeContext>();

  ASSERT_OK(RegisterComparisonFunctions(&registry(), options_));
  ASSERT_OK_AND_ASSIGN(auto result,
                       Evaluate(test_case.expr, test_case.lhs, test_case.rhs));

  EXPECT_THAT(result, test::IsCelBool(test_case.result));
}

INSTANTIATE_TEST_SUITE_P(
    LessThan, ComparisonFunctionTest,
    Combine(ValuesIn<ComparisonTestCase>(
                {// less than
                 {"false < true", true},
                 {"1 < 2", true},
                 {"-2 < -1", true},
                 {"1.1 < 1.2", true},
                 {"'a' < 'b'", true},
                 {"lhs < rhs", true, CelValue::CreateBytesView("a"),
                  CelValue::CreateBytesView("b")},
                 {"lhs < rhs", true, CelValue::CreateDuration(absl::Seconds(1)),
                  CelValue::CreateDuration(absl::Seconds(2))},
                 {"lhs < rhs", true,
                  CelValue::CreateTimestamp(absl::FromUnixSeconds(20)),
                  CelValue::CreateTimestamp(absl::FromUnixSeconds(30))}}),
            // heterogeneous equality enabled
            testing::Bool()));

INSTANTIATE_TEST_SUITE_P(
    GreaterThan, ComparisonFunctionTest,
    testing::Combine(
        testing::ValuesIn<ComparisonTestCase>(
            {{"false > true", false},
             {"1 > 2", false},
             {"-2 > -1", false},
             {"1.1 > 1.2", false},
             {"'a' > 'b'", false},
             {"lhs > rhs", false, CelValue::CreateBytesView("a"),
              CelValue::CreateBytesView("b")},
             {"lhs > rhs", false, CelValue::CreateDuration(absl::Seconds(1)),
              CelValue::CreateDuration(absl::Seconds(2))},
             {"lhs > rhs", false,
              CelValue::CreateTimestamp(absl::FromUnixSeconds(20)),
              CelValue::CreateTimestamp(absl::FromUnixSeconds(30))}}),
        // heterogeneous equality enabled
        testing::Bool()));

INSTANTIATE_TEST_SUITE_P(
    GreaterOrEqual, ComparisonFunctionTest,
    Combine(ValuesIn<ComparisonTestCase>(
                {{"false >= true", false},
                 {"1 >= 2", false},
                 {"-2 >= -1", false},
                 {"1.1 >= 1.2", false},
                 {"'a' >= 'b'", false},
                 {"lhs >= rhs", false, CelValue::CreateBytesView("a"),
                  CelValue::CreateBytesView("b")},
                 {"lhs >= rhs", false,
                  CelValue::CreateDuration(absl::Seconds(1)),
                  CelValue::CreateDuration(absl::Seconds(2))},
                 {"lhs >= rhs", false,
                  CelValue::CreateTimestamp(absl::FromUnixSeconds(20)),
                  CelValue::CreateTimestamp(absl::FromUnixSeconds(30))}}),
            // heterogeneous equality enabled
            testing::Bool()));

INSTANTIATE_TEST_SUITE_P(
    LessOrEqual, ComparisonFunctionTest,
    Combine(testing::ValuesIn<ComparisonTestCase>(
                {{"false <= true", true},
                 {"1 <= 2", true},
                 {"-2 <= -1", true},
                 {"1.1 <= 1.2", true},
                 {"'a' <= 'b'", true},
                 {"lhs <= rhs", true, CelValue::CreateBytesView("a"),
                  CelValue::CreateBytesView("b")},
                 {"lhs <= rhs", true,
                  CelValue::CreateDuration(absl::Seconds(1)),
                  CelValue::CreateDuration(absl::Seconds(2))},
                 {"lhs <= rhs", true,
                  CelValue::CreateTimestamp(absl::FromUnixSeconds(20)),
                  CelValue::CreateTimestamp(absl::FromUnixSeconds(30))}}),
            // heterogeneous equality enabled
            testing::Bool()));

INSTANTIATE_TEST_SUITE_P(HeterogeneousNumericComparisons,
                         ComparisonFunctionTest,
                         Combine(testing::ValuesIn<ComparisonTestCase>(
                                     {                   // less than
                                      {"1 < 2u", true},  // int < uint
                                      {"2 < 1u", false},
                                      {"1 < 2.1", true},  // int < double
                                      {"3 < 2.1", false},
                                      {"1u < 2", true},  // uint < int
                                      {"2u < 1", false},
                                      {"1u < -1.1", false},  // uint < double
                                      {"1u < 2.1", true},
                                      {"1.1 < 2", true},  // double < int
                                      {"1.1 < 1", false},
                                      {"1.0 < 1u", false},  // double < uint
                                      {"1.0 < 3u", true},

                                      // less than or equal
                                      {"1 <= 2u", true},  // int <= uint
                                      {"2 <= 1u", false},
                                      {"1 <= 2.1", true},  // int <= double
                                      {"3 <= 2.1", false},
                                      {"1u <= 2", true},  // uint <= int
                                      {"1u <= 0", false},
                                      {"1u <= -1.1", false},  // uint <= double
                                      {"2u <= 1.0", false},
                                      {"1.1 <= 2", true},  // double <= int
                                      {"2.1 <= 2", false},
                                      {"1.0 <= 1u", true},  // double <= uint
                                      {"1.1 <= 1u", false},

                                      // greater than
                                      {"3 > 2u", true},  // int > uint
                                      {"3 > 4u", false},
                                      {"3 > 2.1", true},  // int > double
                                      {"3 > 4.1", false},
                                      {"3u > 2", true},  // uint > int
                                      {"3u > 4", false},
                                      {"3u > -1.1", true},  // uint > double
                                      {"3u > 4.1", false},
                                      {"3.1 > 2", true},  // double > int
                                      {"3.1 > 4", false},
                                      {"3.0 > 1u", true},  // double > uint
                                      {"3.0 > 4u", false},

                                      // greater than or equal
                                      {"3 >= 2u", true},  // int >= uint
                                      {"3 >= 4u", false},
                                      {"3 >= 2.1", true},  // int >= double
                                      {"3 >= 4.1", false},
                                      {"3u >= 2", true},  // uint >= int
                                      {"3u >= 4", false},
                                      {"3u >= -1.1", true},  // uint >= double
                                      {"3u >= 4.1", false},
                                      {"3.1 >= 2", true},  // double >= int
                                      {"3.1 >= 4", false},
                                      {"3.0 >= 1u", true},  // double >= uint
                                      {"3.0 >= 4u", false},
                                      {"1u >= -1", true},
                                      {"1 >= 4u", false},

                                      // edge cases
                                      {"-1 < 1u", true},
                                      {"1 < 9223372036854775808u", true}}),
                                 testing::Values<bool>(true)));

}  // namespace
}  // namespace google::api::expr::runtime
