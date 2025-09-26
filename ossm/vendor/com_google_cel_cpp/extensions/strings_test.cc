// Copyright 2024 Google LLC
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

#include "extensions/strings.h"

#include <memory>
#include <utility>

#include "cel/expr/syntax.pb.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/cord.h"
#include "checker/standard_library.h"
#include "checker/type_checker_builder.h"
#include "checker/validation_result.h"
#include "common/decl.h"
#include "common/value.h"
#include "compiler/compiler_factory.h"
#include "compiler/standard_library.h"
#include "extensions/protobuf/runtime_adapter.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "parser/options.h"
#include "parser/parser.h"
#include "runtime/activation.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "testutil/baseline_tests.h"
#include "google/protobuf/arena.h"

namespace cel::extensions {
namespace {

using ::absl_testing::IsOk;
using ::cel::expr::ParsedExpr;
using ::google::api::expr::parser::Parse;
using ::google::api::expr::parser::ParserOptions;
using ::testing::Values;

TEST(Strings, SplitWithEmptyDelimiterCord) {
  google::protobuf::Arena arena;
  const auto options = RuntimeOptions{};
  ASSERT_OK_AND_ASSIGN(auto builder,
                       CreateStandardRuntimeBuilder(
                           internal::GetTestingDescriptorPool(), options));
  EXPECT_THAT(RegisterStringsFunctions(builder.function_registry(), options),
              IsOk());

  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(builder).Build());

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr,
                       Parse("foo.split('') == ['h', 'e', 'l', 'l', 'o', ' ', "
                             "'w', 'o', 'r', 'l', 'd', '!']",
                             "<input>", ParserOptions{}));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       ProtobufRuntimeAdapter::CreateProgram(*runtime, expr));

  Activation activation;
  activation.InsertOrAssignValue("foo",
                                 StringValue{absl::Cord("hello world!")});

  ASSERT_OK_AND_ASSIGN(Value result, program->Evaluate(&arena, activation));
  ASSERT_TRUE(result.Is<BoolValue>());
  EXPECT_TRUE(result.GetBool().NativeValue());
}

TEST(Strings, Replace) {
  google::protobuf::Arena arena;
  const auto options = RuntimeOptions{};
  ASSERT_OK_AND_ASSIGN(auto builder,
                       CreateStandardRuntimeBuilder(
                           internal::GetTestingDescriptorPool(), options));
  EXPECT_THAT(RegisterStringsFunctions(builder.function_registry(), options),
              IsOk());

  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(builder).Build());

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr,
                       Parse("foo.replace('he', 'we') == 'wello wello'",
                             "<input>", ParserOptions{}));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       ProtobufRuntimeAdapter::CreateProgram(*runtime, expr));

  Activation activation;
  activation.InsertOrAssignValue("foo", StringValue{absl::Cord("hello hello")});

  ASSERT_OK_AND_ASSIGN(Value result, program->Evaluate(&arena, activation));
  ASSERT_TRUE(result.Is<BoolValue>());
  EXPECT_TRUE(result.GetBool().NativeValue());
}

TEST(Strings, ReplaceWithNegativeLimit) {
  google::protobuf::Arena arena;
  const auto options = RuntimeOptions{};
  ASSERT_OK_AND_ASSIGN(auto builder,
                       CreateStandardRuntimeBuilder(
                           internal::GetTestingDescriptorPool(), options));
  EXPECT_THAT(RegisterStringsFunctions(builder.function_registry(), options),
              IsOk());

  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(builder).Build());

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr,
                       Parse("foo.replace('he', 'we', -1) == 'wello wello'",
                             "<input>", ParserOptions{}));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       ProtobufRuntimeAdapter::CreateProgram(*runtime, expr));

  Activation activation;
  activation.InsertOrAssignValue("foo", StringValue{absl::Cord("hello hello")});

  ASSERT_OK_AND_ASSIGN(Value result, program->Evaluate(&arena, activation));
  ASSERT_TRUE(result.Is<BoolValue>());
  EXPECT_TRUE(result.GetBool().NativeValue());
}

TEST(Strings, ReplaceWithLimit) {
  google::protobuf::Arena arena;
  const auto options = RuntimeOptions{};
  ASSERT_OK_AND_ASSIGN(auto builder,
                       CreateStandardRuntimeBuilder(
                           internal::GetTestingDescriptorPool(), options));
  EXPECT_THAT(RegisterStringsFunctions(builder.function_registry(), options),
              IsOk());

  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(builder).Build());

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr,
                       Parse("foo.replace('he', 'we', 1) == 'wello hello'",
                             "<input>", ParserOptions{}));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       ProtobufRuntimeAdapter::CreateProgram(*runtime, expr));

  Activation activation;
  activation.InsertOrAssignValue("foo", StringValue{absl::Cord("hello hello")});

  ASSERT_OK_AND_ASSIGN(Value result, program->Evaluate(&arena, activation));
  ASSERT_TRUE(result.Is<BoolValue>());
  EXPECT_TRUE(result.GetBool().NativeValue());
}

TEST(Strings, ReplaceWithZeroLimit) {
  google::protobuf::Arena arena;
  const auto options = RuntimeOptions{};
  ASSERT_OK_AND_ASSIGN(auto builder,
                       CreateStandardRuntimeBuilder(
                           internal::GetTestingDescriptorPool(), options));
  EXPECT_THAT(RegisterStringsFunctions(builder.function_registry(), options),
              IsOk());

  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(builder).Build());

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr,
                       Parse("foo.replace('he', 'we', 0) == 'hello hello'",
                             "<input>", ParserOptions{}));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       ProtobufRuntimeAdapter::CreateProgram(*runtime, expr));

  Activation activation;
  activation.InsertOrAssignValue("foo", StringValue{absl::Cord("hello hello")});

  ASSERT_OK_AND_ASSIGN(Value result, program->Evaluate(&arena, activation));
  ASSERT_TRUE(result.Is<BoolValue>());
  EXPECT_TRUE(result.GetBool().NativeValue());
}

TEST(Strings, LowerAscii) {
  google::protobuf::Arena arena;
  const auto options = RuntimeOptions{};
  ASSERT_OK_AND_ASSIGN(auto builder,
                       CreateStandardRuntimeBuilder(
                           internal::GetTestingDescriptorPool(), options));
  EXPECT_THAT(RegisterStringsFunctions(builder.function_registry(), options),
              IsOk());

  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(builder).Build());

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr,
                       Parse("'UPPER lower'.lowerAscii() == 'upper lower'",
                             "<input>", ParserOptions{}));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       ProtobufRuntimeAdapter::CreateProgram(*runtime, expr));

  Activation activation;
  ASSERT_OK_AND_ASSIGN(Value result, program->Evaluate(&arena, activation));
  ASSERT_TRUE(result.Is<BoolValue>());
  EXPECT_TRUE(result.GetBool().NativeValue());
}

TEST(Strings, UpperAscii) {
  google::protobuf::Arena arena;
  const auto options = RuntimeOptions{};
  ASSERT_OK_AND_ASSIGN(auto builder,
                       CreateStandardRuntimeBuilder(
                           internal::GetTestingDescriptorPool(), options));
  EXPECT_THAT(RegisterStringsFunctions(builder.function_registry(), options),
              IsOk());

  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(builder).Build());

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr,
                       Parse("'UPPER lower'.upperAscii() == 'UPPER LOWER'",
                             "<input>", ParserOptions{}));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       ProtobufRuntimeAdapter::CreateProgram(*runtime, expr));

  Activation activation;
  ASSERT_OK_AND_ASSIGN(Value result, program->Evaluate(&arena, activation));
  ASSERT_TRUE(result.Is<BoolValue>());
  EXPECT_TRUE(result.GetBool().NativeValue());
}

TEST(Strings, Format) {
  google::protobuf::Arena arena;
  const auto options = RuntimeOptions{};
  ASSERT_OK_AND_ASSIGN(auto builder,
                       CreateStandardRuntimeBuilder(
                           internal::GetTestingDescriptorPool(), options));
  EXPECT_THAT(RegisterStringsFunctions(builder.function_registry(), options),
              IsOk());

  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(builder).Build());

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr,
                       Parse("'abc %.3f'.format([2.0]) == 'abc 2.000'",
                             "<input>", ParserOptions{}));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       ProtobufRuntimeAdapter::CreateProgram(*runtime, expr));

  Activation activation;
  ASSERT_OK_AND_ASSIGN(Value result, program->Evaluate(&arena, activation));
  ASSERT_TRUE(result.Is<BoolValue>());
  EXPECT_TRUE(result.GetBool().NativeValue());
}

TEST(StringsCheckerLibrary, SmokeTest) {
  ASSERT_OK_AND_ASSIGN(
      auto builder, NewCompilerBuilder(internal::GetTestingDescriptorPool()));
  ASSERT_THAT(builder->AddLibrary(StringsCheckerLibrary()), IsOk());
  ASSERT_THAT(builder->AddLibrary(StandardCheckerLibrary()), IsOk());
  ASSERT_THAT(builder->GetCheckerBuilder().AddVariable(
                  MakeVariableDecl("foo", StringType())),
              IsOk());

  ASSERT_OK_AND_ASSIGN(auto compiler, std::move(*builder).Build());

  ASSERT_OK_AND_ASSIGN(
      ValidationResult result,
      compiler->Compile("foo.replace('he', 'we', 1) == 'wello hello'"));
  ASSERT_TRUE(result.IsValid());

  EXPECT_EQ(test::FormatBaselineAst(*result.GetAst()),
            R"(_==_(
  foo~string^foo.replace(
    "he"~string,
    "we"~string,
    1~int
  )~string^string_replace_string_string_int,
  "wello hello"~string
)~bool^equals)");
}

// Basic test for the included declarations.
// Additional coverage for behavior in the spec tests.
class StringsCheckerLibraryTest : public ::testing::TestWithParam<std::string> {
};

TEST_P(StringsCheckerLibraryTest, TypeChecks) {
  const std::string& expr = GetParam();
  ASSERT_OK_AND_ASSIGN(
      auto builder, NewCompilerBuilder(internal::GetTestingDescriptorPool()));
  ASSERT_THAT(builder->AddLibrary(StringsCompilerLibrary()), IsOk());
  ASSERT_THAT(builder->AddLibrary(StandardCompilerLibrary()), IsOk());

  ASSERT_OK_AND_ASSIGN(auto compiler, std::move(*builder).Build());

  ASSERT_OK_AND_ASSIGN(ValidationResult result, compiler->Compile(expr));
  EXPECT_TRUE(result.IsValid()) << "Failed to compile: " << expr;
}

INSTANTIATE_TEST_SUITE_P(
    Expressions, StringsCheckerLibraryTest,
    Values("['a', 'b', 'c'].join() == 'abc'",
           "['a', 'b', 'c'].join('|') == 'a|b|c'",
           "'a|b|c'.split('|') == ['a', 'b', 'c']",
           "'a|b|c'.split('|', 1) == ['a', 'b|c']",
           "'a|b|c'.split('|') == ['a', 'b', 'c']",
           "'AbC'.lowerAscii() == 'abc'",
           "'tacocat'.replace('cat', 'dog') == 'tacodog'",
           "'tacocat'.replace('aco', 'an', 2) == 'tacocat'",
           "'tacocat'.charAt(2) == 'c'", "'tacocat'.indexOf('c') == 2",
           "'tacocat'.indexOf('c', 3) == 4", "'tacocat'.lastIndexOf('c') == 4",
           "'tacocat'.lastIndexOf('c', 5) == -1",
           "'tacocat'.substring(1) == 'acocat'",
           "'tacocat'.substring(1, 3) == 'aco'", "'aBc'.upperAscii() == 'ABC'",
           "'abc %d'.format([2]) == 'abc 2'",
           "strings.quote('abc') == \"'abc 2'\"", "'abc'.reverse() == 'cba'"));

}  // namespace
}  // namespace cel::extensions
