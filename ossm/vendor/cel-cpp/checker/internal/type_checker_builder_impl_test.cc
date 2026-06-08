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

#include "checker/internal/type_checker_builder_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "checker/checker_options.h"
#include "checker/internal/test_ast_helpers.h"
#include "checker/type_checker.h"
#include "checker/validation_result.h"
#include "common/ast.h"
#include "common/decl.h"
#include "common/type.h"
#include "common/type_introspector.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "google/protobuf/arena.h"

namespace cel::checker_internal {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;


struct ContextDeclsTestCase {
  std::string expr;
  TypeSpec expected_type;
};

class ContextDeclsFieldsDefinedTest
    : public testing::TestWithParam<ContextDeclsTestCase> {};

TEST_P(ContextDeclsFieldsDefinedTest, ContextDeclsFieldsDefined) {
  TypeCheckerBuilderImpl builder(internal::GetSharedTestingDescriptorPool(),
                                 {});
  ASSERT_THAT(
      builder.AddContextDeclaration("cel.expr.conformance.proto3.TestAllTypes"),
      IsOk());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<TypeChecker> type_checker,
                       builder.Build());
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst(GetParam().expr));
  ASSERT_OK_AND_ASSIGN(ValidationResult result,
                       type_checker->Check(std::move(ast)));

  ASSERT_TRUE(result.IsValid());

  EXPECT_EQ(result.GetAst()->GetReturnType(), GetParam().expected_type);
}

INSTANTIATE_TEST_SUITE_P(
    TestAllTypes, ContextDeclsFieldsDefinedTest,
    testing::Values(
        ContextDeclsTestCase{"single_int64", TypeSpec(PrimitiveType::kInt64)},
        ContextDeclsTestCase{"single_uint32", TypeSpec(PrimitiveType::kUint64)},
        ContextDeclsTestCase{"single_double", TypeSpec(PrimitiveType::kDouble)},
        ContextDeclsTestCase{"single_string", TypeSpec(PrimitiveType::kString)},
        ContextDeclsTestCase{"single_any", TypeSpec(WellKnownTypeSpec::kAny)},
        ContextDeclsTestCase{"single_duration",
                             TypeSpec(WellKnownTypeSpec::kDuration)},
        ContextDeclsTestCase{
            "single_bool_wrapper",
            TypeSpec(PrimitiveTypeWrapper(PrimitiveType::kBool))},
        ContextDeclsTestCase{
            "list_value",
            TypeSpec(ListTypeSpec(std::make_unique<TypeSpec>(DynTypeSpec())))},
        ContextDeclsTestCase{
            "standalone_message",
            TypeSpec(MessageTypeSpec(
                "cel.expr.conformance.proto3.TestAllTypes.NestedMessage"))},
        ContextDeclsTestCase{"standalone_enum",
                             TypeSpec(PrimitiveType::kInt64)},
        ContextDeclsTestCase{"repeated_bytes",
                             TypeSpec(ListTypeSpec(std::make_unique<TypeSpec>(
                                 PrimitiveType::kBytes)))},
        ContextDeclsTestCase{
            "repeated_nested_message",
            TypeSpec(ListTypeSpec(std::make_unique<TypeSpec>(MessageTypeSpec(
                "cel.expr.conformance.proto3.TestAllTypes.NestedMessage"))))},
        ContextDeclsTestCase{
            "map_int32_timestamp",
            TypeSpec(MapTypeSpec(
                std::make_unique<TypeSpec>(PrimitiveType::kInt64),
                std::make_unique<TypeSpec>(WellKnownTypeSpec::kTimestamp)))},
        ContextDeclsTestCase{
            "single_struct",
            TypeSpec(
                MapTypeSpec(std::make_unique<TypeSpec>(PrimitiveType::kString),
                            std::make_unique<TypeSpec>(DynTypeSpec())))}));

TEST(ContextDeclsTest, ErrorOnDuplicateContextDeclaration) {
  TypeCheckerBuilderImpl builder(internal::GetSharedTestingDescriptorPool(),
                                 {});
  ASSERT_THAT(
      builder.AddContextDeclaration("cel.expr.conformance.proto3.TestAllTypes"),
      IsOk());
  EXPECT_THAT(
      builder.AddContextDeclaration("cel.expr.conformance.proto3.TestAllTypes"),
      StatusIs(absl::StatusCode::kAlreadyExists,
               "context declaration 'cel.expr.conformance.proto3.TestAllTypes' "
               "already exists"));
}

TEST(ContextDeclsTest, ErrorOnContextDeclarationNotFound) {
  TypeCheckerBuilderImpl builder(internal::GetSharedTestingDescriptorPool(),
                                 {});
  EXPECT_THAT(
      builder.AddContextDeclaration("com.example.UnknownType"),
      StatusIs(absl::StatusCode::kNotFound,
               "context declaration 'com.example.UnknownType' not found"));
}

TEST(ContextDeclsTest, ErrorOnNonStructMessageType) {
  TypeCheckerBuilderImpl builder(internal::GetSharedTestingDescriptorPool(),
                                 {});
  EXPECT_THAT(
      builder.AddContextDeclaration("google.protobuf.Timestamp"),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          "context declaration 'google.protobuf.Timestamp' is not a struct"));
}

TEST(ContextDeclsTest, CustomStructNotSupported) {
  TypeCheckerBuilderImpl builder(internal::GetSharedTestingDescriptorPool(),
                                 {});
  class MyTypeProvider : public cel::TypeIntrospector {
   public:
    absl::StatusOr<absl::optional<Type>> FindTypeImpl(
        absl::string_view name) const override {
      if (name == "com.example.MyStruct") {
        return common_internal::MakeBasicStructType("com.example.MyStruct");
      }
      return absl::nullopt;
    }
  };

  builder.AddTypeProvider(std::make_unique<MyTypeProvider>());

  EXPECT_THAT(builder.AddContextDeclaration("com.example.MyStruct"),
              StatusIs(absl::StatusCode::kNotFound,
                       "context declaration 'com.example.MyStruct' not found"));
}

TEST(ContextDeclsTest, ErrorOnOverlappingContextDeclaration) {
  TypeCheckerBuilderImpl builder(internal::GetSharedTestingDescriptorPool(),
                                 {});
  ASSERT_THAT(
      builder.AddContextDeclaration("cel.expr.conformance.proto3.TestAllTypes"),
      IsOk());
  // We resolve the context declaration variables at the Build() call, so the
  // error surfaces then.
  ASSERT_THAT(
      builder.AddContextDeclaration("cel.expr.conformance.proto2.TestAllTypes"),
      IsOk());

  EXPECT_THAT(
      builder.Build(),
      StatusIs(absl::StatusCode::kAlreadyExists,
               "variable 'single_int32' declared multiple times (from context "
               "declaration: 'cel.expr.conformance.proto2.TestAllTypes')"));
}

TEST(ContextDeclsTest, ErrorOnOverlappingVariableDeclaration) {
  TypeCheckerBuilderImpl builder(internal::GetSharedTestingDescriptorPool(),
                                 {});
  ASSERT_THAT(
      builder.AddContextDeclaration("cel.expr.conformance.proto3.TestAllTypes"),
      IsOk());
  ASSERT_THAT(builder.AddVariable(MakeVariableDecl("single_int64", IntType())),
              IsOk());

  EXPECT_THAT(builder.Build(),
              StatusIs(absl::StatusCode::kAlreadyExists,
                       "variable 'single_int64' declared multiple times"));
}

TEST(TypeCheckerBuilderImplTest,
     InvalidTypeParamNameVariableValidationDisabled) {
  CheckerOptions options;
  options.enable_type_parameter_name_validation = false;
  TypeCheckerBuilderImpl builder(internal::GetSharedTestingDescriptorPool(),
                                 options);
  ASSERT_THAT(builder.AddVariable(MakeVariableDecl("x", TypeParamType(""))),
              IsOk());
  ASSERT_THAT(builder.AddOrReplaceVariable(
                  MakeVariableDecl("x", TypeParamType("T% foo"))),
              IsOk());
}

TEST(TypeCheckerBuilderImplTest, ErrorOnUnspecifiedMessageType) {
  CheckerOptions options;
  options.enable_type_parameter_name_validation = true;
  TypeCheckerBuilderImpl builder(internal::GetSharedTestingDescriptorPool(),
                                 options);
  ASSERT_THAT(
      builder.AddVariable(MakeVariableDecl("x", MessageType())),
      StatusIs(absl::StatusCode::kInvalidArgument,
               "an empty message type cannot be used in a type declaration"));
}

TEST(TypeCheckerBuilderImplTest, ErrorOnInvalidTypeParamNameVariable) {
  CheckerOptions options;
  options.enable_type_parameter_name_validation = true;
  TypeCheckerBuilderImpl builder(internal::GetSharedTestingDescriptorPool(),
                                 options);
  ASSERT_THAT(builder.AddVariable(MakeVariableDecl("x", TypeParamType(""))),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "type parameter name '' is not a valid identifier"));
  ASSERT_THAT(
      builder.AddOrReplaceVariable(
          MakeVariableDecl("x", TypeParamType("T% foo"))),
      StatusIs(absl::StatusCode::kInvalidArgument,
               "type parameter name 'T% foo' is not a valid identifier"));
}

TEST(TypeCheckerBuilderImplTest, ErrorOnTooDeepTypeNestingVariable) {
  CheckerOptions options;
  options.max_type_decl_nesting = 2;
  google::protobuf::Arena arena;

  TypeCheckerBuilderImpl builder(internal::GetSharedTestingDescriptorPool(),
                                 options);
  ASSERT_THAT(builder.AddVariable(
                  MakeVariableDecl("x", TypeType(&arena, TypeParamType("T")))),
              IsOk());
  ASSERT_THAT(
      builder.AddOrReplaceVariable(MakeVariableDecl(
          "x", TypeType(&arena, TypeType(&arena, TypeParamType("T% foo"))))),
      StatusIs(absl::StatusCode::kInvalidArgument,
               "type nesting limit of 2 exceeded"));
}

TEST(TypeCheckerBuilderImplTest, ErrorOnInvalidTypeParamNameFunction) {
  CheckerOptions options;
  options.enable_type_parameter_name_validation = true;
  TypeCheckerBuilderImpl builder(internal::GetSharedTestingDescriptorPool(),
                                 options);
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(
      auto fn_decl,
      MakeFunctionDecl(
          "type2",
          MakeOverloadDecl("type2", TypeType(&arena, TypeParamType("")),
                           TypeParamType(""))));
  ASSERT_THAT(builder.AddFunction(fn_decl),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "type parameter name '' is not a valid identifier"));
}

TEST(TypeCheckerBuilderImplTest, ErrorOnTooDeepTypeNestingFunction) {
  CheckerOptions options;
  options.max_type_decl_nesting = 2;
  google::protobuf::Arena arena;

  TypeCheckerBuilderImpl builder(internal::GetSharedTestingDescriptorPool(),
                                 options);
  ASSERT_OK_AND_ASSIGN(
      auto fn_decl,
      MakeFunctionDecl(
          "add", MakeOverloadDecl("add_int", IntType(), IntType(), IntType())));
  ASSERT_THAT(builder.AddFunction(fn_decl), IsOk());

  Type list_type = ListType(&arena, ListType(&arena, IntType()));

  ASSERT_OK_AND_ASSIGN(
      fn_decl,
      MakeFunctionDecl("add", MakeOverloadDecl("add_list_list_int", list_type,
                                               list_type, list_type)));

  ASSERT_THAT(builder.MergeFunction(fn_decl),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "type nesting limit of 2 exceeded"));
}

TEST(TypeCheckerBuilderImplTest, ReplaceVariable) {
  TypeCheckerBuilderImpl builder(internal::GetSharedTestingDescriptorPool(),
                                 {});
  ASSERT_THAT(
      builder.AddContextDeclaration("cel.expr.conformance.proto3.TestAllTypes"),
      IsOk());
  ASSERT_THAT(builder.AddOrReplaceVariable(
                  MakeVariableDecl("single_int64", StringType())),
              IsOk());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<TypeChecker> type_checker,
                       builder.Build());
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("single_int64"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result,
                       type_checker->Check(std::move(ast)));

  ASSERT_TRUE(result.IsValid());

  const auto& checked_ast = *result.GetAst();

  EXPECT_EQ(checked_ast.GetReturnType(), TypeSpec(PrimitiveType::kString));
}

}  // namespace
}  // namespace cel::checker_internal
