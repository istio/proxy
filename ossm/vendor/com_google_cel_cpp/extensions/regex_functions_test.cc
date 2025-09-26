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

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "checker/standard_library.h"
#include "checker/validation_result.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "compiler/compiler.h"
#include "compiler/compiler_factory.h"
#include "extensions/protobuf/runtime_adapter.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "parser/parser.h"
#include "runtime/activation.h"
#include "runtime/reference_resolver.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/extension_set.h"
#include "google/protobuf/message.h"

namespace cel::extensions {

namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::cel::test::ErrorValueIs;
using ::cel::test::MapValueElements;
using ::cel::test::MapValueIs;
using ::cel::test::StringValueIs;
using ::google::api::expr::parser::Parse;
using ::testing::HasSubstr;
using ::testing::UnorderedElementsAre;
using ::testing::ValuesIn;

struct TestCase {
  const std::string expr_string;
  const std::string expected_result;
};

class RegexFunctionsTest : public ::testing::TestWithParam<TestCase> {
 public:
  void SetUp() override {
    RuntimeOptions options;
    options.enable_regex = true;
    options.enable_qualified_type_identifiers = true;

    ASSERT_OK_AND_ASSIGN(
        RuntimeBuilder builder,
        CreateStandardRuntimeBuilder(descriptor_pool_, options));
    ASSERT_THAT(
        EnableReferenceResolver(builder, ReferenceResolverEnabled::kAlways),
        IsOk());
    ASSERT_THAT(RegisterRegexFunctions(builder.function_registry(), options),
                IsOk());
    ASSERT_OK_AND_ASSIGN(runtime_, std::move(builder).Build());
  }

  absl::StatusOr<Value> TestEvaluate(const std::string& expr_string) {
    CEL_ASSIGN_OR_RETURN(auto parsed_expr, Parse(expr_string));
    CEL_ASSIGN_OR_RETURN(std::unique_ptr<cel::Program> program,
                         cel::extensions::ProtobufRuntimeAdapter::CreateProgram(
                             *runtime_, parsed_expr));
    Activation activation;
    return program->Evaluate(&arena_, activation);
  }

  const google::protobuf::DescriptorPool* descriptor_pool_ =
      internal::GetTestingDescriptorPool();
  google::protobuf::MessageFactory* message_factory_ =
      google::protobuf::MessageFactory::generated_factory();
  google::protobuf::Arena arena_;
  std::unique_ptr<const Runtime> runtime_;
};

TEST_F(RegexFunctionsTest, CaptureStringSuccessWithCombinationOfGroups) {
  // combination of named and unnamed groups should return a celmap
  EXPECT_THAT(
      TestEvaluate((R"cel(
        re.captureN(
          'The user testuser belongs to testdomain',
          'The (user|domain) (?P<Username>.*) belongs to (?P<Domain>.*)'
        )
      )cel")),
      IsOkAndHolds(MapValueIs(MapValueElements(
          UnorderedElementsAre(
              Pair(StringValueIs("1"), StringValueIs("user")),
              Pair(StringValueIs("Username"), StringValueIs("testuser")),
              Pair(StringValueIs("Domain"), StringValueIs("testdomain"))),
          descriptor_pool_, message_factory_, &arena_))));
}

TEST_F(RegexFunctionsTest, CaptureStringSuccessWithSingleNamedGroup) {
  // Regex containing one named group should return a map
  EXPECT_THAT(
      TestEvaluate(R"cel(re.captureN('testuser@', '(?P<username>.*)@'))cel"),
      IsOkAndHolds(MapValueIs(MapValueElements(
          UnorderedElementsAre(
              Pair(StringValueIs("username"), StringValueIs("testuser"))),
          descriptor_pool_, message_factory_, &arena_))));
}

TEST_F(RegexFunctionsTest, CaptureStringSuccessWithMultipleUnamedGroups) {
  // Regex containing all unnamed groups should return a map
  EXPECT_THAT(
      TestEvaluate(
          R"cel(re.captureN('testuser@testdomain', '(.*)@([^.]*)'))cel"),
      IsOkAndHolds(MapValueIs(MapValueElements(
          UnorderedElementsAre(
              Pair(StringValueIs("1"), StringValueIs("testuser")),
              Pair(StringValueIs("2"), StringValueIs("testdomain"))),
          descriptor_pool_, message_factory_, &arena_))));
}

// Extract String: Extract named and unnamed strings
TEST_F(RegexFunctionsTest, ExtractStringWithNamedAndUnnamedGroups) {
  EXPECT_THAT(TestEvaluate(R"cel(
      re.extract(
        'The user testuser belongs to testdomain',
        'The (user|domain) (?P<Username>.*) belongs to (?P<Domain>.*)',
        '\\3 contains \\1 \\2')
    )cel"),
              IsOkAndHolds(StringValueIs("testdomain contains user testuser")));
}

// Extract String: Extract with empty strings
TEST_F(RegexFunctionsTest, ExtractStringWithEmptyStrings) {
  EXPECT_THAT(TestEvaluate(R"cel(re.extract('', '', ''))cel"),
              IsOkAndHolds(StringValueIs("")));
}

// Extract String: Extract unnamed strings
TEST_F(RegexFunctionsTest, ExtractStringWithUnnamedGroups) {
  EXPECT_THAT(TestEvaluate(R"cel(
      re.extract('testuser@google.com', '(.*)@([^.]*)', '\\2!\\1')
    )cel"),
              IsOkAndHolds(StringValueIs("google!testuser")));
}

// Extract String: Extract string with no captured groups
TEST_F(RegexFunctionsTest, ExtractStringWithNoGroups) {
  EXPECT_THAT(TestEvaluate(R"cel(re.extract('foo', '.*', '\'\\0\''))cel"),
              IsOkAndHolds(StringValueIs("'foo'")));
}

// Capture String: Success with matching unnamed group
TEST_F(RegexFunctionsTest, CaptureStringWithUnnamedGroups) {
  EXPECT_THAT(TestEvaluate(R"cel(re.capture('foo', 'fo(o)'))cel"),
              IsOkAndHolds(StringValueIs("o")));
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
  EXPECT_THAT(TestEvaluate(test_case.expr_string),
              IsOkAndHolds(ErrorValueIs(
                  StatusIs(absl::StatusCode::kInvalidArgument,
                           HasSubstr(test_case.expected_result)))));
}

INSTANTIATE_TEST_SUITE_P(RegexFunctionsTest, RegexFunctionsTest,
                         ValuesIn(createParams()));

struct RegexCheckerTestCase {
  const std::string expr_string;
  bool is_valid;
};

class RegexCheckerLibraryTest
    : public ::testing::TestWithParam<RegexCheckerTestCase> {
 public:
  void SetUp() override {
    // Arrange: Configure the compiler.
    // Add the regex checker library to the compiler builder.
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<CompilerBuilder> compiler_builder,
                         NewCompilerBuilder(descriptor_pool_));
    ASSERT_THAT(compiler_builder->AddLibrary(StandardCheckerLibrary()), IsOk());
    ASSERT_THAT(compiler_builder->AddLibrary(RegexCheckerLibrary()), IsOk());
    ASSERT_OK_AND_ASSIGN(compiler_, std::move(*compiler_builder).Build());
  }

  const google::protobuf::DescriptorPool* descriptor_pool_ =
      internal::GetTestingDescriptorPool();
  std::unique_ptr<Compiler> compiler_;
};

TEST_P(RegexCheckerLibraryTest, RegexFunctionsTypeCheckerSuccess) {
  // Act & Assert: Compile the expression and validate the result.
  ASSERT_OK_AND_ASSIGN(ValidationResult result,
                       compiler_->Compile(GetParam().expr_string));
  EXPECT_EQ(result.IsValid(), GetParam().is_valid);
}

// Returns a vector of test cases for the RegexCheckerLibraryTest.
// Returns both positive and negative test cases for the regex functions.
std::vector<RegexCheckerTestCase> createRegexCheckerParams() {
  return {
      {R"(re.extract('testuser@google.com', '(.*)@([^.]*)', '\\2!\\1') == 'google!testuser')",
       true},
      {R"(re.extract(1, '(.*)@([^.]*)', '\\2!\\1') == 'google!testuser')",
       false},
      {R"(re.extract('testuser@google.com', ['1', '2'], '\\2!\\1') == 'google!testuser')",
       false},
      {R"(re.extract('testuser@google.com', '(.*)@([^.]*)', false) == 'google!testuser')",
       false},
      {R"(re.extract('testuser@google.com', '(.*)@([^.]*)', '\\2!\\1') == 2.2)",
       false},
      {R"(re.captureN('testuser@', '(?P<username>.*)@') == {'username': 'testuser'})",
       true},
      {R"(re.captureN(['foo', 'bar'], '(?P<username>.*)@') == {'username': 'testuser'})",
       false},
      {R"(re.captureN('testuser@', 2) == {'username': 'testuser'})", false},
      {R"(re.captureN('testuser@', '(?P<username>.*)@') == true)", false},
      {R"(re.capture('foo', 'fo(o)') == 'o')", true},
      {R"(re.capture('foo', 2) == 'o')", false},
      {R"(re.capture(true, 'fo(o)') == 'o')", false},
      {R"(re.capture('foo', 'fo(o)') == ['o'])", false},
  };
}

INSTANTIATE_TEST_SUITE_P(RegexCheckerLibraryTest, RegexCheckerLibraryTest,
                         ValuesIn(createRegexCheckerParams()));

}  // namespace

}  // namespace cel::extensions
