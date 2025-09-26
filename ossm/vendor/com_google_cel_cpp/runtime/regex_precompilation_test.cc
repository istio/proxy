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

#include "runtime/regex_precompilation.h"

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
#include "runtime/constant_folding.h"
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
using ::testing::_;
using ::testing::HasSubstr;

using ValueMatcher = testing::Matcher<Value>;

struct TestCase {
  std::string name;
  std::string expression;
  ValueMatcher result_matcher;
  absl::Status create_status;
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

class RegexPrecompilationTest : public testing::TestWithParam<TestCase> {};

TEST_P(RegexPrecompilationTest, Basic) {
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

  ASSERT_THAT(EnableRegexPrecompilation(builder), IsOk());

  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(builder).Build());

  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse(test_case.expression));

  auto program_or =
      ProtobufRuntimeAdapter::CreateProgram(*runtime, parsed_expr);
  if (!test_case.create_status.ok()) {
    ASSERT_THAT(program_or.status(),
                StatusIs(test_case.create_status.code(),
                         HasSubstr(test_case.create_status.message())));
    return;
  }

  ASSERT_OK_AND_ASSIGN(auto program, std::move(program_or));

  google::protobuf::Arena arena;
  Activation activation;
  activation.InsertOrAssignValue("string_var",
                                 StringValue(&arena, "string_var"));

  ASSERT_OK_AND_ASSIGN(Value value, program->Evaluate(&arena, activation));
  EXPECT_THAT(value, test_case.result_matcher);
}

TEST_P(RegexPrecompilationTest, WithConstantFolding) {
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
  ASSERT_THAT(EnableRegexPrecompilation(builder), IsOk());

  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(builder).Build());

  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse(test_case.expression));

  auto program_or =
      ProtobufRuntimeAdapter::CreateProgram(*runtime, parsed_expr);
  if (!test_case.create_status.ok()) {
    ASSERT_THAT(program_or.status(),
                StatusIs(test_case.create_status.code(),
                         HasSubstr(test_case.create_status.message())));
    return;
  }

  ASSERT_OK_AND_ASSIGN(auto program, std::move(program_or));
  google::protobuf::Arena arena;
  Activation activation;
  activation.InsertOrAssignValue("string_var",
                                 StringValue(&arena, "string_var"));

  ASSERT_OK_AND_ASSIGN(Value value, program->Evaluate(&arena, activation));
  EXPECT_THAT(value, test_case.result_matcher);
}

INSTANTIATE_TEST_SUITE_P(
    Cases, RegexPrecompilationTest,
    testing::ValuesIn(std::vector<TestCase>{
        {"matches_receiver", R"(string_var.matches(r's\w+_var'))",
         IsBoolValue(true)},
        {"matches_receiver_false", R"(string_var.matches(r'string_var\d+'))",
         IsBoolValue(false)},
        {"matches_global_true", R"(matches(string_var, r's\w+_var'))",
         IsBoolValue(true)},
        {"matches_global_false", R"(matches(string_var, r'string_var\d+'))",
         IsBoolValue(false)},
        {"matches_bad_re2_expression", "matches('123', r'(?<!a)123')", _,
         absl::InvalidArgumentError("unsupported RE2")},
        {"matches_unsupported_call_signature",
         "matches('123', r'(?<!a)123', 'gi')", _,
         absl::InvalidArgumentError("No overloads")},
        {"constant_computation",
         "matches(string_var, r'string' + '_' + r'var')", IsBoolValue(true)},
    }),

    [](const testing::TestParamInfo<TestCase>& info) {
      return info.param.name;
    });

}  // namespace
}  // namespace cel::extensions
