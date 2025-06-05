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

#include "checker/internal/type_checker_impl.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "base/ast_internal/ast_impl.h"
#include "base/ast_internal/expr.h"
#include "checker/checker_options.h"
#include "checker/internal/test_ast_helpers.h"
#include "checker/internal/type_check_env.h"
#include "checker/type_check_issue.h"
#include "checker/validation_result.h"
#include "common/ast.h"
#include "common/decl.h"
#include "common/expr.h"
#include "common/source.h"
#include "common/type.h"
#include "common/type_introspector.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "proto/test/v1/proto2/test_all_types.pb.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/message.h"

namespace cel {
namespace checker_internal {

namespace {

using ::absl_testing::IsOk;
using ::cel::ast_internal::AstImpl;
using ::cel::ast_internal::Reference;
using ::cel::internal::GetSharedTestingDescriptorPool;
using ::google::api::expr::test::v1::proto3::TestAllTypes;
using ::testing::_;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Property;

using AstType = ast_internal::Type;
using Severity = TypeCheckIssue::Severity;

namespace testpb3 = ::google::api::expr::test::v1::proto3;

std::string SevString(Severity severity) {
  switch (severity) {
    case Severity::kDeprecated:
      return "Deprecated";
    case Severity::kError:
      return "Error";
    case Severity::kWarning:
      return "Warning";
    case Severity::kInformation:
      return "Information";
  }
}

}  // namespace
}  // namespace checker_internal

template <typename Sink>
void AbslStringify(Sink& sink, const TypeCheckIssue& issue) {
  absl::Format(&sink, "TypeCheckIssue(%s): %s",
               checker_internal::SevString(issue.severity()), issue.message());
}

namespace checker_internal {
namespace {

absl::Nonnull<google::protobuf::Arena*> TestTypeArena() {
  static absl::NoDestructor<google::protobuf::Arena> kArena;
  return &(*kArena);
}

FunctionDecl MakeIdentFunction() {
  auto decl = MakeFunctionDecl(
      "identity",
      MakeOverloadDecl("identity", TypeParamType("A"), TypeParamType("A")));
  ABSL_CHECK_OK(decl.status());
  return decl.value();
}

MATCHER_P2(IsIssueWithSubstring, severity, substring, "") {
  const TypeCheckIssue& issue = arg;
  if (issue.severity() == severity &&
      absl::StrContains(issue.message(), substring)) {
    return true;
  }

  *result_listener << "expected: " << SevString(severity) << " " << substring
                   << "\nactual: " << SevString(issue.severity()) << " "
                   << issue.message();

  return false;
}

MATCHER_P(IsVariableReference, var_name, "") {
  const Reference& reference = arg;
  if (reference.name() == var_name) {
    return true;
  }
  *result_listener << "expected: " << var_name
                   << "\nactual: " << reference.name();

  return false;
}

MATCHER_P2(IsFunctionReference, fn_name, overloads, "") {
  const Reference& reference = arg;
  if (reference.name() != fn_name) {
    *result_listener << "expected: " << fn_name
                     << "\nactual: " << reference.name();
  }

  absl::flat_hash_set<std::string> got_overload_set(
      reference.overload_id().begin(), reference.overload_id().end());
  absl::flat_hash_set<std::string> want_overload_set(overloads.begin(),
                                                     overloads.end());

  if (got_overload_set != want_overload_set) {
    *result_listener << "expected overload_ids: "
                     << absl::StrJoin(want_overload_set, ",")
                     << "\nactual: " << absl::StrJoin(got_overload_set, ",");
  }

  return reference.name() == fn_name && got_overload_set == want_overload_set;
}

absl::Status RegisterMinimalBuiltins(absl::Nonnull<google::protobuf::Arena*> arena,
                                     TypeCheckEnv& env) {
  Type list_of_a = ListType(arena, TypeParamType("A"));

  FunctionDecl add_op;

  add_op.set_name("_+_");
  CEL_RETURN_IF_ERROR(add_op.AddOverload(
      MakeOverloadDecl("add_int_int", IntType(), IntType(), IntType())));
  CEL_RETURN_IF_ERROR(add_op.AddOverload(
      MakeOverloadDecl("add_uint_uint", UintType(), UintType(), UintType())));
  CEL_RETURN_IF_ERROR(add_op.AddOverload(MakeOverloadDecl(
      "add_double_double", DoubleType(), DoubleType(), DoubleType())));

  CEL_RETURN_IF_ERROR(add_op.AddOverload(
      MakeOverloadDecl("add_list", list_of_a, list_of_a, list_of_a)));

  FunctionDecl not_op;
  not_op.set_name("!_");
  CEL_RETURN_IF_ERROR(not_op.AddOverload(
      MakeOverloadDecl("logical_not",
                       /*return_type=*/BoolType{}, BoolType{})));
  FunctionDecl not_strictly_false;
  not_strictly_false.set_name("@not_strictly_false");
  CEL_RETURN_IF_ERROR(not_strictly_false.AddOverload(
      MakeOverloadDecl("not_strictly_false",
                       /*return_type=*/BoolType{}, DynType{})));
  FunctionDecl mult_op;
  mult_op.set_name("_*_");
  CEL_RETURN_IF_ERROR(mult_op.AddOverload(
      MakeOverloadDecl("mult_int_int",
                       /*return_type=*/IntType(), IntType(), IntType())));
  FunctionDecl or_op;
  or_op.set_name("_||_");
  CEL_RETURN_IF_ERROR(or_op.AddOverload(
      MakeOverloadDecl("logical_or",
                       /*return_type=*/BoolType{}, BoolType{}, BoolType{})));

  FunctionDecl and_op;
  and_op.set_name("_&&_");
  CEL_RETURN_IF_ERROR(and_op.AddOverload(
      MakeOverloadDecl("logical_and",
                       /*return_type=*/BoolType{}, BoolType{}, BoolType{})));

  FunctionDecl lt_op;
  lt_op.set_name("_<_");
  CEL_RETURN_IF_ERROR(lt_op.AddOverload(
      MakeOverloadDecl("lt_int_int",
                       /*return_type=*/BoolType{}, IntType(), IntType())));

  FunctionDecl gt_op;
  gt_op.set_name("_>_");
  CEL_RETURN_IF_ERROR(gt_op.AddOverload(
      MakeOverloadDecl("gt_int_int",
                       /*return_type=*/BoolType{}, IntType(), IntType())));

  FunctionDecl eq_op;
  eq_op.set_name("_==_");
  CEL_RETURN_IF_ERROR(eq_op.AddOverload(MakeOverloadDecl(
      "equals",
      /*return_type=*/BoolType{}, TypeParamType("A"), TypeParamType("A"))));

  FunctionDecl ternary_op;
  ternary_op.set_name("_?_:_");
  CEL_RETURN_IF_ERROR(eq_op.AddOverload(MakeOverloadDecl(
      "conditional",
      /*return_type=*/
      TypeParamType("A"), BoolType{}, TypeParamType("A"), TypeParamType("A"))));

  FunctionDecl to_int;
  to_int.set_name("int");
  CEL_RETURN_IF_ERROR(to_int.AddOverload(
      MakeOverloadDecl("to_int",
                       /*return_type=*/IntType(), DynType())));

  FunctionDecl to_duration;
  to_duration.set_name("duration");
  CEL_RETURN_IF_ERROR(to_duration.AddOverload(
      MakeOverloadDecl("to_duration",
                       /*return_type=*/DurationType(), StringType())));

  FunctionDecl to_timestamp;
  to_timestamp.set_name("timestamp");
  CEL_RETURN_IF_ERROR(to_timestamp.AddOverload(
      MakeOverloadDecl("to_timestamp",
                       /*return_type=*/TimestampType(), IntType())));

  FunctionDecl to_dyn;
  to_dyn.set_name("dyn");
  CEL_RETURN_IF_ERROR(to_dyn.AddOverload(
      MakeOverloadDecl("to_dyn",
                       /*return_type=*/DynType(), TypeParamType("A"))));

  FunctionDecl to_type;
  to_type.set_name("type");
  CEL_RETURN_IF_ERROR(to_type.AddOverload(
      MakeOverloadDecl("to_type",
                       /*return_type=*/TypeType(arena, TypeParamType("A")),
                       TypeParamType("A"))));

  env.InsertFunctionIfAbsent(std::move(not_op));
  env.InsertFunctionIfAbsent(std::move(not_strictly_false));
  env.InsertFunctionIfAbsent(std::move(add_op));
  env.InsertFunctionIfAbsent(std::move(mult_op));
  env.InsertFunctionIfAbsent(std::move(or_op));
  env.InsertFunctionIfAbsent(std::move(and_op));
  env.InsertFunctionIfAbsent(std::move(lt_op));
  env.InsertFunctionIfAbsent(std::move(gt_op));
  env.InsertFunctionIfAbsent(std::move(to_int));
  env.InsertFunctionIfAbsent(std::move(eq_op));
  env.InsertFunctionIfAbsent(std::move(ternary_op));
  env.InsertFunctionIfAbsent(std::move(to_dyn));
  env.InsertFunctionIfAbsent(std::move(to_type));
  env.InsertFunctionIfAbsent(std::move(to_duration));
  env.InsertFunctionIfAbsent(std::move(to_timestamp));

  return absl::OkStatus();
}

TEST(TypeCheckerImplTest, SmokeTest) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());

  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("1 + 2"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
}

TEST(TypeCheckerImplTest, SimpleIdentsResolved) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());

  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  env.InsertVariableIfAbsent(MakeVariableDecl("x", IntType()));
  env.InsertVariableIfAbsent(MakeVariableDecl("y", IntType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("x + y"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
}

TEST(TypeCheckerImplTest, ReportMissingIdentDecl) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());

  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  env.InsertVariableIfAbsent(MakeVariableDecl("x", IntType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("x + y"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_FALSE(result.IsValid());

  EXPECT_THAT(result.GetIssues(),
              ElementsAre(IsIssueWithSubstring(Severity::kError,
                                               "undeclared reference to 'y'")));
}

MATCHER_P3(IsIssueWithLocation, line, column, message, "") {
  const TypeCheckIssue& issue = arg;
  if (issue.location().line == line && issue.location().column == column &&
      absl::StrContains(issue.message(), message)) {
    return true;
  }
  return false;
}

TEST(TypeCheckerImplTest, LocationCalculation) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());

  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  env.InsertVariableIfAbsent(MakeVariableDecl("x", IntType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto source, NewSource("a ||\n"
                                              "b ||\n"
                                              " c ||\n"
                                              " d"));
  ASSERT_OK_AND_ASSIGN(auto ast,
                       MakeTestParsedAst(source->content().ToString()));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_FALSE(result.IsValid());

  EXPECT_THAT(
      result.GetIssues(),
      ElementsAre(IsIssueWithLocation(1, 0, "undeclared reference to 'a'"),
                  IsIssueWithLocation(2, 0, "undeclared reference to 'b'"),
                  IsIssueWithLocation(3, 1, "undeclared reference to 'c'"),
                  IsIssueWithLocation(4, 1, "undeclared reference to 'd'")))
      << absl::StrJoin(result.GetIssues(), "\n",
                       [&](std::string* out, const TypeCheckIssue& issue) {
                         absl::StrAppend(out, issue.ToDisplayString(*source));
                       });
}

TEST(TypeCheckerImplTest, QualifiedIdentsResolved) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());

  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  env.InsertVariableIfAbsent(MakeVariableDecl("x.y", IntType()));
  env.InsertVariableIfAbsent(MakeVariableDecl("x.z", IntType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("x.y + x.z"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
}

TEST(TypeCheckerImplTest, ReportMissingQualifiedIdentDecl) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());

  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  env.InsertVariableIfAbsent(MakeVariableDecl("x", IntType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("y.x"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_FALSE(result.IsValid());

  EXPECT_THAT(result.GetIssues(),
              ElementsAre(IsIssueWithSubstring(
                  Severity::kError, "undeclared reference to 'y.x'")));
}

TEST(TypeCheckerImplTest, ResolveMostQualfiedIdent) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());

  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  env.InsertVariableIfAbsent(MakeVariableDecl("x", IntType()));
  env.InsertVariableIfAbsent(MakeVariableDecl("x.y", MapType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("x.y.z"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());
  auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_THAT(ast_impl.reference_map(),
              Contains(Pair(_, IsVariableReference("x.y"))));
}

TEST(TypeCheckerImplTest, MemberFunctionCallResolved) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());

  env.InsertVariableIfAbsent(MakeVariableDecl("x", IntType()));

  env.InsertVariableIfAbsent(MakeVariableDecl("y", IntType()));
  FunctionDecl foo;
  foo.set_name("foo");
  ASSERT_THAT(foo.AddOverload(MakeMemberOverloadDecl("int_foo_int",
                                                     /*return_type=*/IntType(),
                                                     IntType(), IntType())),
              IsOk());
  env.InsertFunctionIfAbsent(std::move(foo));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("x.foo(y)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
}

TEST(TypeCheckerImplTest, MemberFunctionCallNotDeclared) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());

  env.InsertVariableIfAbsent(MakeVariableDecl("x", IntType()));
  env.InsertVariableIfAbsent(MakeVariableDecl("y", IntType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("x.foo(y)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_FALSE(result.IsValid());

  EXPECT_THAT(result.GetIssues(),
              ElementsAre(IsIssueWithSubstring(
                  Severity::kError, "undeclared reference to 'foo'")));
}

TEST(TypeCheckerImplTest, FunctionShapeMismatch) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  // foo(int, int) -> int
  ASSERT_OK_AND_ASSIGN(
      auto foo,
      MakeFunctionDecl("foo", MakeOverloadDecl("foo_int_int", IntType(),
                                               IntType(), IntType())));
  env.InsertFunctionIfAbsent(foo);
  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("foo(1, 2, 3)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_FALSE(result.IsValid());

  EXPECT_THAT(result.GetIssues(),
              ElementsAre(IsIssueWithSubstring(
                  Severity::kError, "undeclared reference to 'foo'")));
}

TEST(TypeCheckerImplTest, NamespaceFunctionCallResolved) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  // Variables
  env.InsertVariableIfAbsent(MakeVariableDecl("x", IntType()));
  env.InsertVariableIfAbsent(MakeVariableDecl("y", IntType()));

  // add x.foo as a namespaced function.
  FunctionDecl foo;
  foo.set_name("x.foo");
  ASSERT_THAT(
      foo.AddOverload(MakeOverloadDecl("x_foo_int",
                                       /*return_type=*/IntType(), IntType())),
      IsOk());
  env.InsertFunctionIfAbsent(std::move(foo));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("x.foo(y)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());
  EXPECT_THAT(result.GetIssues(), IsEmpty());

  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());
  auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_TRUE(ast_impl.root_expr().has_call_expr())
      << absl::StrCat("kind: ", ast_impl.root_expr().kind().index());
  EXPECT_EQ(ast_impl.root_expr().call_expr().function(), "x.foo");
  EXPECT_FALSE(ast_impl.root_expr().call_expr().has_target());
}

TEST(TypeCheckerImplTest, NamespacedFunctionSkipsFieldCheck) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  // Variables
  env.InsertVariableIfAbsent(MakeVariableDecl("x", IntType()));

  // add x.foo as a namespaced function.
  FunctionDecl foo;
  foo.set_name("x.y.foo");
  ASSERT_THAT(
      foo.AddOverload(MakeOverloadDecl("x_y_foo_int",
                                       /*return_type=*/IntType(), IntType())),
      IsOk());
  env.InsertFunctionIfAbsent(std::move(foo));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("x.y.foo(x)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());
  EXPECT_THAT(result.GetIssues(), IsEmpty());

  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());
  auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_TRUE(ast_impl.root_expr().has_call_expr())
      << absl::StrCat("kind: ", ast_impl.root_expr().kind().index());
  EXPECT_EQ(ast_impl.root_expr().call_expr().function(), "x.y.foo");
  EXPECT_FALSE(ast_impl.root_expr().call_expr().has_target());
}

TEST(TypeCheckerImplTest, MixedListTypeToDyn) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());

  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("[1, 'a']"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
  auto& ast_impl = AstImpl::CastFromPublicAst(*result.GetAst());
  EXPECT_TRUE(ast_impl.type_map().at(1).list_type().elem_type().has_dyn());
}

TEST(TypeCheckerImplTest, FreeListTypeToDyn) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());

  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("[]"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
  auto& ast_impl = AstImpl::CastFromPublicAst(*result.GetAst());
  EXPECT_TRUE(ast_impl.type_map().at(1).list_type().elem_type().has_dyn());
}

TEST(TypeCheckerImplTest, FreeMapValueTypeToDyn) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());

  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("{}.field"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
  auto& ast_impl = AstImpl::CastFromPublicAst(*result.GetAst());
  auto root_id = ast_impl.root_expr().id();
  EXPECT_TRUE(ast_impl.type_map().at(root_id).has_dyn());
}

TEST(TypeCheckerImplTest, FreeMapTypeToDyn) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());

  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("{}"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
  auto& ast_impl = AstImpl::CastFromPublicAst(*result.GetAst());
  EXPECT_TRUE(ast_impl.type_map().at(1).map_type().key_type().has_dyn());
  EXPECT_TRUE(ast_impl.type_map().at(1).map_type().value_type().has_dyn());
}

TEST(TypeCheckerImplTest, MapTypeWithMixedKeys) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());

  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("{'a': 1, 2: 3}"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
  auto& ast_impl = AstImpl::CastFromPublicAst(*result.GetAst());
  EXPECT_TRUE(ast_impl.type_map().at(1).map_type().key_type().has_dyn());
  EXPECT_EQ(ast_impl.type_map().at(1).map_type().value_type().primitive(),
            ast_internal::PrimitiveType::kInt64);
}

TEST(TypeCheckerImplTest, MapTypeUnsupportedKeyWarns) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());

  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("{{}: 'a'}"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(),
              ElementsAre(IsIssueWithSubstring(Severity::kWarning,
                                               "unsupported map key type:")));
}

TEST(TypeCheckerImplTest, MapTypeWithMixedValues) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());

  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("{'a': 1, 'b': '2'}"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
  auto& ast_impl = AstImpl::CastFromPublicAst(*result.GetAst());
  EXPECT_EQ(ast_impl.type_map().at(1).map_type().key_type().primitive(),
            ast_internal::PrimitiveType::kString);
  EXPECT_TRUE(ast_impl.type_map().at(1).map_type().value_type().has_dyn());
}

TEST(TypeCheckerImplTest, ComprehensionVariablesResolved) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());

  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast,
                       MakeTestParsedAst("[1, 2, 3].exists(x, x * x > 10)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
}

TEST(TypeCheckerImplTest, MapComprehensionVariablesResolved) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());

  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast,
                       MakeTestParsedAst("{1: 3, 2: 4}.exists(x, x == 2)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
}

TEST(TypeCheckerImplTest, NestedComprehensions) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());

  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(
      auto ast,
      MakeTestParsedAst("[1, 2].all(x, ['1', '2'].exists(y, int(y) == x))"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
}

TEST(TypeCheckerImplTest, ComprehensionVarsFollowNamespacePriorityRules) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  env.set_container("com");
  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  // Namespace resolution still applies, compre var doesn't shadow com.x
  env.InsertVariableIfAbsent(MakeVariableDecl("com.x", IntType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast,
                       MakeTestParsedAst("['1', '2'].all(x, x == 2)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());
  auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_THAT(ast_impl.reference_map(),
              Contains(Pair(_, IsVariableReference("com.x"))));
}

TEST(TypeCheckerImplTest, ComprehensionVarsFollowQualifiedIdentPriority) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  // Namespace resolution still applies, compre var doesn't shadow x.y
  env.InsertVariableIfAbsent(MakeVariableDecl("x.y", IntType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast,
                       MakeTestParsedAst("[{'y': '2'}].all(x, x.y == 2)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());
  auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_THAT(ast_impl.reference_map(),
              Contains(Pair(_, IsVariableReference("x.y"))));
}

TEST(TypeCheckerImplTest, ComprehensionVarsCyclicParamAssignability) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  // This is valid because the list construction in the transform will resolve
  // to list(dyn) since candidates E1 -> E2 and list(E1) -> E2 don't agree.
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("[].map(c, [ c, [c] ])"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());

  // Remainder are conceptually the same, but confirm generality.
  ASSERT_OK_AND_ASSIGN(ast, MakeTestParsedAst("[].map(c, [ c, [[c]] ])"));
  ASSERT_OK_AND_ASSIGN(result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  ASSERT_OK_AND_ASSIGN(ast, MakeTestParsedAst("[].map(c, [ [c], [[c]] ])"));
  ASSERT_OK_AND_ASSIGN(result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  ASSERT_OK_AND_ASSIGN(ast, MakeTestParsedAst("[].map(c, [ c, c ])"));
  ASSERT_OK_AND_ASSIGN(result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  ASSERT_OK_AND_ASSIGN(ast, MakeTestParsedAst("[].map(c, [ [c], c ])"));
  ASSERT_OK_AND_ASSIGN(result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  ASSERT_OK_AND_ASSIGN(ast, MakeTestParsedAst("[].map(c, [ [[c]], c ])"));
  ASSERT_OK_AND_ASSIGN(result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  ASSERT_OK_AND_ASSIGN(ast, MakeTestParsedAst("[].map(c, [ c, type(c) ])"));
  ASSERT_OK_AND_ASSIGN(result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());
}

struct PrimitiveLiteralsTestCase {
  std::string expr;
  ast_internal::PrimitiveType expected_type;
};

class PrimitiveLiteralsTest
    : public testing::TestWithParam<PrimitiveLiteralsTestCase> {};

TEST_P(PrimitiveLiteralsTest, LiteralsTypeInferred) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  const PrimitiveLiteralsTestCase& test_case = GetParam();
  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst(test_case.expr));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_TRUE(result.IsValid());
  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());
  auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_EQ(ast_impl.type_map()[1].primitive(), test_case.expected_type);
}

INSTANTIATE_TEST_SUITE_P(
    PrimitiveLiteralsTests, PrimitiveLiteralsTest,
    ::testing::Values(
        PrimitiveLiteralsTestCase{
            .expr = "1",
            .expected_type = ast_internal::PrimitiveType::kInt64,
        },
        PrimitiveLiteralsTestCase{
            .expr = "1.0",
            .expected_type = ast_internal::PrimitiveType::kDouble,
        },
        PrimitiveLiteralsTestCase{
            .expr = "1u",
            .expected_type = ast_internal::PrimitiveType::kUint64,
        },
        PrimitiveLiteralsTestCase{
            .expr = "'string'",
            .expected_type = ast_internal::PrimitiveType::kString,
        },
        PrimitiveLiteralsTestCase{
            .expr = "b'bytes'",
            .expected_type = ast_internal::PrimitiveType::kBytes,
        },
        PrimitiveLiteralsTestCase{
            .expr = "false",
            .expected_type = ast_internal::PrimitiveType::kBool,
        }));
struct AstTypeConversionTestCase {
  Type decl_type;
  ast_internal::Type expected_type;
};

class AstTypeConversionTest
    : public testing::TestWithParam<AstTypeConversionTestCase> {};

TEST_P(AstTypeConversionTest, TypeConversion) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  ASSERT_TRUE(
      env.InsertVariableIfAbsent(MakeVariableDecl("x", GetParam().decl_type)));
  const AstTypeConversionTestCase& test_case = GetParam();
  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("x"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_TRUE(result.IsValid());
  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());
  auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_EQ(ast_impl.type_map()[1], test_case.expected_type)
      << GetParam().decl_type.DebugString();
}

INSTANTIATE_TEST_SUITE_P(
    Primitives, AstTypeConversionTest,
    ::testing::Values(
        AstTypeConversionTestCase{
            .decl_type = NullType(),
            .expected_type = AstType(ast_internal::NullValue()),
        },
        AstTypeConversionTestCase{
            .decl_type = DynType(),
            .expected_type = AstType(ast_internal::DynamicType()),
        },
        AstTypeConversionTestCase{
            .decl_type = BoolType(),
            .expected_type = AstType(ast_internal::PrimitiveType::kBool),
        },
        AstTypeConversionTestCase{
            .decl_type = IntType(),
            .expected_type = AstType(ast_internal::PrimitiveType::kInt64),
        },
        AstTypeConversionTestCase{
            .decl_type = UintType(),
            .expected_type = AstType(ast_internal::PrimitiveType::kUint64),
        },
        AstTypeConversionTestCase{
            .decl_type = DoubleType(),
            .expected_type = AstType(ast_internal::PrimitiveType::kDouble),
        },
        AstTypeConversionTestCase{
            .decl_type = StringType(),
            .expected_type = AstType(ast_internal::PrimitiveType::kString),
        },
        AstTypeConversionTestCase{
            .decl_type = BytesType(),
            .expected_type = AstType(ast_internal::PrimitiveType::kBytes),
        },
        AstTypeConversionTestCase{
            .decl_type = TimestampType(),
            .expected_type = AstType(ast_internal::WellKnownType::kTimestamp),
        },
        AstTypeConversionTestCase{
            .decl_type = DurationType(),
            .expected_type = AstType(ast_internal::WellKnownType::kDuration),
        }));

INSTANTIATE_TEST_SUITE_P(
    Wrappers, AstTypeConversionTest,
    ::testing::Values(
        AstTypeConversionTestCase{
            .decl_type = IntWrapperType(),
            .expected_type = AstType(ast_internal::PrimitiveTypeWrapper(
                ast_internal::PrimitiveType::kInt64)),
        },
        AstTypeConversionTestCase{
            .decl_type = UintWrapperType(),
            .expected_type = AstType(ast_internal::PrimitiveTypeWrapper(
                ast_internal::PrimitiveType::kUint64)),
        },
        AstTypeConversionTestCase{
            .decl_type = DoubleWrapperType(),
            .expected_type = AstType(ast_internal::PrimitiveTypeWrapper(
                ast_internal::PrimitiveType::kDouble)),
        },
        AstTypeConversionTestCase{
            .decl_type = BoolWrapperType(),
            .expected_type = AstType(ast_internal::PrimitiveTypeWrapper(
                ast_internal::PrimitiveType::kBool)),
        },
        AstTypeConversionTestCase{
            .decl_type = StringWrapperType(),
            .expected_type = AstType(ast_internal::PrimitiveTypeWrapper(
                ast_internal::PrimitiveType::kString)),
        },
        AstTypeConversionTestCase{
            .decl_type = BytesWrapperType(),
            .expected_type = AstType(ast_internal::PrimitiveTypeWrapper(
                ast_internal::PrimitiveType::kBytes)),
        }));

INSTANTIATE_TEST_SUITE_P(
    ComplexTypes, AstTypeConversionTest,
    ::testing::Values(
        AstTypeConversionTestCase{
            .decl_type = ListType(TestTypeArena(), IntType()),
            .expected_type =
                AstType(ast_internal::ListType(std::make_unique<AstType>(
                    ast_internal::PrimitiveType::kInt64))),
        },
        AstTypeConversionTestCase{
            .decl_type = MapType(TestTypeArena(), IntType(), IntType()),
            .expected_type = AstType(ast_internal::MapType(
                std::make_unique<AstType>(ast_internal::PrimitiveType::kInt64),
                std::make_unique<AstType>(
                    ast_internal::PrimitiveType::kInt64))),
        },
        AstTypeConversionTestCase{
            .decl_type = TypeType(TestTypeArena(), IntType()),
            .expected_type = AstType(
                std::make_unique<AstType>(ast_internal::PrimitiveType::kInt64)),
        },
        AstTypeConversionTestCase{
            .decl_type = OpaqueType(TestTypeArena(), "tuple",
                                    {IntType(), IntType()}),
            .expected_type = AstType(ast_internal::AbstractType(
                "tuple", {AstType(ast_internal::PrimitiveType::kInt64),
                          AstType(ast_internal::PrimitiveType::kInt64)})),
        },
        AstTypeConversionTestCase{
            .decl_type = StructType(MessageType(TestAllTypes::descriptor())),
            .expected_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes"))}));

TEST(TypeCheckerImplTest, NullLiteral) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("null"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_TRUE(result.IsValid());
  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());
  auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_TRUE(ast_impl.type_map()[1].has_null());
}

TEST(TypeCheckerImplTest, ComprehensionUnsupportedRange) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  env.InsertVariableIfAbsent(MakeVariableDecl("y", IntType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("'abc'.all(x, y == 2)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_FALSE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), Contains(IsIssueWithSubstring(
                                      Severity::kError,
                                      "expression of type 'string' cannot be "
                                      "the range of a comprehension")));
}

TEST(TypeCheckerImplTest, ComprehensionDynRange) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  env.InsertVariableIfAbsent(MakeVariableDecl("range", DynType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("range.all(x, x == 2)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
}

TEST(TypeCheckerImplTest, BasicOvlResolution) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  env.InsertVariableIfAbsent(MakeVariableDecl("x", DoubleType()));
  env.InsertVariableIfAbsent(MakeVariableDecl("y", DoubleType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("x + y"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());

  // Assumes parser numbering: + should always be id 2.
  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());
  auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_THAT(ast_impl.reference_map()[2],
              IsFunctionReference(
                  "_+_", std::vector<std::string>{"add_double_double"}));
}

TEST(TypeCheckerImplTest, OvlResolutionMultipleOverloads) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  env.InsertVariableIfAbsent(MakeVariableDecl("x", DoubleType()));
  env.InsertVariableIfAbsent(MakeVariableDecl("y", DoubleType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("dyn(x) + dyn(y)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());

  // Assumes parser numbering: + should always be id 3.
  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());
  auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_THAT(ast_impl.reference_map()[3],
              IsFunctionReference("_+_", std::vector<std::string>{
                                             "add_double_double", "add_int_int",
                                             "add_list", "add_uint_uint"}));
}

TEST(TypeCheckerImplTest, BasicFunctionResultTypeResolution) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  env.InsertVariableIfAbsent(MakeVariableDecl("x", DoubleType()));
  env.InsertVariableIfAbsent(MakeVariableDecl("y", DoubleType()));
  env.InsertVariableIfAbsent(MakeVariableDecl("z", DoubleType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("x + y + z"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());

  // Assumes parser numbering: + should always be id 2 and 4.
  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());
  auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_THAT(ast_impl.reference_map()[2],
              IsFunctionReference(
                  "_+_", std::vector<std::string>{"add_double_double"}));
  EXPECT_THAT(ast_impl.reference_map()[4],
              IsFunctionReference(
                  "_+_", std::vector<std::string>{"add_double_double"}));
  int64_t root_id = ast_impl.root_expr().id();
  EXPECT_EQ(ast_impl.type_map()[root_id].primitive(),
            ast_internal::PrimitiveType::kDouble);
}

TEST(TypeCheckerImplTest, BasicOvlResolutionNoMatch) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  env.InsertVariableIfAbsent(MakeVariableDecl("x", IntType()));
  env.InsertVariableIfAbsent(MakeVariableDecl("y", StringType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("x + y"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_FALSE(result.IsValid());

  EXPECT_THAT(result.GetIssues(),
              Contains(IsIssueWithSubstring(Severity::kError,
                                            "no matching overload for '_+_'"
                                            " applied to '(int, string)'")));
}

TEST(TypeCheckerImplTest, ParmeterizedOvlResolutionMatch) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  env.InsertVariableIfAbsent(MakeVariableDecl("x", IntType()));
  env.InsertVariableIfAbsent(MakeVariableDecl("y", StringType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("([x] + []) == [x]"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());
}

TEST(TypeCheckerImplTest, AliasedTypeVarSameType) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  google::protobuf::Arena arena;
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast,
                       MakeTestParsedAst("[].exists(x, x == 10 || x == '10')"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_FALSE(result.IsValid());
  EXPECT_THAT(
      result.GetIssues(),
      ElementsAre(IsIssueWithSubstring(
          Severity::kError, "no matching overload for '_==_' applied to")));
}

TEST(TypeCheckerImplTest, TypeVarRange) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  google::protobuf::Arena arena;

  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());
  env.InsertFunctionIfAbsent(MakeIdentFunction());
  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast,
                       MakeTestParsedAst("identity([]).exists(x, x == 10 )"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid()) << absl::StrJoin(result.GetIssues(), "\n");
}

TEST(TypeCheckerImplTest, WellKnownTypeCreation) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  env.AddTypeProvider(std::make_unique<TypeIntrospector>());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(
      auto ast, MakeTestParsedAst("google.protobuf.Int32Value{value: 10}"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Ast> checked_ast, result.ReleaseAst());

  const auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_THAT(ast_impl.type_map(),
              Contains(Pair(ast_impl.root_expr().id(),
                            Eq(AstType(ast_internal::PrimitiveTypeWrapper(
                                ast_internal::PrimitiveType::kInt64))))));
  EXPECT_THAT(ast_impl.reference_map(),
              Contains(Pair(ast_impl.root_expr().id(),
                            Property(&ast_internal::Reference::name,
                                     "google.protobuf.Int32Value"))));
}

TEST(TypeCheckerImplTest, TypeInferredFromStructCreation) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  env.AddTypeProvider(std::make_unique<TypeIntrospector>());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast,
                       MakeTestParsedAst("google.protobuf.Struct{fields: {}}"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Ast> checked_ast, result.ReleaseAst());

  const auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  int64_t map_expr_id =
      ast_impl.root_expr().struct_expr().fields().at(0).value().id();
  ASSERT_NE(map_expr_id, 0);
  EXPECT_THAT(
      ast_impl.type_map(),
      Contains(Pair(
          map_expr_id,
          Eq(AstType(ast_internal::MapType(
              std::make_unique<AstType>(ast_internal::PrimitiveType::kString),
              std::make_unique<AstType>(ast_internal::DynamicType())))))));
}

TEST(TypeCheckerImplTest, ContainerLookupForMessageCreation) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  env.set_container("google.protobuf");
  env.AddTypeProvider(std::make_unique<TypeIntrospector>());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("Int32Value{value: 10}"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Ast> checked_ast, result.ReleaseAst());

  const auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_THAT(ast_impl.type_map(),
              Contains(Pair(ast_impl.root_expr().id(),
                            Eq(AstType(ast_internal::PrimitiveTypeWrapper(
                                ast_internal::PrimitiveType::kInt64))))));
  EXPECT_THAT(ast_impl.reference_map(),
              Contains(Pair(ast_impl.root_expr().id(),
                            Property(&ast_internal::Reference::name,
                                     "google.protobuf.Int32Value"))));
}

TEST(TypeCheckerImplTest, ContainerLookupForMessageCreationNoRewrite) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  env.set_container("google.protobuf");
  env.AddTypeProvider(std::make_unique<TypeIntrospector>());

  CheckerOptions options;
  options.update_struct_type_names = false;
  TypeCheckerImpl impl(std::move(env), options);
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("Int32Value{value: 10}"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Ast> checked_ast, result.ReleaseAst());

  const auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_THAT(ast_impl.type_map(),
              Contains(Pair(ast_impl.root_expr().id(),
                            Eq(AstType(ast_internal::PrimitiveTypeWrapper(
                                ast_internal::PrimitiveType::kInt64))))));
  EXPECT_THAT(ast_impl.reference_map(),
              Contains(Pair(ast_impl.root_expr().id(),
                            Property(&ast_internal::Reference::name,
                                     "google.protobuf.Int32Value"))));
  EXPECT_THAT(ast_impl.root_expr().struct_expr(),
              Property(&StructExpr::name, "Int32Value"));
}

TEST(TypeCheckerImplTest, EnumValueCopiedToReferenceMap) {
  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  env.set_container("google.api.expr.test.v1.proto3");

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast,
                       MakeTestParsedAst("TestAllTypes.NestedEnum.BAZ"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Ast> checked_ast, result.ReleaseAst());

  const auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  auto ref_iter = ast_impl.reference_map().find(ast_impl.root_expr().id());
  ASSERT_NE(ref_iter, ast_impl.reference_map().end());
  EXPECT_EQ(ref_iter->second.name(),
            "google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum.BAZ");
  EXPECT_EQ(ref_iter->second.value().int_value(), 2);
}

struct CheckedExprTestCase {
  std::string expr;
  ast_internal::Type expected_result_type;
  std::string error_substring;
};

class WktCreationTest : public testing::TestWithParam<CheckedExprTestCase> {};

TEST_P(WktCreationTest, MessageCreation) {
  google::protobuf::Arena arena;
  const CheckedExprTestCase& test_case = GetParam();
  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  env.AddTypeProvider(std::make_unique<TypeIntrospector>());
  env.set_container("google.protobuf");

  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst(test_case.expr));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  if (!test_case.error_substring.empty()) {
    EXPECT_THAT(result.GetIssues(),
                Contains(IsIssueWithSubstring(Severity::kError,
                                              test_case.error_substring)));
    return;
  }

  ASSERT_TRUE(result.IsValid())
      << absl::StrJoin(result.GetIssues(), "\n",
                       [](std::string* out, const TypeCheckIssue& issue) {
                         absl::StrAppend(out, issue.message());
                       });

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Ast> checked_ast, result.ReleaseAst());

  const auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_THAT(ast_impl.type_map(),
              Contains(Pair(ast_impl.root_expr().id(),
                            Eq(test_case.expected_result_type))));
}

INSTANTIATE_TEST_SUITE_P(
    WellKnownTypes, WktCreationTest,
    ::testing::Values(
        CheckedExprTestCase{
            .expr = "google.protobuf.Int32Value{value: 10}",
            .expected_result_type = AstType(ast_internal::PrimitiveTypeWrapper(
                ast_internal::PrimitiveType::kInt64)),
        },
        CheckedExprTestCase{
            .expr = ".google.protobuf.Int32Value{value: 10}",
            .expected_result_type = AstType(ast_internal::PrimitiveTypeWrapper(
                ast_internal::PrimitiveType::kInt64)),
        },
        CheckedExprTestCase{
            .expr = "Int32Value{value: 10}",
            .expected_result_type = AstType(ast_internal::PrimitiveTypeWrapper(
                ast_internal::PrimitiveType::kInt64)),
        },
        CheckedExprTestCase{
            .expr = "google.protobuf.Int32Value{value: '10'}",
            .expected_result_type = AstType(),
            .error_substring = "expected type of field 'value' is 'int' but "
                               "provided type is 'string'"},
        CheckedExprTestCase{
            .expr = "google.protobuf.Int32Value{not_a_field: '10'}",
            .expected_result_type = AstType(),
            .error_substring = "undefined field 'not_a_field' not found in "
                               "struct 'google.protobuf.Int32Value'"},
        CheckedExprTestCase{
            .expr = "NotAType{not_a_field: '10'}",
            .expected_result_type = AstType(),
            .error_substring =
                "undeclared reference to 'NotAType' (in container "
                "'google.protobuf')"},
        CheckedExprTestCase{
            .expr = ".protobuf.Int32Value{value: 10}",
            .expected_result_type = AstType(),
            .error_substring =
                "undeclared reference to '.protobuf.Int32Value' (in container "
                "'google.protobuf')"},
        CheckedExprTestCase{
            .expr = "Int32Value{value: 10}.value",
            .expected_result_type = AstType(),
            .error_substring =
                "expression of type 'google.protobuf.Int64Value' cannot be the "
                "operand of a select operation"},
        CheckedExprTestCase{
            .expr = "Int64Value{value: 10}",
            .expected_result_type = AstType(ast_internal::PrimitiveTypeWrapper(
                ast_internal::PrimitiveType::kInt64)),
        },
        CheckedExprTestCase{
            .expr = "BoolValue{value: true}",
            .expected_result_type = AstType(ast_internal::PrimitiveTypeWrapper(
                ast_internal::PrimitiveType::kBool)),
        },
        CheckedExprTestCase{
            .expr = "UInt64Value{value: 10u}",
            .expected_result_type = AstType(ast_internal::PrimitiveTypeWrapper(
                ast_internal::PrimitiveType::kUint64)),
        },
        CheckedExprTestCase{
            .expr = "UInt32Value{value: 10u}",
            .expected_result_type = AstType(ast_internal::PrimitiveTypeWrapper(
                ast_internal::PrimitiveType::kUint64)),
        },
        CheckedExprTestCase{
            .expr = "FloatValue{value: 1.25}",
            .expected_result_type = AstType(ast_internal::PrimitiveTypeWrapper(
                ast_internal::PrimitiveType::kDouble)),
        },
        CheckedExprTestCase{
            .expr = "DoubleValue{value: 1.25}",
            .expected_result_type = AstType(ast_internal::PrimitiveTypeWrapper(
                ast_internal::PrimitiveType::kDouble)),
        },
        CheckedExprTestCase{
            .expr = "StringValue{value: 'test'}",
            .expected_result_type = AstType(ast_internal::PrimitiveTypeWrapper(
                ast_internal::PrimitiveType::kString)),
        },
        CheckedExprTestCase{
            .expr = "BytesValue{value: b'test'}",
            .expected_result_type = AstType(ast_internal::PrimitiveTypeWrapper(
                ast_internal::PrimitiveType::kBytes)),
        },
        CheckedExprTestCase{
            .expr = "Duration{seconds: 10, nanos: 11}",
            .expected_result_type =
                AstType(ast_internal::WellKnownType::kDuration),
        },
        CheckedExprTestCase{
            .expr = "Timestamp{seconds: 10, nanos: 11}",
            .expected_result_type =
                AstType(ast_internal::WellKnownType::kTimestamp),
        },
        CheckedExprTestCase{
            .expr = "Struct{fields: {'key': 'value'}}",
            .expected_result_type = AstType(ast_internal::MapType(
                std::make_unique<AstType>(ast_internal::PrimitiveType::kString),
                std::make_unique<AstType>(ast_internal::DynamicType()))),
        },
        CheckedExprTestCase{
            .expr = "ListValue{values: [1, 2, 3]}",
            .expected_result_type = AstType(ast_internal::ListType(
                std::make_unique<AstType>(ast_internal::DynamicType()))),
        },
        CheckedExprTestCase{
            .expr = R"cel(
              Any{
                type_url:'type.googleapis.com/google.protobuf.Int32Value',
                value: b''
              })cel",
            .expected_result_type = AstType(ast_internal::WellKnownType::kAny),
        },
        CheckedExprTestCase{
            .expr = "Int64Value{value: 10} + 1",
            .expected_result_type =
                AstType(ast_internal::PrimitiveType::kInt64),
        },
        CheckedExprTestCase{
            .expr = "BoolValue{value: false} || true",
            .expected_result_type = AstType(ast_internal::PrimitiveType::kBool),
        }));

class GenericMessagesTest : public testing::TestWithParam<CheckedExprTestCase> {
};

TEST_P(GenericMessagesTest, TypeChecksProto3) {
  const CheckedExprTestCase& test_case = GetParam();
  google::protobuf::Arena arena;

  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  env.set_container("google.api.expr.test.v1.proto3");
  google::protobuf::LinkMessageReflection<testpb3::TestAllTypes>();

  ASSERT_TRUE(env.InsertVariableIfAbsent(MakeVariableDecl(
      "test_msg", MessageType(testpb3::TestAllTypes::descriptor()))));
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst(test_case.expr));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  if (!test_case.error_substring.empty()) {
    EXPECT_THAT(result.GetIssues(),
                Contains(IsIssueWithSubstring(Severity::kError,
                                              test_case.error_substring)));
    return;
  }

  ASSERT_TRUE(result.IsValid())
      << absl::StrJoin(result.GetIssues(), "\n",
                       [](std::string* out, const TypeCheckIssue& issue) {
                         absl::StrAppend(out, issue.message());
                       });

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Ast> checked_ast, result.ReleaseAst());

  const auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_THAT(ast_impl.type_map(),
              Contains(Pair(ast_impl.root_expr().id(),
                            Eq(test_case.expected_result_type))));
}

INSTANTIATE_TEST_SUITE_P(
    TestAllTypesCreation, GenericMessagesTest,
    ::testing::Values(
        CheckedExprTestCase{
            .expr = "TestAllTypes{not_a_field: 10}",
            .expected_result_type = AstType(),
            .error_substring =
                "undefined field 'not_a_field' not found in "
                "struct 'google.api.expr.test.v1.proto3.TestAllTypes'"},
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_int64: 10}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_int64: 'string'}",
            .expected_result_type = AstType(),
            .error_substring =
                "expected type of field 'single_int64' is 'int' but "
                "provided type is 'string'"},
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_int32: 10}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_uint64: 10u}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_uint32: 10u}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_sint64: 10}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_sint32: 10}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_fixed64: 10u}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_fixed32: 10u}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_sfixed64: 10}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_sfixed32: 10}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_double: 1.25}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_float: 1.25}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_string: 'string'}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_bool: true}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_bytes: b'string'}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        // Well-known
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_any: TestAllTypes{single_int64: 10}}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_any: 1}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_any: 'string'}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_any: ['string']}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_duration: duration('1s')}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_timestamp: timestamp(0)}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_struct: {}}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_struct: {'key': 'value'}}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_struct: {1: 2}}",
            .expected_result_type = AstType(),
            .error_substring = "expected type of field 'single_struct' is "
                               "'map<string, dyn>' but "
                               "provided type is 'map<int, int>'"},
        CheckedExprTestCase{
            .expr = "TestAllTypes{list_value: [1, 2, 3]}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{list_value: []}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{list_value: 1}",
            .expected_result_type = AstType(),
            .error_substring =
                "expected type of field 'list_value' is 'list<dyn>' but "
                "provided type is 'int'"},
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_int64_wrapper: 1}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_int64_wrapper: null}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_value: null}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_value: 1.0}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_value: 'string'}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_value: {'string': 'string'}}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_value: ['string']}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{repeated_int64: [1, 2, 3]}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{repeated_int64: ['string']}",
            .expected_result_type = AstType(),
            .error_substring =
                "expected type of field 'repeated_int64' is 'list<int>'"},
        CheckedExprTestCase{
            .expr = "TestAllTypes{map_string_int64: ['string']}",
            .expected_result_type = AstType(),
            .error_substring = "expected type of field 'map_string_int64' is "
                               "'map<string, int>'"},
        CheckedExprTestCase{
            .expr = "TestAllTypes{map_string_int64: {'string': 1}}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_nested_enum: 1}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr =
                "TestAllTypes{single_nested_enum: TestAllTypes.NestedEnum.BAR}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes.NestedEnum.BAR",
            .expected_result_type =
                AstType(ast_internal::PrimitiveType::kInt64),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes",
            .expected_result_type =
                AstType(std::make_unique<AstType>(ast_internal::MessageType(
                    "google.api.expr.test.v1.proto3.TestAllTypes"))),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes == type(TestAllTypes{})",
            .expected_result_type = AstType(ast_internal::PrimitiveType::kBool),
        },
        // Special case for the NullValue enum.
        CheckedExprTestCase{
            .expr = "TestAllTypes{null_value: 0}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{null_value: null}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        // Legacy nullability behaviors.
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_duration: null}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_timestamp: null}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_nested_message: null}",
            .expected_result_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes")),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{}.single_duration == null",
            .expected_result_type = AstType(ast_internal::PrimitiveType::kBool),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{}.single_timestamp == null",
            .expected_result_type = AstType(ast_internal::PrimitiveType::kBool),
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{}.single_nested_message == null",
            .expected_result_type = AstType(ast_internal::PrimitiveType::kBool),
        }));

INSTANTIATE_TEST_SUITE_P(
    TestAllTypesFieldSelection, GenericMessagesTest,
    ::testing::Values(
        CheckedExprTestCase{
            .expr = "test_msg.not_a_field",
            .expected_result_type = AstType(),
            .error_substring =
                "undefined field 'not_a_field' not found in "
                "struct 'google.api.expr.test.v1.proto3.TestAllTypes'"},
        CheckedExprTestCase{
            .expr = "test_msg.single_int64",
            .expected_result_type =
                AstType(ast_internal::PrimitiveType::kInt64),
        },
        CheckedExprTestCase{
            .expr = "test_msg.single_nested_enum",
            .expected_result_type =
                AstType(ast_internal::PrimitiveType::kInt64),
        },
        CheckedExprTestCase{
            .expr = "test_msg.single_nested_enum == 1",
            .expected_result_type = AstType(ast_internal::PrimitiveType::kBool),
        },
        CheckedExprTestCase{
            .expr =
                "test_msg.single_nested_enum == TestAllTypes.NestedEnum.BAR",
            .expected_result_type = AstType(ast_internal::PrimitiveType::kBool),
        },
        CheckedExprTestCase{
            .expr = "has(test_msg.not_a_field)",
            .expected_result_type = AstType(),
            .error_substring =
                "undefined field 'not_a_field' not found in "
                "struct 'google.api.expr.test.v1.proto3.TestAllTypes'"},
        CheckedExprTestCase{
            .expr = "has(test_msg.single_int64)",
            .expected_result_type = AstType(ast_internal::PrimitiveType::kBool),
        },
        CheckedExprTestCase{
            .expr = "test_msg.single_int32",
            .expected_result_type =
                AstType(ast_internal::PrimitiveType::kInt64),
        },
        CheckedExprTestCase{
            .expr = "test_msg.single_uint64",
            .expected_result_type =
                AstType(ast_internal::PrimitiveType::kUint64),
        },
        CheckedExprTestCase{
            .expr = "test_msg.single_uint32",
            .expected_result_type =
                AstType(ast_internal::PrimitiveType::kUint64),
        },
        CheckedExprTestCase{
            .expr = "test_msg.single_sint64",
            .expected_result_type =
                AstType(ast_internal::PrimitiveType::kInt64),
        },
        CheckedExprTestCase{
            .expr = "test_msg.single_sint32",
            .expected_result_type =
                AstType(ast_internal::PrimitiveType::kInt64),
        },
        CheckedExprTestCase{
            .expr = "test_msg.single_fixed64",
            .expected_result_type =
                AstType(ast_internal::PrimitiveType::kUint64),
        },
        CheckedExprTestCase{
            .expr = "test_msg.single_fixed32",
            .expected_result_type =
                AstType(ast_internal::PrimitiveType::kUint64),
        },
        CheckedExprTestCase{
            .expr = "test_msg.single_sfixed64",
            .expected_result_type =
                AstType(ast_internal::PrimitiveType::kInt64),
        },
        CheckedExprTestCase{
            .expr = "test_msg.single_sfixed32",
            .expected_result_type =
                AstType(ast_internal::PrimitiveType::kInt64),
        },
        CheckedExprTestCase{
            .expr = "test_msg.single_float",
            .expected_result_type =
                AstType(ast_internal::PrimitiveType::kDouble),
        },
        CheckedExprTestCase{
            .expr = "test_msg.single_double",
            .expected_result_type =
                AstType(ast_internal::PrimitiveType::kDouble),
        },
        CheckedExprTestCase{
            .expr = "test_msg.single_string",
            .expected_result_type =
                AstType(ast_internal::PrimitiveType::kString),
        },
        CheckedExprTestCase{
            .expr = "test_msg.single_bool",
            .expected_result_type = AstType(ast_internal::PrimitiveType::kBool),
        },
        CheckedExprTestCase{
            .expr = "test_msg.single_bytes",
            .expected_result_type =
                AstType(ast_internal::PrimitiveType::kBytes),
        },
        // Basic tests for containers. This is covered in more detail in
        // conformance tests and the type provider implementation.
        CheckedExprTestCase{
            .expr = "test_msg.repeated_int32",
            .expected_result_type =
                AstType(ast_internal::ListType(std::make_unique<AstType>(
                    ast_internal::PrimitiveType::kInt64))),
        },
        CheckedExprTestCase{
            .expr = "test_msg.repeated_string",
            .expected_result_type =
                AstType(ast_internal::ListType(std::make_unique<AstType>(
                    ast_internal::PrimitiveType::kString))),
        },
        CheckedExprTestCase{
            .expr = "test_msg.map_bool_bool",
            .expected_result_type = AstType(ast_internal::MapType(
                std::make_unique<AstType>(ast_internal::PrimitiveType::kBool),
                std::make_unique<AstType>(ast_internal::PrimitiveType::kBool))),
        },
        // Note: The Go type checker permits this so C++ does as well. Some
        // test cases expect that field selection on a map is always allowed,
        // even if a specific, non-string key type is known.
        CheckedExprTestCase{
            .expr = "test_msg.map_bool_bool.field_like_key",
            .expected_result_type = AstType(ast_internal::PrimitiveType::kBool),
        },
        CheckedExprTestCase{
            .expr = "test_msg.map_string_int64",
            .expected_result_type = AstType(ast_internal::MapType(
                std::make_unique<AstType>(ast_internal::PrimitiveType::kString),
                std::make_unique<AstType>(
                    ast_internal::PrimitiveType::kInt64))),
        },
        CheckedExprTestCase{
            .expr = "test_msg.map_string_int64.field_like_key",
            .expected_result_type =
                AstType(ast_internal::PrimitiveType::kInt64),
        },
        // Well-known
        CheckedExprTestCase{
            .expr = "test_msg.single_duration",
            .expected_result_type =
                AstType(ast_internal::WellKnownType::kDuration),
        },
        CheckedExprTestCase{
            .expr = "test_msg.single_timestamp",
            .expected_result_type =
                AstType(ast_internal::WellKnownType::kTimestamp),
        },
        CheckedExprTestCase{
            .expr = "test_msg.single_any",
            .expected_result_type = AstType(ast_internal::WellKnownType::kAny),
        },
        CheckedExprTestCase{
            .expr = "test_msg.single_int64_wrapper",
            .expected_result_type = AstType(ast_internal::PrimitiveTypeWrapper(
                ast_internal::PrimitiveType::kInt64)),
        },
        CheckedExprTestCase{
            .expr = "test_msg.single_struct",
            .expected_result_type = AstType(ast_internal::MapType(
                std::make_unique<AstType>(ast_internal::PrimitiveType::kString),
                std::make_unique<AstType>(ast_internal::DynamicType()))),
        },
        CheckedExprTestCase{
            .expr = "test_msg.list_value",
            .expected_result_type = AstType(ast_internal::ListType(
                std::make_unique<AstType>(ast_internal::DynamicType()))),
        },
        CheckedExprTestCase{
            .expr = "test_msg.list_value",
            .expected_result_type = AstType(ast_internal::ListType(
                std::make_unique<AstType>(ast_internal::DynamicType()))),
        },
        // Basic tests for nested messages.
        CheckedExprTestCase{
            .expr = "NestedTestAllTypes{}.child.child.payload.single_int64",
            .expected_result_type =
                AstType(ast_internal::PrimitiveType::kInt64),
        },
        CheckedExprTestCase{
            .expr = "test_msg.single_struct.field.nested_field",
            .expected_result_type = AstType(ast_internal::DynamicType()),
        },
        CheckedExprTestCase{
            .expr = "{}.field.nested_field",
            .expected_result_type = AstType(ast_internal::DynamicType()),
        }));

class StrictNullAssignmentTest
    : public testing::TestWithParam<CheckedExprTestCase> {};

TEST_P(StrictNullAssignmentTest, TypeChecksProto3) {
  const CheckedExprTestCase& test_case = GetParam();
  google::protobuf::Arena arena;

  TypeCheckEnv env(GetSharedTestingDescriptorPool());
  env.set_container("google.api.expr.test.v1.proto3");
  google::protobuf::LinkMessageReflection<testpb3::TestAllTypes>();

  ASSERT_TRUE(env.InsertVariableIfAbsent(MakeVariableDecl(
      "test_msg", MessageType(testpb3::TestAllTypes::descriptor()))));
  ASSERT_THAT(RegisterMinimalBuiltins(&arena, env), IsOk());
  CheckerOptions options;
  options.enable_legacy_null_assignment = false;
  TypeCheckerImpl impl(std::move(env), options);
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst(test_case.expr));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  if (!test_case.error_substring.empty()) {
    EXPECT_THAT(result.GetIssues(),
                Contains(IsIssueWithSubstring(Severity::kError,
                                              test_case.error_substring)));
    return;
  }

  ASSERT_TRUE(result.IsValid())
      << absl::StrJoin(result.GetIssues(), "\n",
                       [](std::string* out, const TypeCheckIssue& issue) {
                         absl::StrAppend(out, issue.message());
                       });

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Ast> checked_ast, result.ReleaseAst());

  const auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_THAT(ast_impl.type_map(),
              Contains(Pair(ast_impl.root_expr().id(),
                            Eq(test_case.expected_result_type))));
}

INSTANTIATE_TEST_SUITE_P(
    TestStrictNullAssignment, StrictNullAssignmentTest,
    ::testing::Values(
        // Legacy nullability behaviors rejected.
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_duration: null}",
            .expected_result_type = AstType(),
            .error_substring =
                "'single_duration' is 'google.protobuf.Duration' but provided "
                "type is 'null_type'"},
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_timestamp: null}",
            .expected_result_type = AstType(),
            .error_substring =
                "'single_timestamp' is 'google.protobuf.Timestamp' but "
                "provided type is 'null_type'"},
        CheckedExprTestCase{
            .expr = "TestAllTypes{single_nested_message: null}",
            .expected_result_type = AstType(),
            // Debug string includes descriptor address.
            .error_substring = "but provided type is 'null_type'"},
        CheckedExprTestCase{
            .expr = "TestAllTypes{}.single_duration == null",
            .expected_result_type = AstType(),
            .error_substring = "no matching overload for '_==_'",
        },
        CheckedExprTestCase{
            .expr = "TestAllTypes{}.single_timestamp == null",
            .expected_result_type = AstType(),
            .error_substring = "no matching overload for '_==_'"},
        CheckedExprTestCase{
            .expr = "TestAllTypes{}.single_nested_message == null",
            .expected_result_type = AstType(),
            .error_substring = "no matching overload for '_==_'",
        }));

}  // namespace
}  // namespace checker_internal
}  // namespace cel
