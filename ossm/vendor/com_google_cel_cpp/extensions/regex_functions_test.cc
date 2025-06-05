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

#include "extensions/regex_functions.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/arena.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "eval/public/activation.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/testing/matchers.h"
#include "internal/testing.h"
#include "parser/parser.h"

namespace cel::extensions {

namespace {

using ::absl_testing::StatusIs;
using ::google::api::expr::runtime::CelValue;
using Builder = ::google::api::expr::runtime::CelExpressionBuilder;
using ::absl_testing::IsOkAndHolds;
using ::google::api::expr::parser::Parse;
using ::google::api::expr::runtime::test::IsCelError;
using ::google::api::expr::runtime::test::IsCelString;

struct TestCase {
  const std::string expr_string;
  const std::string expected_result;
};

class RegexFunctionsTest : public ::testing::TestWithParam<TestCase> {
 public:
  RegexFunctionsTest() {
    options_.enable_regex = true;
    options_.enable_qualified_identifier_rewrites = true;
    builder_ = CreateCelExpressionBuilder(options_);
  }

  absl::StatusOr<CelValue> TestCaptureStringInclusion(
      const std::string& expr_string) {
    CEL_RETURN_IF_ERROR(
        RegisterRegexFunctions(builder_->GetRegistry(), options_));
    CEL_ASSIGN_OR_RETURN(auto parsed_expr, Parse(expr_string));
    CEL_ASSIGN_OR_RETURN(
        auto expr_plan, builder_->CreateExpression(&parsed_expr.expr(),
                                                   &parsed_expr.source_info()));
    ::google::api::expr::runtime::Activation activation;
    return expr_plan->Evaluate(activation, &arena_);
  }

  google::protobuf::Arena arena_;
  google::api::expr::runtime::InterpreterOptions options_;
  std::unique_ptr<Builder> builder_;
};

TEST_F(RegexFunctionsTest, CaptureStringSuccessWithCombinationOfGroups) {
  // combination of named and unnamed groups should return a celmap
  std::vector<std::pair<CelValue, CelValue>> cel_values;
  cel_values.emplace_back(std::make_pair(
      CelValue::CreateString(google::protobuf::Arena::Create<std::string>(&arena_, "1")),
      CelValue::CreateString(
          google::protobuf::Arena::Create<std::string>(&arena_, "user"))));
  cel_values.emplace_back(std::make_pair(
      CelValue::CreateString(
          google::protobuf::Arena::Create<std::string>(&arena_, "Username")),
      CelValue::CreateString(
          google::protobuf::Arena::Create<std::string>(&arena_, "testuser"))));
  cel_values.emplace_back(
      std::make_pair(CelValue::CreateString(
                         google::protobuf::Arena::Create<std::string>(&arena_, "Domain")),
                     CelValue::CreateString(google::protobuf::Arena::Create<std::string>(
                         &arena_, "testdomain"))));

  auto container_map = google::api::expr::runtime::CreateContainerBackedMap(
      absl::MakeSpan(cel_values));

  // Release ownership of container_map to Arena.
  auto* cel_map = container_map->release();
  arena_.Own(cel_map);
  CelValue expected_result = CelValue::CreateMap(cel_map);

  auto status = TestCaptureStringInclusion(
      (R"(re.captureN('The user testuser belongs to testdomain',
      'The (user|domain) (?P<Username>.*) belongs to (?P<Domain>.*)'))"));
  ASSERT_OK(status.status());
  EXPECT_EQ(status.value().DebugString(), expected_result.DebugString());
}

TEST_F(RegexFunctionsTest, CaptureStringSuccessWithSingleNamedGroup) {
  // Regex containing one named group should return a map
  std::vector<std::pair<CelValue, CelValue>> cel_values;
  cel_values.push_back(std::make_pair(
      CelValue::CreateString(
          google::protobuf::Arena::Create<std::string>(&arena_, "username")),
      CelValue::CreateString(
          google::protobuf::Arena::Create<std::string>(&arena_, "testuser"))));
  auto container_map = google::api::expr::runtime::CreateContainerBackedMap(
      absl::MakeSpan(cel_values));
  // Release ownership of container_map to Arena.
  auto cel_map = container_map->release();
  arena_.Own(cel_map);
  CelValue expected_result = CelValue::CreateMap(cel_map);

  auto status = TestCaptureStringInclusion((R"(re.captureN('testuser@',
                                            '(?P<username>.*)@'))"));
  ASSERT_OK(status.status());
  EXPECT_EQ(status.value().DebugString(), expected_result.DebugString());
}

TEST_F(RegexFunctionsTest, CaptureStringSuccessWithMultipleUnamedGroups) {
  // Regex containing all unnamed groups should return a map
  std::vector<std::pair<CelValue, CelValue>> cel_values;
  cel_values.emplace_back(std::make_pair(
      CelValue::CreateString(google::protobuf::Arena::Create<std::string>(&arena_, "1")),
      CelValue::CreateString(
          google::protobuf::Arena::Create<std::string>(&arena_, "testuser"))));
  cel_values.emplace_back(std::make_pair(
      CelValue::CreateString(google::protobuf::Arena::Create<std::string>(&arena_, "2")),
      CelValue::CreateString(
          google::protobuf::Arena::Create<std::string>(&arena_, "testdomain"))));
  auto container_map = google::api::expr::runtime::CreateContainerBackedMap(
      absl::MakeSpan(cel_values));
  // Release ownership of container_map to Arena.
  auto cel_map = container_map->release();
  arena_.Own(cel_map);
  CelValue expected_result = CelValue::CreateMap(cel_map);

  auto status =
      TestCaptureStringInclusion((R"(re.captureN('testuser@testdomain',
                                 '(.*)@([^.]*)'))"));
  ASSERT_OK(status.status());
  EXPECT_EQ(status.value().DebugString(), expected_result.DebugString());
}

// Extract String: Extract named and unnamed strings
TEST_F(RegexFunctionsTest, ExtractStringWithNamedAndUnnamedGroups) {
  auto status = TestCaptureStringInclusion(
      (R"(re.extract('The user testuser belongs to testdomain',
      'The (user|domain) (?P<Username>.*) belongs to (?P<Domain>.*)',
      '\\3 contains \\1 \\2'))"));
  ASSERT_TRUE(status.value().IsString());
  EXPECT_THAT(status,
              IsOkAndHolds(IsCelString("testdomain contains user testuser")));
}

// Extract String: Extract with empty strings
TEST_F(RegexFunctionsTest, ExtractStringWithEmptyStrings) {
  std::string expected_result = "";
  auto status = TestCaptureStringInclusion((R"(re.extract('', '', ''))"));
  ASSERT_TRUE(status.value().IsString());
  EXPECT_THAT(status, IsOkAndHolds(IsCelString(expected_result)));
}

// Extract String: Extract unnamed strings
TEST_F(RegexFunctionsTest, ExtractStringWithUnnamedGroups) {
  auto status = TestCaptureStringInclusion(
      (R"(re.extract('testuser@google.com', '(.*)@([^.]*)', '\\2!\\1'))"));
  EXPECT_THAT(status, IsOkAndHolds(IsCelString("google!testuser")));
}

// Extract String: Extract string with no captured groups
TEST_F(RegexFunctionsTest, ExtractStringWithNoGroups) {
  auto status =
      TestCaptureStringInclusion((R"(re.extract('foo', '.*', '\'\\0\''))"));
  EXPECT_THAT(status, IsOkAndHolds(IsCelString("'foo'")));
}

// Capture String: Success with matching unnamed group
TEST_F(RegexFunctionsTest, CaptureStringWithUnnamedGroups) {
  auto status = TestCaptureStringInclusion((R"(re.capture('foo', 'fo(o)'))"));
  EXPECT_THAT(status, IsOkAndHolds(IsCelString("o")));
}

std::vector<TestCase> createParams() {
  return {
      {// Extract String: Fails for mismatched regex
       (R"(re.extract('foo', 'f(o+)(s)', '\\1\\2'))"),
       "Unable to extract string for the given regex"},
      {// Extract String: Fails when rewritten string has too many placeholders
       (R"(re.extract('foo', 'f(o+)', '\\1\\2'))"),
       "Unable to extract string for the given regex"},
      {// Extract String: Fails when  regex is invalid
       (R"(re.extract('foo', 'f(o+)(abc', '\\1\\2'))"), "Regex is Invalid"},
      {// Capture String: Empty regex
       (R"(re.capture('foo', ''))"),
       "Unable to capture groups for the given regex"},
      {// Capture String: No Capturing groups
       (R"(re.capture('foo', '.*'))"),
       "Unable to capture groups for the given regex"},
      {// Capture String: Mismatched String
       (R"(re.capture('', 'bar'))"),
       "Unable to capture groups for the given regex"},
      {// Capture String: Mismatched groups
       (R"(re.capture('foo', 'fo(o+)(s)'))"),
       "Unable to capture groups for the given regex"},
      {// Capture String: Regex is Invalid
       (R"(re.capture('foo', 'fo(o+)(abc'))"), "Regex is Invalid"},
      {// Capture String N: Empty regex
       (R"(re.captureN('foo', ''))"),
       "Capturing groups were not found in the given regex."},
      {// Capture String N: No Capturing groups
       (R"(re.captureN('foo', '.*'))"),
       "Capturing groups were not found in the given regex."},
      {// Capture String N: Mismatched String
       (R"(re.captureN('', 'bar'))"),
       "Capturing groups were not found in the given regex."},
      {// Capture String N: Mismatched groups
       (R"(re.captureN('foo', 'fo(o+)(s)'))"),
       "Unable to capture groups for the given regex"},
      {// Capture String N: Regex is Invalid
       (R"(re.captureN('foo', 'fo(o+)(abc'))"), "Regex is Invalid"},
  };
}

TEST_P(RegexFunctionsTest, RegexFunctionsTests) {
  const TestCase& test_case = GetParam();
  ABSL_LOG(INFO) << "Testing Cel Expression: " << test_case.expr_string;
  auto status = TestCaptureStringInclusion(test_case.expr_string);
  EXPECT_THAT(
      status.value(),
      IsCelError(StatusIs(absl::StatusCode::kInvalidArgument,
                          testing::HasSubstr(test_case.expected_result))));
}

INSTANTIATE_TEST_SUITE_P(RegexFunctionsTest, RegexFunctionsTest,
                         testing::ValuesIn(createParams()));

}  // namespace

}  // namespace cel::extensions
