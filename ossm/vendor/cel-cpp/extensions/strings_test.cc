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
#include <string>
#include <utility>

#include "cel/expr/syntax.pb.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "checker/standard_library.h"
#include "checker/type_checker_builder.h"
#include "checker/validation_result.h"
#include "common/decl.h"
#include "common/type.h"
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

using StringsExtFunctionsTest = testing::TestWithParam<std::string>;

TEST_P(StringsExtFunctionsTest, ParserAndCheckerTests) {
  const std::string& expr = GetParam();

  ASSERT_OK_AND_ASSIGN(
      auto compiler_builder,
      NewCompilerBuilder(internal::GetTestingDescriptorPool()));

  ASSERT_THAT(compiler_builder->AddLibrary(StandardCompilerLibrary()), IsOk());
  ASSERT_THAT(compiler_builder->AddLibrary(StringsCompilerLibrary()), IsOk());

  ASSERT_OK_AND_ASSIGN(auto compiler, std::move(*compiler_builder).Build());

  auto result = compiler->Compile(expr, "<input>");

  ASSERT_THAT(result, IsOk());
  ASSERT_TRUE(result->IsValid());

  RuntimeOptions opts;
  ASSERT_OK_AND_ASSIGN(
      auto runtime_builder,
      CreateStandardRuntimeBuilder(internal::GetTestingDescriptorPool(), opts));

  ASSERT_THAT(
      RegisterStringsFunctions(runtime_builder.function_registry(), opts),
      IsOk());

  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(runtime_builder).Build());

  ASSERT_OK_AND_ASSIGN(auto program,
                       runtime->CreateProgram(*result->ReleaseAst()));

  google::protobuf::Arena arena;
  cel::Activation activation;
  ASSERT_OK_AND_ASSIGN(auto value, program->Evaluate(&arena, activation));

  ASSERT_TRUE(value.Is<BoolValue>());
  EXPECT_TRUE(value.GetBool().NativeValue());
}

INSTANTIATE_TEST_SUITE_P(
    StringsExtMacrosParamsTest, StringsExtFunctionsTest,
    testing::Values(
        // Tests for split()
        "'hello world!'.split('') == ['h', 'e', 'l', 'l', 'o', ' ', "
        "'w', 'o', 'r', 'l', 'd', '!']",
        // Tests for replace()
        "'hello hello'.replace('he', 'we') == 'wello wello'",
        "'hello hello'.replace('he', 'we', -1) == 'wello wello'",
        "'hello hello'.replace('he', 'we', 1) == 'wello hello'",
        "'hello hello'.replace('he', 'we', 0) == 'hello hello'",
        // Tests for lowerAscii()
        "'UPPER lower'.lowerAscii() == 'upper lower'",
        // Tests for upperAscii()
        "'UPPER lower'.upperAscii() == 'UPPER LOWER'",
        // Tests for format()
        "'abc %.3f'.format([2.0]) == 'abc 2.000'",

        // Tests for charAt()
        "'tacocat'.charAt(3) == 'o'", "'tacocat'.charAt(7) == ''",
        "'©αT'.charAt(0) == '©' && '©αT'.charAt(1) == 'α' && '©αT'.charAt(2) "
        "== 'T'",

        // Tests for indexOf()
        "'tacocat'.indexOf('') == 0", "'tacocat'.indexOf('ac') == 1",
        "'tacocat'.indexOf('none') == -1", "'tacocat'.indexOf('', 3) == 3",
        "'tacocat'.indexOf('a', 3) == 5", "'tacocat'.indexOf('at', 3) == 5",
        "'ta©o©αT'.indexOf('©') == 2", "'ta©o©αT'.indexOf('©', 3) == 4",
        "'ta©o©αT'.indexOf('©αT', 3) == 4", "'ta©o©αT'.indexOf('©α', 5) == -1",
        "'ijk'.indexOf('k') == 2", "'hello wello'.indexOf('hello wello') == 0",
        "'hello wello'.indexOf('ello', 6) == 7",
        "'hello wello'.indexOf('elbo room!!') == -1",
        "'hello wello'.indexOf('elbo room!!!') == -1",
        "''.lastIndexOf('@@') == -1", "'tacocat'.lastIndexOf('') == 7",
        "'tacocat'.lastIndexOf('at') == 5",
        "'tacocat'.lastIndexOf('none') == -1",
        "'tacocat'.lastIndexOf('', 3) == 3",
        "'tacocat'.lastIndexOf('a', 3) == 1", "'ta©o©αT'.lastIndexOf('©') == 4",
        "'ta©o©αT'.lastIndexOf('©', 3) == 2",
        "'ta©o©αT'.lastIndexOf('©α', 4) == 4",
        "'hello wello'.lastIndexOf('ello', 6) == 1",
        "'hello wello'.lastIndexOf('low') == -1",
        "'hello wello'.lastIndexOf('elbo room!!') == -1",
        "'hello wello'.lastIndexOf('elbo room!!!') == -1",
        "'hello wello'.lastIndexOf('hello wello') == 0",
        "'bananananana'.lastIndexOf('nana', 7) == 6",

        // Tests for substring()
        "'tacocat'.substring(4) == 'cat'", "'tacocat'.substring(7) == ''",
        "'tacocat'.substring(0, 4) == 'taco'",
        "'tacocat'.substring(4, 4) == ''",
        "'ta©o©αT'.substring(2, 6) == '©o©α'",
        "'ta©o©αT'.substring(7, 7) == ''",

        // Tests for reverse()
        "''.reverse() == ''", "'hello'.reverse() == 'olleh'",
        "'©αT'.reverse() == 'Tα©'", "'gums'.reverse() == 'smug'",
        "'palindromes'.reverse() == 'semordnilap'",
        "'John Smith'.reverse() == 'htimS nhoJ'",
        "'u180etext'.reverse() == 'txete081u'",
        "'2600+U'.reverse() == 'U+0062'",
        "'\u180e\u200b\u200c\u200d\u2060\ufeff'.reverse() == "
        "'\ufeff\u2060\u200d\u200c\u200b\u180e'",

        // Tests for strings.quote()
        R"(strings.quote("first\nsecond") == "\"first\\nsecond\"")",
        R"(strings.quote("bell\a") == "\"bell\\a\"")",
        R"(strings.quote("\bbackspace") == "\"\\bbackspace\"")",
        R"(strings.quote("\fform feed") == "\"\\fform feed\"")",
        R"(strings.quote("carriage \r return") == "\"carriage \\r return\"")",
        R"(strings.quote("vertical \v tab") == "\"vertical \\v tab\"")",
        R"(strings.quote("verbatim") == "\"verbatim\"")",
        R"(strings.quote("ends with \\") == "\"ends with \\\\\"")",
        R"(strings.quote("\\ starts with") == "\"\\\\ starts with\"")",

        // Tests for trim()
        R"(' \f\n\r\t\vtext  '.trim() == 'text')",
        R"('\u0085\u00a0\u1680text'.trim() == 'text')",
        R"('text\u2000\u2001\u2002\u2003\u2004\u2004\u2006\u2007\u2008\u2009'.trim() == 'text')",
        R"('\u200atext\u2028\u2029\u202F\u205F\u3000'.trim() == 'text')",
        R"('  hello world  '.trim() == 'hello world')"));

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
           "strings.quote('abc') == \"'abc 2'\"", "'abc'.reverse() == 'cba'",
           "'ta©o©αT'.substring(7, 7) == ''"));

class StringsOverloadNotFoundTest
    : public ::testing::TestWithParam<std::string> {};

TEST_P(StringsOverloadNotFoundTest, PlannerTests) {
  const std::string& expr_string = GetParam();
  const auto options = RuntimeOptions{};
  ASSERT_OK_AND_ASSIGN(auto builder,
                       CreateStandardRuntimeBuilder(
                           internal::GetTestingDescriptorPool(), options));
  EXPECT_THAT(RegisterStringsFunctions(builder.function_registry(), options),
              IsOk());

  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(builder).Build());

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr,
                       Parse(expr_string, "<input>", ParserOptions{}));

  EXPECT_THAT(
      ProtobufRuntimeAdapter::CreateProgram(*runtime, expr),
      absl_testing::StatusIs(absl::StatusCode::kInvalidArgument,
                             testing::HasSubstr("No overloads provided")));
}

INSTANTIATE_TEST_SUITE_P(
    OverloadNotFound, StringsOverloadNotFoundTest,
    Values(
        // string_ext.type_errors/indexof_ternary_invalid_arguments
        "'42'.indexOf('4', 0, 1) == 0",
        // string_ext.type_errors/replace_quaternary_invalid_argument
        "'42'.replace('2', '1', 1, false) == '41'",
        // string_ext.type_errors/split_ternary_invalid_argument
        "'42'.split('2', 1, 1) == ['4']",
        // string_ext.type_errors/substring_ternary_invalid_argument
        "'hello'.substring(1, 2, 3) == ''"));

class StringsRuntimeErrorTest : public ::testing::TestWithParam<std::string> {};

TEST_P(StringsRuntimeErrorTest, EvaluationErrors) {
  const std::string& expr = GetParam();

  ASSERT_OK_AND_ASSIGN(
      auto compiler_builder,
      NewCompilerBuilder(internal::GetTestingDescriptorPool()));

  ASSERT_THAT(compiler_builder->AddLibrary(StandardCompilerLibrary()), IsOk());
  ASSERT_THAT(compiler_builder->AddLibrary(StringsCompilerLibrary()), IsOk());

  ASSERT_OK_AND_ASSIGN(auto compiler, std::move(*compiler_builder).Build());

  auto result = compiler->Compile(expr, "<input>");

  ASSERT_THAT(result, IsOk());
  ASSERT_TRUE(result->IsValid());

  RuntimeOptions opts;
  ASSERT_OK_AND_ASSIGN(
      auto runtime_builder,
      CreateStandardRuntimeBuilder(internal::GetTestingDescriptorPool(), opts));

  ASSERT_THAT(
      RegisterStringsFunctions(runtime_builder.function_registry(), opts),
      IsOk());

  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(runtime_builder).Build());

  ASSERT_OK_AND_ASSIGN(auto program,
                       runtime->CreateProgram(*result->ReleaseAst()));

  google::protobuf::Arena arena;
  cel::Activation activation;
  ASSERT_OK_AND_ASSIGN(auto value, program->Evaluate(&arena, activation));

  ASSERT_TRUE(value.Is<ErrorValue>());
  EXPECT_THAT(value.As<ErrorValue>()->NativeValue().code(),
              absl::StatusCode::kInvalidArgument);
}

INSTANTIATE_TEST_SUITE_P(EvaluationErrors, StringsRuntimeErrorTest,
                         Values("'a'.substring(-1)", "'a'.substring(2)",
                                "'a'.substring(0, -1)", "'a'.substring(0, 2)",
                                "'a'.substring(1, 0)"));

}  // namespace
}  // namespace cel::extensions
