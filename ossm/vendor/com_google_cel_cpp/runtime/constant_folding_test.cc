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

#include "runtime/constant_folding.h"

#include <string>
#include <utility>
#include <vector>

#include "cel/expr/syntax.pb.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "base/function_adapter.h"
#include "common/value.h"
#include "extensions/protobuf/runtime_adapter.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "parser/parser.h"
#include "runtime/activation.h"
#include "runtime/register_function_helper.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "google/protobuf/arena.h"

namespace cel::extensions {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::cel::expr::ParsedExpr;
using ::google::api::expr::parser::Parse;
using ::testing::HasSubstr;

using ValueMatcher = testing::Matcher<Value>;

struct TestCase {
  std::string name;
  std::string expression;
  ValueMatcher result_matcher;
  absl::Status status;
};

MATCHER_P(IsIntValue, expected, "") {
  const Value& value = arg;
  return value->Is<IntValue>() && value.GetInt().NativeValue() == expected;
}

MATCHER_P(IsBoolValue, expected, "") {
  const Value& value = arg;
  return value->Is<BoolValue>() && value.GetBool().NativeValue() == expected;
}

MATCHER_P(IsErrorValue, expected_substr, "") {
  const Value& value = arg;
  return value->Is<ErrorValue>() &&
         absl::StrContains(value.GetError().NativeValue().message(),
                           expected_substr);
}

class ConstantFoldingExtTest : public testing::TestWithParam<TestCase> {};

TEST_P(ConstantFoldingExtTest, Runner) {
  google::protobuf::Arena arena;
  RuntimeOptions options;
  const TestCase& test_case = GetParam();
  ASSERT_OK_AND_ASSIGN(cel::RuntimeBuilder builder,
                       CreateStandardRuntimeBuilder(
                           internal::GetTestingDescriptorPool(), options));

  auto status = RegisterHelper<BinaryFunctionAdapter<
      absl::StatusOr<Value>, const StringValue&, const StringValue&>>::
      RegisterGlobalOverload(
          "prepend",
          [](const StringValue& value, const StringValue& prefix) {
            return StringValue(
                absl::StrCat(prefix.ToString(), value.ToString()));
          },
          builder.function_registry());
  ASSERT_THAT(status, IsOk());

  ASSERT_THAT(EnableConstantFolding(builder), IsOk());

  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(builder).Build());

  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse(test_case.expression));

  ASSERT_OK_AND_ASSIGN(auto program, ProtobufRuntimeAdapter::CreateProgram(
                                         *runtime, parsed_expr));
  Activation activation;

  auto result = program->Evaluate(&arena, activation);
  if (test_case.status.ok()) {
    ASSERT_OK_AND_ASSIGN(Value value, std::move(result));

    EXPECT_THAT(value, test_case.result_matcher);
    return;
  }

  EXPECT_THAT(result.status(), StatusIs(test_case.status.code(),
                                        HasSubstr(test_case.status.message())));
}

INSTANTIATE_TEST_SUITE_P(
    Cases, ConstantFoldingExtTest,
    testing::ValuesIn(std::vector<TestCase>{
        {"sum", "1 + 2 + 3", IsIntValue(6)},
        {"list_create", "[1, 2, 3, 4].filter(x, x < 4).size()", IsIntValue(3)},
        {"string_concat", "('12' + '34' + '56' + '78' + '90').size()",
         IsIntValue(10)},
        {"comprehension", "[1, 2, 3, 4].exists(x, x in [4, 5, 6, 7])",
         IsBoolValue(true)},
        {"nested_comprehension",
         "[1, 2, 3, 4].exists(x, [1, 2, 3, 4].all(y, y <= x))",
         IsBoolValue(true)},
        {"runtime_error", "[1, 2, 3, 4].exists(x, ['4'].all(y, y <= x))",
         IsErrorValue("No matching overloads")},
        // TODO(uncreated-issue/32): Depends on map creation
        // {"map_create", "{'abc': 'def', 'abd': 'deg'}.size()", 2},
        {"custom_function", "prepend('def', 'abc') == 'abcdef'",
         IsBoolValue(true)}}),

    [](const testing::TestParamInfo<TestCase>& info) {
      return info.param.name;
    });

}  // namespace
}  // namespace cel::extensions
