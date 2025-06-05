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

#include "eval/public/builtin_func_registrar.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/arena.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "eval/public/activation.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/testing/matchers.h"
#include "internal/testing.h"
#include "internal/time.h"
#include "parser/parser.h"

namespace google::api::expr::runtime {
namespace {

using google::api::expr::v1alpha1::Expr;
using google::api::expr::v1alpha1::SourceInfo;

using ::absl_testing::StatusIs;
using ::cel::internal::MaxDuration;
using ::cel::internal::MinDuration;
using ::testing::HasSubstr;

struct TestCase {
  std::string test_name;
  std::string expr;
  absl::flat_hash_map<std::string, CelValue> vars;
  absl::StatusOr<CelValue> result = CelValue::CreateBool(true);
  InterpreterOptions options;
};

InterpreterOptions OverflowChecksEnabled() {
  static InterpreterOptions options;
  options.enable_timestamp_duration_overflow_errors = true;
  return options;
}

void ExpectResult(const TestCase& test_case) {
  auto parsed_expr = parser::Parse(test_case.expr);
  ASSERT_OK(parsed_expr);
  const Expr& expr_ast = parsed_expr->expr();
  const SourceInfo& source_info = parsed_expr->source_info();

  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(test_case.options);
  ASSERT_OK(
      RegisterBuiltinFunctions(builder->GetRegistry(), test_case.options));
  ASSERT_OK_AND_ASSIGN(auto cel_expression,
                       builder->CreateExpression(&expr_ast, &source_info));

  Activation activation;
  for (auto var : test_case.vars) {
    activation.InsertValue(var.first, var.second);
  }

  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(auto value,
                       cel_expression->Evaluate(activation, &arena));
  if (!test_case.result.ok()) {
    EXPECT_TRUE(value.IsError()) << value.DebugString();
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
        {"TimeSubTimeLegacy",
         "t0 - t1 == duration('90s90ns')",
         {
             {"t0", CelValue::CreateTimestamp(absl::FromUnixSeconds(100) +
                                              absl::Nanoseconds(100))},
             {"t1", CelValue::CreateTimestamp(absl::FromUnixSeconds(10) +
                                              absl::Nanoseconds(10))},
         }},

        {"TimeSubDurationLegacy",
         "t0 - duration('90s90ns')",
         {
             {"t0", CelValue::CreateTimestamp(absl::FromUnixSeconds(100) +
                                              absl::Nanoseconds(100))},
         },
         CelValue::CreateTimestamp(absl::FromUnixSeconds(10) +
                                   absl::Nanoseconds(10))},

        {"TimeAddDurationLegacy",
         "t + duration('90s90ns')",
         {{"t", CelValue::CreateTimestamp(absl::FromUnixSeconds(10) +
                                          absl::Nanoseconds(10))}},
         CelValue::CreateTimestamp(absl::FromUnixSeconds(100) +
                                   absl::Nanoseconds(100))},
        {"DurationAddTimeLegacy",
         "duration('90s90ns') + t == t + duration('90s90ns')",
         {{"t", CelValue::CreateTimestamp(absl::FromUnixSeconds(10) +
                                          absl::Nanoseconds(10))}}},

        {"DurationAddDurationLegacy",
         "duration('80s80ns') + duration('10s10ns') == duration('90s90ns')"},

        {"DurationSubDurationLegacy",
         "duration('90s90ns') - duration('80s80ns') == duration('10s10ns')"},

        {"MinDurationSubDurationLegacy",
         "min - duration('1ns') < duration('-87660000h')",
         {{"min", CelValue::CreateDuration(MinDuration())}}},

        {"MaxDurationAddDurationLegacy",
         "max + duration('1ns') > duration('87660000h')",
         {{"max", CelValue::CreateDuration(MaxDuration())}}},

        {"TimestampConversionFromStringLegacy",
         "timestamp('10000-01-02T00:00:00Z') > "
         "timestamp('9999-01-01T00:00:00Z')"},

        {"TimestampFromUnixEpochSeconds",
         "timestamp(123) > timestamp('1970-01-01T00:02:02.999999999Z') && "
         "timestamp(123) == timestamp('1970-01-01T00:02:03Z') && "
         "timestamp(123) < timestamp('1970-01-01T00:02:03.000000001Z')"},

        // Timestamp duration tests with fixes enabled for overflow checking.
        {"TimeSubTime",
         "t0 - t1 == duration('90s90ns')",
         {
             {"t0", CelValue::CreateTimestamp(absl::FromUnixSeconds(100) +
                                              absl::Nanoseconds(100))},
             {"t1", CelValue::CreateTimestamp(absl::FromUnixSeconds(10) +
                                              absl::Nanoseconds(10))},
         },
         CelValue::CreateBool(true),
         OverflowChecksEnabled()},

        {"TimeSubDuration",
         "t0 - duration('90s90ns')",
         {
             {"t0", CelValue::CreateTimestamp(absl::FromUnixSeconds(100) +
                                              absl::Nanoseconds(100))},
         },
         CelValue::CreateTimestamp(absl::FromUnixSeconds(10) +
                                   absl::Nanoseconds(10)),
         OverflowChecksEnabled()},

        {"TimeSubtractDurationOverflow",
         "timestamp('0001-01-01T00:00:00Z') - duration('1ns')",
         {},
         absl::OutOfRangeError("timestamp overflow"),
         OverflowChecksEnabled()},

        {"TimeAddDuration",
         "t + duration('90s90ns')",
         {{"t", CelValue::CreateTimestamp(absl::FromUnixSeconds(10) +
                                          absl::Nanoseconds(10))}},
         CelValue::CreateTimestamp(absl::FromUnixSeconds(100) +
                                   absl::Nanoseconds(100)),
         OverflowChecksEnabled()},

        {"TimeAddDurationOverflow",
         "timestamp('9999-12-31T23:59:59.999999999Z') + duration('1ns')",
         {},
         absl::OutOfRangeError("timestamp overflow"),
         OverflowChecksEnabled()},

        {"DurationAddTime",
         "duration('90s90ns') + t == t + duration('90s90ns')",
         {{"t", CelValue::CreateTimestamp(absl::FromUnixSeconds(10) +
                                          absl::Nanoseconds(10))}},
         CelValue::CreateBool(true),
         OverflowChecksEnabled()},

        {"DurationAddTimeOverflow",
         "duration('1ns') + timestamp('9999-12-31T23:59:59.999999999Z')",
         {},
         absl::OutOfRangeError("timestamp overflow"),
         OverflowChecksEnabled()},

        {"DurationAddDuration",
         "duration('80s80ns') + duration('10s10ns') == duration('90s90ns')",
         {},
         CelValue::CreateBool(true),
         OverflowChecksEnabled()},

        {"DurationSubDuration",
         "duration('90s90ns') - duration('80s80ns') == duration('10s10ns')",
         {},
         CelValue::CreateBool(true),
         OverflowChecksEnabled()},

        {"MinDurationSubDuration",
         "min - duration('1ns')",
         {{"min", CelValue::CreateDuration(MinDuration())}},
         absl::OutOfRangeError("overflow"),
         OverflowChecksEnabled()},

        {"MaxDurationAddDuration",
         "max + duration('1ns')",
         {{"max", CelValue::CreateDuration(MaxDuration())}},
         absl::OutOfRangeError("overflow"),
         OverflowChecksEnabled()},

        // Timestamp conversion overflow checks.
        {"TimestampConversionFromStringOverflow",
         "timestamp('10000-01-02T00:00:00Z')",
         {},
         absl::OutOfRangeError("timestamp overflow"),
         OverflowChecksEnabled()},

        {"TimestampConversionFromStringUnderflow",
         "timestamp('0000-01-01T00:00:00Z')",
         {},
         absl::OutOfRangeError("timestamp overflow"),
         OverflowChecksEnabled()},

        // List concatenation tests.
        {"ListConcatEmptyInputs",
         "[] + [] == []",
         {},
         CelValue::CreateBool(true),
         OverflowChecksEnabled()},
        {"ListConcatRightEmpty",
         "[1] + [] == [1]",
         {},
         CelValue::CreateBool(true),
         OverflowChecksEnabled()},
        {"ListConcatLeftEmpty",
         "[] + [1] == [1]",
         {},
         CelValue::CreateBool(true),
         OverflowChecksEnabled()},
        {"ListConcat",
         "[2] + [1] == [2, 1]",
         {},
         CelValue::CreateBool(true),
         OverflowChecksEnabled()},
    }),
    [](const testing::TestParamInfo<BuiltinFuncParamsTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace google::api::expr::runtime
