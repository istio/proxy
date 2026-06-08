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

#include "checker/type_checker_builder_factory.h"

#include <memory>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "checker/checker_options.h"
#include "checker/internal/test_ast_helpers.h"
#include "checker/standard_library.h"
#include "checker/type_checker.h"
#include "checker/type_checker_builder.h"
#include "checker/validation_result.h"
#include "common/decl.h"
#include "common/type.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::cel::checker_internal::MakeTestParsedAst;
using ::cel::internal::GetSharedTestingDescriptorPool;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Truly;

TEST(TypeCheckerBuilderTest, AddVariable) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool()));

  ASSERT_THAT(builder->AddVariable(MakeVariableDecl("x", IntType())), IsOk());

  ASSERT_OK_AND_ASSIGN(auto checker, builder->Build());
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("x"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, checker->Check(std::move(ast)));
  EXPECT_TRUE(result.IsValid());
}

TEST(TypeCheckerBuilderTest, AddComplexType) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool()));

  MapType map_type(builder->arena(), StringType(), IntType());

  ASSERT_THAT(builder->AddVariable(MakeVariableDecl("m", map_type)), IsOk());

  ASSERT_OK_AND_ASSIGN(auto checker, builder->Build());
  builder.reset();
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("m.foo"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, checker->Check(std::move(ast)));
  EXPECT_TRUE(result.IsValid());
}

TEST(TypeCheckerBuilderTest, TypeCheckersIndependent) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool()));

  MapType map_type(builder->arena(), StringType(), IntType());

  ASSERT_THAT(builder->AddVariable(MakeVariableDecl("m", map_type)), IsOk());
  ASSERT_OK_AND_ASSIGN(
      FunctionDecl fn,
      MakeFunctionDecl(
          "foo", MakeOverloadDecl("foo", IntType(), IntType(), IntType())));
  ASSERT_THAT(builder->AddFunction(std::move(fn)), IsOk());

  ASSERT_OK_AND_ASSIGN(auto checker1, builder->Build());

  ASSERT_THAT(builder->AddVariable(MakeVariableDecl("ns.m2", map_type)),
              IsOk());
  builder->set_container("ns");
  ASSERT_OK_AND_ASSIGN(auto checker2, builder->Build());
  // Test for lifetime issues between separate type checker instances from the
  // same builder.
  builder.reset();

  {
    ASSERT_OK_AND_ASSIGN(auto ast1, MakeTestParsedAst("foo(m.bar, m.bar)"));
    ASSERT_OK_AND_ASSIGN(auto ast2, MakeTestParsedAst("foo(m.bar, m2.bar)"));

    ASSERT_OK_AND_ASSIGN(ValidationResult result,
                         checker1->Check(std::move(ast1)));
    EXPECT_TRUE(result.IsValid());
    ASSERT_OK_AND_ASSIGN(ValidationResult result2,
                         checker1->Check(std::move(ast2)));
    EXPECT_FALSE(result2.IsValid());
  }
  checker1.reset();

  {
    ASSERT_OK_AND_ASSIGN(auto ast1, MakeTestParsedAst("foo(m.bar, m.bar)"));
    ASSERT_OK_AND_ASSIGN(auto ast2, MakeTestParsedAst("foo(m.bar, m2.bar)"));

    ASSERT_OK_AND_ASSIGN(ValidationResult result,
                         checker2->Check(std::move(ast1)));
    EXPECT_TRUE(result.IsValid());
    ASSERT_OK_AND_ASSIGN(ValidationResult result2,
                         checker2->Check(std::move(ast2)));
    EXPECT_TRUE(result2.IsValid());
  }
}

TEST(TypeCheckerBuilderTest, AddVariableRedeclaredError) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool()));

  ASSERT_THAT(builder->AddVariable(MakeVariableDecl("x", IntType())), IsOk());
  // We resolve the variable declarations at the Build() call, so the error
  // surfaces then.
  ASSERT_THAT(builder->AddVariable(MakeVariableDecl("x", IntType())), IsOk());

  EXPECT_THAT(builder->Build(),
              StatusIs(absl::StatusCode::kAlreadyExists,
                       "variable 'x' declared multiple times"));
}

TEST(TypeCheckerBuilderTest, AddFunction) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool()));

  ASSERT_OK_AND_ASSIGN(
      auto fn_decl,
      MakeFunctionDecl(
          "add", MakeOverloadDecl("add_int", IntType(), IntType(), IntType())));

  ASSERT_THAT(builder->AddFunction(fn_decl), IsOk());
  ASSERT_OK_AND_ASSIGN(auto checker, builder->Build());
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("add(1, 2)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, checker->Check(std::move(ast)));
  EXPECT_TRUE(result.IsValid());
}

TEST(TypeCheckerBuilderTest, AddFunctionRedeclaredError) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool()));

  ASSERT_OK_AND_ASSIGN(
      auto fn_decl,
      MakeFunctionDecl(
          "add", MakeOverloadDecl("add_int", IntType(), IntType(), IntType())));

  ASSERT_THAT(builder->AddFunction(fn_decl), IsOk());
  ASSERT_THAT(builder->AddFunction(fn_decl), IsOk());

  EXPECT_THAT(builder->Build(),
              StatusIs(absl::StatusCode::kAlreadyExists,
                       "function 'add' declared multiple times"));
}

TEST(TypeCheckerBuilderTest, AddLibrary) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool()));

  ASSERT_OK_AND_ASSIGN(
      auto fn_decl,
      MakeFunctionDecl(
          "add", MakeOverloadDecl("add_int", IntType(), IntType(), IntType())));

  ASSERT_THAT(builder->AddLibrary({"",
                                   [&](TypeCheckerBuilder& b) {
                                     return builder->AddFunction(fn_decl);
                                   }}),

              IsOk());
  ASSERT_OK_AND_ASSIGN(auto checker, builder->Build());
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("add(1, 2)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, checker->Check(std::move(ast)));
  EXPECT_TRUE(result.IsValid());
}

// Example test lib that adds:
//  - add(int, int) -> int
//  - add(double, double) -> double
//  - sub(int, int) -> int
//  - sub(double, double) -> double
absl::Status SubsetTestlibConfigurer(TypeCheckerBuilder& builder) {
  absl::Status s;
  CEL_ASSIGN_OR_RETURN(
      FunctionDecl fn_decl,
      MakeFunctionDecl(
          "add", MakeOverloadDecl("add_int", IntType(), IntType(), IntType()),
          MakeOverloadDecl("add_double", DoubleType(), DoubleType(),
                           DoubleType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(fn_decl)));

  CEL_ASSIGN_OR_RETURN(
      fn_decl,
      MakeFunctionDecl(
          "sub", MakeOverloadDecl("sub_int", IntType(), IntType(), IntType()),
          MakeOverloadDecl("sub_double", DoubleType(), DoubleType(),
                           DoubleType())));

  CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(fn_decl)));

  return absl::OkStatus();
}

CheckerLibrary SubsetTestlib() { return {"testlib", SubsetTestlibConfigurer}; }

TEST(TypeCheckerBuilderTest, AddLibraryIncludeSubset) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool()));

  ASSERT_THAT(builder->AddLibrary(SubsetTestlib()), IsOk());
  ASSERT_THAT(
      builder->AddLibrarySubset(
          {"testlib",
           [](absl::string_view /*function*/, absl::string_view overload_id) {
             return (overload_id == "add_int" || overload_id == "sub_int");
           }}),
      IsOk());
  ASSERT_OK_AND_ASSIGN(auto checker, builder->Build());

  std::vector<ValidationResult> results;
  for (const auto& expr :
       {"sub(1, 2)", "add(1, 2)", "sub(1.0, 2.0)", "add(1.0, 2.0)"}) {
    ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst(expr));
    ASSERT_OK_AND_ASSIGN(ValidationResult result,
                         checker->Check(std::move(ast)));
    results.push_back(std::move(result));
  }
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("add(1, 2)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, checker->Check(std::move(ast)));
  EXPECT_THAT(results, ElementsAre(Truly([](const ValidationResult& result) {
                                     return result.IsValid();
                                   }),
                                   Truly([](const ValidationResult& result) {
                                     return result.IsValid();
                                   }),
                                   Truly([](const ValidationResult& result) {
                                     return !result.IsValid();
                                   }),
                                   Truly([](const ValidationResult& result) {
                                     return !result.IsValid();
                                   })));
}

TEST(TypeCheckerBuilderTest, AddLibraryExcludeSubset) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool()));

  ASSERT_THAT(builder->AddLibrary(SubsetTestlib()), IsOk());
  ASSERT_THAT(
      builder->AddLibrarySubset(
          {"testlib",
           [](absl::string_view /*function*/, absl::string_view overload_id) {
             return (overload_id != "add_int" && overload_id != "sub_int");
             ;
           }}),
      IsOk());
  ASSERT_OK_AND_ASSIGN(auto checker, builder->Build());

  std::vector<ValidationResult> results;
  for (const auto& expr :
       {"sub(1, 2)", "add(1, 2)", "sub(1.0, 2.0)", "add(1.0, 2.0)"}) {
    ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst(expr));
    ASSERT_OK_AND_ASSIGN(ValidationResult result,
                         checker->Check(std::move(ast)));
    results.push_back(std::move(result));
  }
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("add(1, 2)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, checker->Check(std::move(ast)));
  EXPECT_THAT(results, ElementsAre(Truly([](const ValidationResult& result) {
                                     return !result.IsValid();
                                   }),
                                   Truly([](const ValidationResult& result) {
                                     return !result.IsValid();
                                   }),
                                   Truly([](const ValidationResult& result) {
                                     return result.IsValid();
                                   }),
                                   Truly([](const ValidationResult& result) {
                                     return result.IsValid();
                                   })));
}

TEST(TypeCheckerBuilderTest, AddLibrarySubsetRemoveAllOvl) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool()));

  ASSERT_THAT(builder->AddLibrary(SubsetTestlib()), IsOk());
  ASSERT_THAT(builder->AddLibrarySubset({"testlib",
                                         [](absl::string_view function,
                                            absl::string_view /*overload_id*/) {
                                           return function != "add";
                                         }}),
              IsOk());
  ASSERT_OK_AND_ASSIGN(auto checker, builder->Build());

  std::vector<ValidationResult> results;
  for (const auto& expr :
       {"sub(1, 2)", "add(1, 2)", "sub(1.0, 2.0)", "add(1.0, 2.0)"}) {
    ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst(expr));
    ASSERT_OK_AND_ASSIGN(ValidationResult result,
                         checker->Check(std::move(ast)));
    results.push_back(std::move(result));
  }
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("add(1, 2)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, checker->Check(std::move(ast)));
  EXPECT_THAT(results, ElementsAre(Truly([](const ValidationResult& result) {
                                     return result.IsValid();
                                   }),
                                   Truly([](const ValidationResult& result) {
                                     return !result.IsValid();
                                   }),
                                   Truly([](const ValidationResult& result) {
                                     return result.IsValid();
                                   }),
                                   Truly([](const ValidationResult& result) {
                                     return !result.IsValid();
                                   })));
}

TEST(TypeCheckerBuilderTest, AddLibraryOneSubsetPerLibraryId) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool()));

  ASSERT_THAT(builder->AddLibrary(SubsetTestlib()), IsOk());
  ASSERT_THAT(
      builder->AddLibrarySubset(
          {"testlib", [](absl::string_view function,
                         absl::string_view /*overload_id*/) { return true; }}),
      IsOk());
  EXPECT_THAT(
      builder->AddLibrarySubset(
          {"testlib", [](absl::string_view function,
                         absl::string_view /*overload_id*/) { return true; }}),
      StatusIs(absl::StatusCode::kAlreadyExists));
}

TEST(TypeCheckerBuilderTest, AddLibrarySubsetLibraryIdRequireds) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool()));

  ASSERT_THAT(builder->AddLibrary(SubsetTestlib()), IsOk());
  EXPECT_THAT(builder->AddLibrarySubset({"",
                                         [](absl::string_view function,
                                            absl::string_view /*overload_id*/) {
                                           return function == "add";
                                         }}),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(TypeCheckerBuilderTest, AddContextDeclaration) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool()));

  ASSERT_OK_AND_ASSIGN(
      auto fn_decl,
      MakeFunctionDecl("increment", MakeOverloadDecl("increment_int", IntType(),
                                                     IntType())));

  ASSERT_THAT(builder->AddContextDeclaration(
                  "cel.expr.conformance.proto3.TestAllTypes"),
              IsOk());
  ASSERT_THAT(builder->AddFunction(fn_decl), IsOk());

  ASSERT_OK_AND_ASSIGN(auto checker, builder->Build());
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("increment(single_int64)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, checker->Check(std::move(ast)));
  EXPECT_TRUE(result.IsValid());
}

TEST(TypeCheckerBuilderTest, WellKnownTypeContextDeclarationError) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool()));

  ASSERT_THAT(builder->AddContextDeclaration("google.protobuf.Any"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("'google.protobuf.Any' is not a struct")));
}

TEST(TypeCheckerBuilderTest, AllowWellKnownTypeContextDeclaration) {
  CheckerOptions options;
  options.allow_well_known_type_context_declarations = true;
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool(), options));

  ASSERT_THAT(builder->AddContextDeclaration("google.protobuf.Any"), IsOk());
  ASSERT_THAT(builder->AddLibrary(StandardCheckerLibrary()), IsOk());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<TypeChecker> type_checker,
                       builder->Build());
  ASSERT_OK_AND_ASSIGN(
      auto ast,
      MakeTestParsedAst(
          R"cel(value == b'' && type_url == 'type.googleapis.com/google.protobuf.Duration')cel"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result,
                       type_checker->Check(std::move(ast)));

  ASSERT_TRUE(result.IsValid());
}

TEST(TypeCheckerBuilderTest, AllowWellKnownTypeContextDeclarationStruct) {
  CheckerOptions options;
  options.allow_well_known_type_context_declarations = true;
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool(), options));

  ASSERT_THAT(builder->AddContextDeclaration("google.protobuf.Struct"), IsOk());
  ASSERT_THAT(builder->AddLibrary(StandardCheckerLibrary()), IsOk());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<TypeChecker> type_checker,
                       builder->Build());
  ASSERT_OK_AND_ASSIGN(
      auto ast,
      MakeTestParsedAst(R"cel(fields.foo.bar_list.exists(x, x == 1))cel"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result,
                       type_checker->Check(std::move(ast)));

  ASSERT_TRUE(result.IsValid());
}

TEST(TypeCheckerBuilderTest, AllowWellKnownTypeContextDeclarationValue) {
  CheckerOptions options;
  options.allow_well_known_type_context_declarations = true;
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool(), options));

  ASSERT_THAT(builder->AddContextDeclaration("google.protobuf.Value"), IsOk());
  ASSERT_THAT(builder->AddLibrary(StandardCheckerLibrary()), IsOk());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<TypeChecker> type_checker,
                       builder->Build());
  ASSERT_OK_AND_ASSIGN(
      auto ast, MakeTestParsedAst(
                    // Note: one of fields are all added with safe traversal, so
                    // we lose the union discriminator information.
                    R"cel(
            null_value == null &&
            number_value == 0.0 &&
            string_value == '' &&
            list_value == [] &&
            struct_value == {} &&
            bool_value == false)cel"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result,
                       type_checker->Check(std::move(ast)));

  ASSERT_TRUE(result.IsValid());
}

TEST(TypeCheckerBuilderTest, AllowWellKnownTypeContextDeclarationInt64Value) {
  CheckerOptions options;
  options.allow_well_known_type_context_declarations = true;
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool(), options));

  ASSERT_THAT(builder->AddContextDeclaration("google.protobuf.Int64Value"),
              IsOk());
  ASSERT_THAT(builder->AddLibrary(StandardCheckerLibrary()), IsOk());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<TypeChecker> type_checker,
                       builder->Build());
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst(R"cel(value == 0)cel"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result,
                       type_checker->Check(std::move(ast)));

  ASSERT_TRUE(result.IsValid());
}

TEST(TypeCheckerBuilderTest, AddLibraryRedeclaredError) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool()));

  ASSERT_OK_AND_ASSIGN(
      auto fn_decl,
      MakeFunctionDecl(
          "add", MakeOverloadDecl("add_int", IntType(), IntType(), IntType())));

  ASSERT_THAT(builder->AddLibrary({"testlib",
                                   [&](TypeCheckerBuilder& b) {
                                     return builder->AddFunction(fn_decl);
                                   }}),
              IsOk());
  EXPECT_THAT(builder->AddLibrary({"testlib",
                                   [&](TypeCheckerBuilder& b) {
                                     return builder->AddFunction(fn_decl);
                                   }}),
              StatusIs(absl::StatusCode::kAlreadyExists, HasSubstr("testlib")));
}

TEST(TypeCheckerBuilderTest, BuildForwardsLibraryErrors) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool()));

  ASSERT_OK_AND_ASSIGN(
      auto fn_decl,
      MakeFunctionDecl(
          "add", MakeOverloadDecl("add_int", IntType(), IntType(), IntType())));

  ASSERT_THAT(builder->AddLibrary({"",
                                   [&](TypeCheckerBuilder& b) {
                                     return builder->AddFunction(fn_decl);
                                   }}),
              IsOk());
  ASSERT_THAT(builder->AddLibrary({"",
                                   [](TypeCheckerBuilder& b) {
                                     return absl::InternalError("test error");
                                   }}),
              IsOk());

  EXPECT_THAT(builder->Build(),
              StatusIs(absl::StatusCode::kInternal, "test error"));
}

TEST(TypeCheckerBuilderTest, AddFunctionOverlapsWithStdMacroError) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool()));

  ASSERT_OK_AND_ASSIGN(
      auto fn_decl, MakeFunctionDecl("map", MakeMemberOverloadDecl(
                                                "ovl_3", ListType(), ListType(),
                                                DynType(), DynType())));

  EXPECT_THAT(builder->AddFunction(fn_decl),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "overload for name 'map' with 3 argument(s) overlaps "
                       "with predefined macro"));

  fn_decl.set_name("filter");

  EXPECT_THAT(builder->AddFunction(fn_decl),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "overload for name 'filter' with 3 argument(s) overlaps "
                       "with predefined macro"));

  fn_decl.set_name("exists");

  EXPECT_THAT(builder->AddFunction(fn_decl),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "overload for name 'exists' with 3 argument(s) overlaps "
                       "with predefined macro"));

  fn_decl.set_name("exists_one");

  EXPECT_THAT(builder->AddFunction(fn_decl),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "overload for name 'exists_one' with 3 argument(s) "
                       "overlaps with predefined macro"));

  fn_decl.set_name("all");

  EXPECT_THAT(builder->AddFunction(fn_decl),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "overload for name 'all' with 3 argument(s) overlaps "
                       "with predefined macro"));

  fn_decl.set_name("optMap");

  EXPECT_THAT(builder->AddFunction(fn_decl),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "overload for name 'optMap' with 3 argument(s) overlaps "
                       "with predefined macro"));

  fn_decl.set_name("optFlatMap");

  EXPECT_THAT(
      builder->AddFunction(fn_decl),
      StatusIs(absl::StatusCode::kInvalidArgument,
               "overload for name 'optFlatMap' with 3 argument(s) overlaps "
               "with predefined macro"));

  ASSERT_OK_AND_ASSIGN(
      fn_decl, MakeFunctionDecl(
                   "has", MakeOverloadDecl("ovl_1", BoolType(), DynType())));

  EXPECT_THAT(builder->AddFunction(fn_decl),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "overload for name 'has' with 1 argument(s) overlaps "
                       "with predefined macro"));

  ASSERT_OK_AND_ASSIGN(
      fn_decl, MakeFunctionDecl("map", MakeMemberOverloadDecl(
                                           "ovl_4", ListType(), ListType(),

                                           DynType(), DynType(), DynType())));

  EXPECT_THAT(builder->AddFunction(fn_decl),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "overload for name 'map' with 4 argument(s) overlaps "
                       "with predefined macro"));
}

TEST(TypeCheckerBuilderTest, AddFunctionNoOverlapWithStdMacroError) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool()));

  ASSERT_OK_AND_ASSIGN(
      auto fn_decl,
      MakeFunctionDecl("has", MakeMemberOverloadDecl("ovl", BoolType(),
                                                     DynType(), StringType())));

  EXPECT_THAT(builder->AddFunction(fn_decl), IsOk());
}

}  // namespace
}  // namespace cel
