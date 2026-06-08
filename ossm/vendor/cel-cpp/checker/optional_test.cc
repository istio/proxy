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

#include "checker/optional.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/status_matchers.h"
#include "absl/strings/str_join.h"
#include "checker/checker_options.h"
#include "checker/internal/test_ast_helpers.h"
#include "checker/standard_library.h"
#include "checker/type_check_issue.h"
#include "checker/type_checker.h"
#include "checker/type_checker_builder.h"
#include "checker/type_checker_builder_factory.h"
#include "common/ast.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::cel::checker_internal::MakeTestParsedAst;
using ::cel::internal::GetSharedTestingDescriptorPool;
using ::testing::_;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Key;
using ::testing::Not;
using ::testing::Property;
using ::testing::SizeIs;

MATCHER_P(IsOptionalType, inner_type, "") {
  const TypeSpec& type = arg;
  if (!type.has_abstract_type()) {
    return false;
  }
  const auto& abs_type = type.abstract_type();
  if (abs_type.name() != "optional_type") {
    *result_listener << "expected optional_type, got: " << abs_type.name();
    return false;
  }
  if (abs_type.parameter_types().size() != 1) {
    *result_listener << "unexpected number of parameters: "
                     << abs_type.parameter_types().size();
    return false;
  }

  if (inner_type == abs_type.parameter_types()[0]) {
    return true;
  }

  *result_listener << "unexpected inner type: "
                   << abs_type.parameter_types()[0].type_kind().index();
  return false;
}

TEST(OptionalTest, OptSelectDoesNotAnnotateFieldType) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool()));
  ASSERT_THAT(builder->AddLibrary(StandardCheckerLibrary()), IsOk());
  ASSERT_THAT(builder->AddLibrary(OptionalCheckerLibrary()), IsOk());
  builder->set_container("cel.expr.conformance.proto3");
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<TypeChecker> checker,
                       std::move(*builder).Build());

  ASSERT_OK_AND_ASSIGN(auto ast,
                       MakeTestParsedAst("TestAllTypes{}.?single_int64"));

  ASSERT_OK_AND_ASSIGN(auto result, checker->Check(std::move(ast)));

  EXPECT_THAT(result.GetIssues(), IsEmpty());
  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());

  ASSERT_THAT(checked_ast->root_expr().call_expr().args(), SizeIs(2));
  int64_t field_id = checked_ast->root_expr().call_expr().args()[1].id();
  EXPECT_NE(field_id, 0);

  EXPECT_THAT(checked_ast->type_map(), Not(Contains(Key(field_id))));
  EXPECT_THAT(checked_ast->GetTypeOrDyn(checked_ast->root_expr().id()),
              IsOptionalType(TypeSpec(PrimitiveType::kInt64)));
}

struct TestCase {
  std::string expr;
  testing::Matcher<TypeSpec> result_type_matcher;
  std::string error_substring;
};

class OptionalTest : public testing::TestWithParam<TestCase> {};

TEST_P(OptionalTest, Runner) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool()));
  const TestCase& test_case = GetParam();
  ASSERT_THAT(builder->AddLibrary(StandardCheckerLibrary()), IsOk());
  ASSERT_THAT(builder->AddLibrary(OptionalCheckerLibrary()), IsOk());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<TypeChecker> checker,
                       std::move(*builder).Build());

  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst(test_case.expr));

  ASSERT_OK_AND_ASSIGN(auto result, checker->Check(std::move(ast)));

  if (!test_case.error_substring.empty()) {
    EXPECT_THAT(result.GetIssues(),
                Contains(Property(&TypeCheckIssue::message,
                                  HasSubstr(test_case.error_substring))))
        << absl::StrJoin(result.GetIssues(), "\n",
                         [](std::string* out, const auto& i) {
                           absl::StrAppend(out, i.message());
                         });
    return;
  }

  EXPECT_THAT(result.GetIssues(), IsEmpty())
      << "for expression: " << test_case.expr;
  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());

  int64_t root_id = checked_ast->root_expr().id();

  EXPECT_THAT(checked_ast->GetTypeOrDyn(root_id), test_case.result_type_matcher)
      << "for expression: " << test_case.expr;
}

INSTANTIATE_TEST_SUITE_P(
    OptionalTests, OptionalTest,
    ::testing::Values(
        TestCase{
            "optional.of('abc')",
            IsOptionalType(TypeSpec(PrimitiveType::kString)),
        },
        TestCase{
            "optional.ofNonZeroValue('')",
            IsOptionalType(TypeSpec(PrimitiveType::kString)),
        },
        TestCase{
            "optional.none()",
            IsOptionalType(TypeSpec(DynTypeSpec())),
        },
        // Odd case -- the correct result might be a bespoke recursively-defined
        // type but CEL doesn't support that. Null is used because it is
        // implicitly assignable to optional types. This allows for a recursive
        // type to be non-trivial and verify the checker is actually avoiding
        // introducing a cyclic type.
        TestCase{
            "[optional.none()].map(x, [?x, null, x])",
            Eq(TypeSpec(ListTypeSpec(std::make_unique<TypeSpec>(
                ListTypeSpec(std::make_unique<TypeSpec>(NullTypeSpec())))))),
        },
        TestCase{
            "optional.of('abc').hasValue()",
            Eq(TypeSpec(PrimitiveType::kBool)),
        },
        TestCase{
            "optional.of('abc').value()",
            Eq(TypeSpec(PrimitiveType::kString)),
        },
        TestCase{
            "type(optional.of('abc')) == optional_type",
            Eq(TypeSpec(PrimitiveType::kBool)),
        },
        TestCase{
            "type(optional.of('abc')) == optional_type",
            Eq(TypeSpec(PrimitiveType::kBool)),
        },
        TestCase{
            "optional.of('abc').or(optional.of('def'))",
            IsOptionalType(TypeSpec(PrimitiveType::kString)),
        },
        TestCase{"optional.of('abc').or(optional.of(1))", _,
                 "no matching overload for 'or'"},
        TestCase{
            "optional.of('abc').orValue('def')",
            Eq(TypeSpec(PrimitiveType::kString)),
        },
        TestCase{"optional.of('abc').orValue(1)", _,
                 "no matching overload for 'orValue'"},
        TestCase{
            "{'k': 'v'}.?k",
            IsOptionalType(TypeSpec(PrimitiveType::kString)),
        },
        TestCase{"1.?k", _,
                 "expression of type 'int' cannot be the operand of a select "
                 "operation"},
        TestCase{
            "{'k': {'k': 'v'}}.?k.?k2",
            IsOptionalType(TypeSpec(PrimitiveType::kString)),
        },
        TestCase{
            "{'k': {'k': 'v'}}.?k.k2",
            IsOptionalType(TypeSpec(PrimitiveType::kString)),
        },
        TestCase{"{?'k': optional.of('v')}",
                 Eq(TypeSpec(MapTypeSpec(std::unique_ptr<TypeSpec>(new TypeSpec(
                                             PrimitiveType::kString)),
                                         std::unique_ptr<TypeSpec>(new TypeSpec(
                                             PrimitiveType::kString)))))},
        TestCase{"{'k': 'v', ?'k2': optional.none()}",
                 Eq(TypeSpec(MapTypeSpec(std::unique_ptr<TypeSpec>(new TypeSpec(
                                             PrimitiveType::kString)),
                                         std::unique_ptr<TypeSpec>(new TypeSpec(
                                             PrimitiveType::kString)))))},
        TestCase{"{'k': 'v', ?'k2': 'v'}", _,
                 "expected type 'optional_type(string)' but found 'string'"},
        TestCase{"[?optional.of('v')]",
                 Eq(TypeSpec(ListTypeSpec(std::unique_ptr<TypeSpec>(
                     new TypeSpec(PrimitiveType::kString)))))},
        TestCase{"['v', ?optional.none()]",
                 Eq(TypeSpec(ListTypeSpec(std::unique_ptr<TypeSpec>(
                     new TypeSpec(PrimitiveType::kString)))))},
        TestCase{"['v1', ?'v2']", _,
                 "expected type 'optional_type(string)' but found 'string'"},
        TestCase{"[optional.of(dyn('1')), optional.of('2')][0]",
                 IsOptionalType(TypeSpec(DynTypeSpec()))},
        TestCase{"[optional.of('1'), optional.of(dyn('2'))][0]",
                 IsOptionalType(TypeSpec(DynTypeSpec()))},
        TestCase{"[{1: optional.of(1)}, {1: optional.of(dyn(1))}][0][1]",
                 IsOptionalType(TypeSpec(DynTypeSpec()))},
        TestCase{"[{1: optional.of(dyn(1))}, {1: optional.of(1)}][0][1]",
                 IsOptionalType(TypeSpec(DynTypeSpec()))},
        TestCase{"[optional.of('1'), optional.of(2)][0]",
                 Eq(TypeSpec(DynTypeSpec()))},
        TestCase{"['v1', ?'v2']", _,
                 "expected type 'optional_type(string)' but found 'string'"},
        TestCase{"cel.expr.conformance.proto3.TestAllTypes{?single_int64: "
                 "optional.of(1)}",
                 Eq(TypeSpec(MessageTypeSpec(
                     "cel.expr.conformance.proto3.TestAllTypes")))},
        TestCase{"[0][?1]", IsOptionalType(TypeSpec(PrimitiveType::kInt64))},
        TestCase{"[[0]][?1][?1]",
                 IsOptionalType(TypeSpec(PrimitiveType::kInt64))},
        TestCase{"[[0]][?1][1]",
                 IsOptionalType(TypeSpec(PrimitiveType::kInt64))},
        TestCase{"{0: 1}[?1]", IsOptionalType(TypeSpec(PrimitiveType::kInt64))},
        TestCase{"{0: {0: 1}}[?1][?1]",
                 IsOptionalType(TypeSpec(PrimitiveType::kInt64))},
        TestCase{"{0: {0: 1}}[?1][1]",
                 IsOptionalType(TypeSpec(PrimitiveType::kInt64))},
        TestCase{"{0: {0: 1}}[?1]['']", _, "no matching overload for '_[_]'"},
        TestCase{"{0: {0: 1}}[?1][?'']", _, "no matching overload for '_[?_]'"},
        TestCase{"optional.of('abc').optMap(x, x + 'def')",
                 IsOptionalType(TypeSpec(PrimitiveType::kString))},
        TestCase{"optional.of('abc').optFlatMap(x, optional.of(x + 'def'))",
                 IsOptionalType(TypeSpec(PrimitiveType::kString))},
        // Legacy nullability behaviors.
        TestCase{"cel.expr.conformance.proto3.TestAllTypes{?null_value: "
                 "optional.of(0)}",
                 Eq(TypeSpec(MessageTypeSpec(
                     "cel.expr.conformance.proto3.TestAllTypes")))},
        TestCase{"cel.expr.conformance.proto3.TestAllTypes{?null_value: null}",
                 Eq(TypeSpec(MessageTypeSpec(
                     "cel.expr.conformance.proto3.TestAllTypes")))},
        TestCase{"cel.expr.conformance.proto3.TestAllTypes{?null_value: "
                 "optional.of(null)}",
                 Eq(TypeSpec(MessageTypeSpec(
                     "cel.expr.conformance.proto3.TestAllTypes")))},
        TestCase{"cel.expr.conformance.proto3.TestAllTypes{}.?single_int64 "
                 "== null",
                 Eq(TypeSpec(PrimitiveType::kBool))}));

class OptionalStrictNullAssignmentTest
    : public testing::TestWithParam<TestCase> {};

TEST_P(OptionalStrictNullAssignmentTest, Runner) {
  CheckerOptions options;
  options.enable_legacy_null_assignment = false;
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool(), options));
  const TestCase& test_case = GetParam();
  ASSERT_THAT(builder->AddLibrary(StandardCheckerLibrary()), IsOk());
  ASSERT_THAT(builder->AddLibrary(OptionalCheckerLibrary()), IsOk());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<TypeChecker> checker,
                       std::move(*builder).Build());

  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst(test_case.expr));

  ASSERT_OK_AND_ASSIGN(auto result, checker->Check(std::move(ast)));

  if (!test_case.error_substring.empty()) {
    EXPECT_THAT(result.GetIssues(),
                Contains(Property(&TypeCheckIssue::message,
                                  HasSubstr(test_case.error_substring))))
        << absl::StrJoin(result.GetIssues(), "\n",
                         [](std::string* out, const auto& i) {
                           absl::StrAppend(out, i.message());
                         });
    return;
  }

  EXPECT_THAT(result.GetIssues(), IsEmpty())
      << "for expression: " << test_case.expr;
  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());

  int64_t root_id = checked_ast->root_expr().id();

  EXPECT_THAT(checked_ast->GetTypeOrDyn(root_id), test_case.result_type_matcher)
      << "for expression: " << test_case.expr;
}

INSTANTIATE_TEST_SUITE_P(
    OptionalTests, OptionalStrictNullAssignmentTest,
    ::testing::Values(
        TestCase{
            "cel.expr.conformance.proto3.TestAllTypes{?single_int64: null}", _,
            "expected type of field 'single_int64' is 'optional_type(int)' but "
            "provided type is 'null_type'"},
        TestCase{"cel.expr.conformance.proto3.TestAllTypes{}.?single_int64 "
                 "== null",
                 _, "no matching overload for '_==_'"}));

}  // namespace
}  // namespace cel
