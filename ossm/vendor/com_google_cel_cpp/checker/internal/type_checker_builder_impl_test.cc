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
#include "checker/internal/test_ast_helpers.h"
#include "checker/type_checker.h"
#include "checker/validation_result.h"
#include "common/ast/ast_impl.h"
#include "common/ast/expr.h"
#include "common/decl.h"
#include "common/type.h"
#include "common/type_introspector.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"

namespace cel::checker_internal {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::cel::ast_internal::AstImpl;

using AstType = cel::ast_internal::Type;

struct ContextDeclsTestCase {
  std::string expr;
  AstType expected_type;
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

  const auto& ast_impl = AstImpl::CastFromPublicAst(*result.GetAst());

  EXPECT_EQ(ast_impl.GetReturnType(), GetParam().expected_type);
}

INSTANTIATE_TEST_SUITE_P(
    TestAllTypes, ContextDeclsFieldsDefinedTest,
    testing::Values(
        ContextDeclsTestCase{"single_int64",
                             AstType(ast_internal::PrimitiveType::kInt64)},
        ContextDeclsTestCase{"single_uint32",
                             AstType(ast_internal::PrimitiveType::kUint64)},
        ContextDeclsTestCase{"single_double",
                             AstType(ast_internal::PrimitiveType::kDouble)},
        ContextDeclsTestCase{"single_string",
                             AstType(ast_internal::PrimitiveType::kString)},
        ContextDeclsTestCase{"single_any",
                             AstType(ast_internal::WellKnownType::kAny)},
        ContextDeclsTestCase{"single_duration",
                             AstType(ast_internal::WellKnownType::kDuration)},
        ContextDeclsTestCase{"single_bool_wrapper",
                             AstType(ast_internal::PrimitiveTypeWrapper(
                                 ast_internal::PrimitiveType::kBool))},
        ContextDeclsTestCase{
            "list_value",
            AstType(ast_internal::ListType(
                std::make_unique<AstType>(ast_internal::DynamicType())))},
        ContextDeclsTestCase{
            "standalone_message",
            AstType(ast_internal::MessageType(
                "cel.expr.conformance.proto3.TestAllTypes.NestedMessage"))},
        ContextDeclsTestCase{"standalone_enum",
                             AstType(ast_internal::PrimitiveType::kInt64)},
        ContextDeclsTestCase{
            "repeated_bytes",
            AstType(ast_internal::ListType(std::make_unique<AstType>(
                ast_internal::PrimitiveType::kBytes)))},
        ContextDeclsTestCase{
            "repeated_nested_message",
            AstType(ast_internal::ListType(std::make_unique<
                                           AstType>(ast_internal::MessageType(
                "cel.expr.conformance.proto3.TestAllTypes.NestedMessage"))))},
        ContextDeclsTestCase{
            "map_int32_timestamp",
            AstType(ast_internal::MapType(
                std::make_unique<AstType>(ast_internal::PrimitiveType::kInt64),
                std::make_unique<AstType>(
                    ast_internal::WellKnownType::kTimestamp)))},
        ContextDeclsTestCase{
            "single_struct",
            AstType(ast_internal::MapType(
                std::make_unique<AstType>(ast_internal::PrimitiveType::kString),
                std::make_unique<AstType>(ast_internal::DynamicType())))}));

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

TEST(ContextDeclsTest, ReplaceVariable) {
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

  const auto& ast_impl = AstImpl::CastFromPublicAst(*result.GetAst());

  EXPECT_EQ(ast_impl.GetReturnType(),
            AstType(ast_internal::PrimitiveType::kString));
}

}  // namespace
}  // namespace cel::checker_internal
