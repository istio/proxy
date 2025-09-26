// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "compiler/optional.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "checker/standard_library.h"
#include "checker/type_check_issue.h"
#include "checker/validation_result.h"
#include "common/decl.h"
#include "common/source.h"
#include "common/type.h"
#include "compiler/compiler.h"
#include "compiler/compiler_factory.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "testutil/baseline_tests.h"
#include "cel/expr/conformance/proto3/test_all_types.pb.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::cel::expr::conformance::proto3::TestAllTypes;
using ::cel::test::FormatBaselineAst;
using ::testing::HasSubstr;

struct TestCase {
  std::string expr;
  std::string expected_ast;
};

class OptionalTest : public testing::TestWithParam<TestCase> {};

std::string FormatIssues(const ValidationResult& result) {
  const Source* source = result.GetSource();
  return absl::StrJoin(
      result.GetIssues(), "\n",
      [=](std::string* out, const TypeCheckIssue& issue) {
        absl::StrAppend(
            out, (source) ? issue.ToDisplayString(*source) : issue.message());
      });
}

TEST_P(OptionalTest, OptionalsEnabled) {
  const TestCase& test_case = GetParam();

  ASSERT_OK_AND_ASSIGN(
      auto builder,
      NewCompilerBuilder(cel::internal::GetSharedTestingDescriptorPool()));
  ASSERT_THAT(builder->AddLibrary(StandardCheckerLibrary()), IsOk());
  ASSERT_THAT(builder->AddLibrary(OptionalCompilerLibrary()), IsOk());
  ASSERT_THAT(builder->GetCheckerBuilder().AddVariable(MakeVariableDecl(
                  "msg", MessageType(TestAllTypes::descriptor()))),
              IsOk());

  ASSERT_OK_AND_ASSIGN(auto compiler, std::move(*builder).Build());

  absl::StatusOr<ValidationResult> maybe_result =
      compiler->Compile(test_case.expr);

  ASSERT_OK_AND_ASSIGN(ValidationResult result, std::move(maybe_result));
  ASSERT_TRUE(result.IsValid()) << FormatIssues(result);
  EXPECT_EQ(FormatBaselineAst(*result.GetAst()),
            absl::StripAsciiWhitespace(test_case.expected_ast))
      << test_case.expr;
}

INSTANTIATE_TEST_SUITE_P(
    OptionalTest, OptionalTest,
    ::testing::Values(
        TestCase{
            .expr = "msg.?single_int64",
            .expected_ast = R"(
_?._(
  msg~cel.expr.conformance.proto3.TestAllTypes^msg,
  "single_int64"
)~optional_type(int)^select_optional_field)",
        },
        TestCase{
            .expr = "optional.of('foo')",
            .expected_ast = R"(
optional.of(
  "foo"~string
)~optional_type(string)^optional_of)",
        },
        TestCase{
            .expr = "optional.of('foo').optMap(x, x)",
            .expected_ast = R"(
_?_:_(
  optional.of(
    "foo"~string
  )~optional_type(string)^optional_of.hasValue()~bool^optional_hasValue,
  optional.of(
    __comprehension__(
      // Variable
      #unused,
      // Target
      []~list(dyn),
      // Accumulator
      x,
      // Init
      optional.of(
        "foo"~string
      )~optional_type(string)^optional_of.value()~string^optional_value,
      // LoopCondition
      false~bool,
      // LoopStep
      x~string^x,
      // Result
      x~string^x)~string
  )~optional_type(string)^optional_of,
  optional.none()~optional_type(string)^optional_none
)~optional_type(string)^conditional
)",
        },
        TestCase{
            .expr = "optional.of('foo').optFlatMap(x, optional.of(x))",
            .expected_ast = R"(
_?_:_(
  optional.of(
    "foo"~string
  )~optional_type(string)^optional_of.hasValue()~bool^optional_hasValue,
  __comprehension__(
    // Variable
    #unused,
    // Target
    []~list(dyn),
    // Accumulator
    x,
    // Init
    optional.of(
      "foo"~string
    )~optional_type(string)^optional_of.value()~string^optional_value,
    // LoopCondition
    false~bool,
    // LoopStep
    x~string^x,
    // Result
    optional.of(
      x~string^x
    )~optional_type(string)^optional_of)~optional_type(string),
  optional.none()~optional_type(string)^optional_none
)~optional_type(string)^conditional
)",
        },
        TestCase{
            .expr = "optional.ofNonZeroValue(1)",
            .expected_ast = R"(
optional.ofNonZeroValue(
  1~int
)~optional_type(int)^optional_ofNonZeroValue
)",
        },
        TestCase{
            .expr = "[0][?1]",
            .expected_ast = R"(
_[?_](
  [
    0~int
  ]~list(int),
  1~int
)~optional_type(int)^list_optindex_optional_int
)",
        },
        TestCase{
            .expr = "{0: 2}[?1]",
            .expected_ast = R"(
_[?_](
  {
    0~int:2~int
  }~map(int, int),
  1~int
)~optional_type(int)^map_optindex_optional_value
)",
        },
        TestCase{
            .expr = "msg.?repeated_int64[1]",
            .expected_ast = R"(
_[_](
  _?._(
    msg~cel.expr.conformance.proto3.TestAllTypes^msg,
    "repeated_int64"
  )~optional_type(list(int))^select_optional_field,
  1~int
)~optional_type(int)^optional_list_index_int
)",
        },
        TestCase{
            .expr = "msg.?map_int64_int64[1]",
            .expected_ast = R"(
_[_](
  _?._(
    msg~cel.expr.conformance.proto3.TestAllTypes^msg,
    "map_int64_int64"
  )~optional_type(map(int, int))^select_optional_field,
  1~int
)~optional_type(int)^optional_map_index_value
)",
        },
        TestCase{
            .expr = "optional.of(1).or(optional.of(2))",
            .expected_ast = R"(
optional.of(
  1~int
)~optional_type(int)^optional_of.or(
  optional.of(
    2~int
  )~optional_type(int)^optional_of
)~optional_type(int)^optional_or_optional)",
        },
        TestCase{
            .expr = "optional.of(1).orValue(2)",
            .expected_ast = R"(
optional.of(
  1~int
)~optional_type(int)^optional_of.orValue(
  2~int
)~int^optional_orValue_value
)",
        },
        TestCase{
            .expr = "optional.of(1).value()",
            .expected_ast = R"(
optional.of(
  1~int
)~optional_type(int)^optional_of.value()~int^optional_value
)",
        },
        TestCase{
            .expr = "optional.of(1).hasValue()",
            .expected_ast = R"(
optional.of(
  1~int
)~optional_type(int)^optional_of.hasValue()~bool^optional_hasValue
)",
        }));

TEST(OptionalTest, NotEnabled) {
  ASSERT_OK_AND_ASSIGN(
      auto builder,
      NewCompilerBuilder(cel::internal::GetSharedTestingDescriptorPool()));
  ASSERT_THAT(builder->AddLibrary(StandardCheckerLibrary()), IsOk());
  ASSERT_THAT(builder->GetCheckerBuilder().AddVariable(MakeVariableDecl(
                  "msg", MessageType(TestAllTypes::descriptor()))),
              IsOk());

  ASSERT_OK_AND_ASSIGN(auto compiler, std::move(*builder).Build());

  ASSERT_OK_AND_ASSIGN(auto result, compiler->Compile("optional.of(1)"));

  EXPECT_THAT(FormatIssues(result),
              HasSubstr("undeclared reference to 'optional'"));
}

}  // namespace
}  // namespace cel
