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

#include "compiler/compiler_factory.h"

#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/match.h"
#include "checker/optional.h"
#include "checker/standard_library.h"
#include "checker/type_check_issue.h"
#include "checker/type_checker.h"
#include "checker/validation_result.h"
#include "common/decl.h"
#include "common/source.h"
#include "common/type.h"
#include "compiler/compiler.h"
#include "compiler/standard_library.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "parser/macro.h"
#include "parser/parser_interface.h"
#include "testutil/baseline_tests.h"
#include "google/protobuf/descriptor.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::cel::test::FormatBaselineAst;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Property;
using ::testing::Truly;

TEST(CompilerFactoryTest, Works) {
  ASSERT_OK_AND_ASSIGN(
      auto builder,
      NewCompilerBuilder(cel::internal::GetSharedTestingDescriptorPool()));

  ASSERT_THAT(builder->AddLibrary(StandardCompilerLibrary()), IsOk());
  ASSERT_OK_AND_ASSIGN(auto compiler, builder->Build());

  ASSERT_OK_AND_ASSIGN(
      ValidationResult result,
      compiler->Compile("['a', 'b', 'c'].exists(x, x in ['c', 'd', 'e']) && 10 "
                        "< (5 % 3 * 2 + 1 - 2)"));

  ASSERT_TRUE(result.IsValid());

  EXPECT_EQ(FormatBaselineAst(*result.GetAst()),
            R"(_&&_(
  __comprehension__(
    // Variable
    x,
    // Target
    [
      "a"~string,
      "b"~string,
      "c"~string
    ]~list(string),
    // Accumulator
    @result,
    // Init
    false~bool,
    // LoopCondition
    @not_strictly_false(
      !_(
        @result~bool^@result
      )~bool^logical_not
    )~bool^not_strictly_false,
    // LoopStep
    _||_(
      @result~bool^@result,
      @in(
        x~string^x,
        [
          "c"~string,
          "d"~string,
          "e"~string
        ]~list(string)
      )~bool^in_list
    )~bool^logical_or,
    // Result
    @result~bool^@result)~bool,
  _<_(
    10~int,
    _-_(
      _+_(
        _*_(
          _%_(
            5~int,
            3~int
          )~int^modulo_int64,
          2~int
        )~int^multiply_int64,
        1~int
      )~int^add_int64,
      2~int
    )~int^subtract_int64
  )~bool^less_int64
)~bool^logical_and)");
}

TEST(CompilerFactoryTest, ParserLibrary) {
  ASSERT_OK_AND_ASSIGN(
      auto builder,
      NewCompilerBuilder(cel::internal::GetSharedTestingDescriptorPool()));

  ASSERT_THAT(
      builder->AddLibrary({"test",
                           [](ParserBuilder& builder) -> absl::Status {
                             builder.GetOptions().disable_standard_macros =
                                 true;
                             return builder.AddMacro(cel::HasMacro());
                           }}),
      IsOk());

  ASSERT_THAT(builder->GetCheckerBuilder().AddVariable(
                  MakeVariableDecl("a", MapType())),
              IsOk());

  ASSERT_OK_AND_ASSIGN(auto compiler, builder->Build());

  ASSERT_THAT(compiler->Compile("has(a.b)"), IsOk());

  ASSERT_OK_AND_ASSIGN(ValidationResult result,
                       compiler->Compile("[].map(x, x)"));

  EXPECT_FALSE(result.IsValid());
  EXPECT_THAT(result.GetIssues(),
              Contains(Property(&TypeCheckIssue::message,
                                HasSubstr("undeclared reference to 'map'"))))
      << result.GetIssues()[2].message();
}

TEST(CompilerFactoryTest, ParserOptions) {
  ASSERT_OK_AND_ASSIGN(
      auto builder,
      NewCompilerBuilder(cel::internal::GetSharedTestingDescriptorPool()));

  builder->GetParserBuilder().GetOptions().enable_optional_syntax = true;
  ASSERT_THAT(builder->AddLibrary(OptionalCheckerLibrary()), IsOk());

  ASSERT_THAT(builder->GetCheckerBuilder().AddVariable(
                  MakeVariableDecl("a", MapType())),
              IsOk());

  ASSERT_OK_AND_ASSIGN(auto compiler, builder->Build());

  ASSERT_THAT(compiler->Compile("a.?b.orValue('foo')"), IsOk());
}

TEST(CompilerFactoryTest, GetParser) {
  ASSERT_OK_AND_ASSIGN(
      auto builder,
      NewCompilerBuilder(cel::internal::GetSharedTestingDescriptorPool()));

  ASSERT_OK_AND_ASSIGN(auto compiler, builder->Build());

  const cel::Parser& parser = compiler->GetParser();

  ASSERT_OK_AND_ASSIGN(auto source, cel::NewSource("Or(a, b)"));
  ASSERT_OK_AND_ASSIGN(auto ast, parser.Parse(*source));
}

TEST(CompilerFactoryTest, GetTypeChecker) {
  ASSERT_OK_AND_ASSIGN(
      auto builder,
      NewCompilerBuilder(cel::internal::GetSharedTestingDescriptorPool()));

  absl::Status s;
  s.Update(builder->GetCheckerBuilder().AddVariable(
      MakeVariableDecl("a", BoolType())));

  s.Update(builder->GetCheckerBuilder().AddVariable(
      MakeVariableDecl("b", BoolType())));

  ASSERT_OK_AND_ASSIGN(
      auto or_decl,
      MakeFunctionDecl("Or", MakeOverloadDecl("Or_bool_bool", BoolType(),
                                              BoolType(), BoolType())));
  s.Update(builder->GetCheckerBuilder().AddFunction(std::move(or_decl)));

  ASSERT_THAT(s, IsOk());
  ASSERT_OK_AND_ASSIGN(auto compiler, builder->Build());

  const cel::Parser& parser = compiler->GetParser();

  ASSERT_OK_AND_ASSIGN(auto source, cel::NewSource("Or(a, b)"));
  ASSERT_OK_AND_ASSIGN(auto ast, parser.Parse(*source));

  const cel::TypeChecker& checker = compiler->GetTypeChecker();
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult result,
                       checker.Check(std::move(ast)));
  EXPECT_TRUE(result.IsValid());
}

TEST(CompilerFactoryTest, DisableStandardMacros) {
  CompilerOptions options;
  options.parser_options.disable_standard_macros = true;

  ASSERT_OK_AND_ASSIGN(
      auto builder,
      NewCompilerBuilder(cel::internal::GetSharedTestingDescriptorPool(),
                         options));
  // Add the type checker library, but not the parser library for CEL standard.
  ASSERT_THAT(builder->AddLibrary(CompilerLibrary::FromCheckerLibrary(
                  StandardCheckerLibrary())),
              IsOk());
  ASSERT_THAT(builder->GetParserBuilder().AddMacro(cel::ExistsMacro()), IsOk());

  // a: map(dyn, dyn)
  ASSERT_THAT(builder->GetCheckerBuilder().AddVariable(
                  MakeVariableDecl("a", MapType())),
              IsOk());

  ASSERT_OK_AND_ASSIGN(auto compiler, builder->Build());

  ASSERT_OK_AND_ASSIGN(ValidationResult result, compiler->Compile("a.b"));

  EXPECT_TRUE(result.IsValid());

  // The has macro is disabled, so looks like a function call.
  ASSERT_OK_AND_ASSIGN(result, compiler->Compile("has(a.b)"));

  EXPECT_FALSE(result.IsValid());
  EXPECT_THAT(result.GetIssues(),
              Contains(Truly([](const TypeCheckIssue& issue) {
                return absl::StrContains(issue.message(),
                                         "undeclared reference to 'has'");
              })));

  ASSERT_OK_AND_ASSIGN(result, compiler->Compile("a.exists(x, x == 'foo')"));
  EXPECT_TRUE(result.IsValid());
}

TEST(CompilerFactoryTest, DisableStandardMacrosWithStdlib) {
  CompilerOptions options;
  options.parser_options.disable_standard_macros = true;

  ASSERT_OK_AND_ASSIGN(
      auto builder,
      NewCompilerBuilder(cel::internal::GetSharedTestingDescriptorPool(),
                         options));

  ASSERT_THAT(builder->AddLibrary(StandardCompilerLibrary()), IsOk());
  ASSERT_THAT(builder->GetParserBuilder().AddMacro(cel::ExistsMacro()), IsOk());

  // a: map(dyn, dyn)
  ASSERT_THAT(builder->GetCheckerBuilder().AddVariable(
                  MakeVariableDecl("a", MapType())),
              IsOk());

  ASSERT_OK_AND_ASSIGN(auto compiler, builder->Build());

  ASSERT_OK_AND_ASSIGN(ValidationResult result, compiler->Compile("a.b"));

  EXPECT_TRUE(result.IsValid());

  // The has macro is disabled, so looks like a function call.
  ASSERT_OK_AND_ASSIGN(result, compiler->Compile("has(a.b)"));

  EXPECT_FALSE(result.IsValid());
  EXPECT_THAT(result.GetIssues(),
              Contains(Truly([](const TypeCheckIssue& issue) {
                return absl::StrContains(issue.message(),
                                         "undeclared reference to 'has'");
              })));

  ASSERT_OK_AND_ASSIGN(result, compiler->Compile("a.exists(x, x == 'foo')"));
  EXPECT_TRUE(result.IsValid());
}

TEST(CompilerFactoryTest, FailsIfLibraryAddedTwice) {
  ASSERT_OK_AND_ASSIGN(
      auto builder,
      NewCompilerBuilder(cel::internal::GetSharedTestingDescriptorPool()));

  ASSERT_THAT(builder->AddLibrary(StandardCompilerLibrary()), IsOk());
  ASSERT_THAT(builder->AddLibrary(StandardCompilerLibrary()),
              StatusIs(absl::StatusCode::kAlreadyExists,
                       HasSubstr("library already exists: stdlib")));
}

TEST(CompilerFactoryTest, FailsIfLibrarySubsetAddedTwice) {
  ASSERT_OK_AND_ASSIGN(
      auto builder,
      NewCompilerBuilder(cel::internal::GetSharedTestingDescriptorPool()));

  ASSERT_THAT(builder->AddLibrary(StandardCompilerLibrary()), IsOk());

  ASSERT_THAT(builder->AddLibrarySubset({
                  .library_id = "stdlib",
                  .should_include_macro = nullptr,
                  .should_include_overload = nullptr,
              }),
              IsOk());

  ASSERT_THAT(builder->AddLibrarySubset({
                  .library_id = "stdlib",
                  .should_include_macro = nullptr,
                  .should_include_overload = nullptr,
              }),
              StatusIs(absl::StatusCode::kAlreadyExists,
                       HasSubstr("library subset already exists for: stdlib")));
}

TEST(CompilerFactoryTest, FailsIfLibrarySubsetHasNoId) {
  ASSERT_OK_AND_ASSIGN(
      auto builder,
      NewCompilerBuilder(cel::internal::GetSharedTestingDescriptorPool()));

  ASSERT_THAT(builder->AddLibrary(StandardCompilerLibrary()), IsOk());
  ASSERT_THAT(builder->AddLibrarySubset({
                  .library_id = "",
                  .should_include_macro = nullptr,
                  .should_include_overload = nullptr,
              }),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("library id must not be empty")));
}

TEST(CompilerFactoryTest, FailsIfNullDescriptorPool) {
  std::shared_ptr<const google::protobuf::DescriptorPool> pool =
      internal::GetSharedTestingDescriptorPool();
  pool.reset();
  ASSERT_THAT(
      NewCompilerBuilder(std::move(pool)),
      absl_testing::StatusIs(absl::StatusCode::kInvalidArgument,
                             HasSubstr("descriptor_pool must not be null")));
}

}  // namespace
}  // namespace cel
