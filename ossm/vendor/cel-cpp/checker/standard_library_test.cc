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

#include "checker/standard_library.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "checker/checker_options.h"
#include "checker/internal/test_ast_helpers.h"
#include "checker/type_checker.h"
#include "checker/type_checker_builder.h"
#include "checker/type_checker_builder_factory.h"
#include "checker/validation_result.h"
#include "common/ast.h"
#include "common/constant.h"
#include "common/decl.h"
#include "common/type.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "google/protobuf/arena.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::cel::Reference;
using ::cel::internal::GetSharedTestingDescriptorPool;
using ::testing::IsEmpty;
using ::testing::Pointee;
using ::testing::Property;

using AstType = cel::TypeSpec;

TEST(StandardLibraryTest, StandardLibraryAddsDecls) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool()));
  EXPECT_THAT(builder->AddLibrary(StandardCheckerLibrary()), IsOk());
  EXPECT_THAT(builder->Build(), IsOk());
}

TEST(StandardLibraryTest, StandardLibraryErrorsIfAddedTwice) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool()));
  EXPECT_THAT(builder->AddLibrary(StandardCheckerLibrary()), IsOk());
  EXPECT_THAT(builder->AddLibrary(StandardCheckerLibrary()),
              StatusIs(absl::StatusCode::kAlreadyExists));
}

TEST(StandardLibraryTest, ComprehensionVarsIndirectCyclicParamAssignability) {
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool()));
  ASSERT_THAT(builder->AddLibrary(StandardCheckerLibrary()), IsOk());

  // Note: this is atypical -- parameterized variables aren't well supported
  // outside of built-in syntax.
  // e.g. `list : Type(List(A))` is instantiated per reference to bind A to
  // the concrete type of a list in the same assignability context.
  //
  // Validate that parameterization is sanitized to be contextual
  // List(V) -> List(T%1)
  // Map(K, V) -> Map(T%2, T%3)
  Type list_type = ListType(&arena, TypeParamType("V"));
  Type map_type = MapType(&arena, TypeParamType("K"), TypeParamType("V"));

  ASSERT_THAT(builder->AddVariable(MakeVariableDecl("list_var", list_type)),
              IsOk());
  ASSERT_THAT(builder->AddVariable(MakeVariableDecl("map_var", map_type)),
              IsOk());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<TypeChecker> type_checker,
                       builder->Build());

  ASSERT_OK_AND_ASSIGN(
      auto ast, checker_internal::MakeTestParsedAst(
                    "list_var.exists(v,"
                    "  map_var.filter(k, map_var[k] > 1.0).size() > int(v)"
                    ")"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result,
                       type_checker->Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
}

TEST(StandardLibraryTest, ComprehensionResultTypeIsSubstituted) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool()));
  ASSERT_THAT(builder->AddLibrary(StandardCheckerLibrary()), IsOk());

  // Test that type for the result list of .map is resolved to a concrete type
  // when it is known. Checks for a bug where the result type is considered to
  // still be flexible and may widen to dyn.
  builder->set_container("cel.expr.conformance.proto2");
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<TypeChecker> type_checker,
                       builder->Build());

  ASSERT_OK_AND_ASSIGN(auto ast, checker_internal::MakeTestParsedAst(
                                     "[TestAllTypes{}]"
                                     ".map(x, x.repeated_nested_message[0])"
                                     ".map(x, x.bb)[0]"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result,
                       type_checker->Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty()) << result.FormatError();

  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());

  TypeSpec type = checked_ast->GetTypeOrDyn(checked_ast->root_expr().id());
  EXPECT_TRUE(type.has_primitive() &&
              type.primitive() == PrimitiveType::kInt64);
}

class StandardLibraryDefinitionsTest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_OK_AND_ASSIGN(
        std::unique_ptr<TypeCheckerBuilder> builder,
        CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool()));
    ASSERT_THAT(builder->AddLibrary(StandardCheckerLibrary()), IsOk());
    ASSERT_OK_AND_ASSIGN(stdlib_type_checker_, builder->Build());
  }

 protected:
  std::unique_ptr<TypeChecker> stdlib_type_checker_;
};

class StdlibTypeVarDefinitionTest
    : public StandardLibraryDefinitionsTest,
      public testing::WithParamInterface<std::string> {};

TEST_P(StdlibTypeVarDefinitionTest, DefinesTypeConstants) {
  auto ast = std::make_unique<Ast>();
  ast->mutable_root_expr().mutable_ident_expr().set_name(GetParam());
  ast->mutable_root_expr().set_id(1);

  ASSERT_OK_AND_ASSIGN(ValidationResult result,
                       stdlib_type_checker_->Check(std::move(ast)));

  EXPECT_THAT(result.GetIssues(), IsEmpty());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Ast> checked_ast, result.ReleaseAst());
  EXPECT_THAT(checked_ast->GetReference(1),
              Pointee(Property(&Reference::name, GetParam())));
  EXPECT_THAT(checked_ast->GetTypeOrDyn(1), Property(&AstType::has_type, true));
}

INSTANTIATE_TEST_SUITE_P(StdlibTypeVarDefinitions, StdlibTypeVarDefinitionTest,
                         ::testing::Values("bool", "bytes", "double", "dyn",
                                           "int", "list", "map", "null_type",
                                           "string", "type", "uint"),
                         [](const auto& info) -> std::string {
                           return info.param;
                         });

TEST_F(StandardLibraryDefinitionsTest, DefinesProtoStructNull) {
  auto ast = std::make_unique<Ast>();

  auto& enumerator = ast->mutable_root_expr();
  enumerator.set_id(4);
  enumerator.mutable_select_expr().set_field("NULL_VALUE");
  auto& enumeration = enumerator.mutable_select_expr().mutable_operand();
  enumeration.set_id(3);
  enumeration.mutable_select_expr().set_field("NullValue");
  auto& protobuf = enumeration.mutable_select_expr().mutable_operand();
  protobuf.set_id(2);
  protobuf.mutable_select_expr().set_field("protobuf");
  auto& google = protobuf.mutable_select_expr().mutable_operand();
  google.set_id(1);
  google.mutable_ident_expr().set_name("google");

  ASSERT_OK_AND_ASSIGN(ValidationResult result,
                       stdlib_type_checker_->Check(std::move(ast)));

  EXPECT_THAT(result.GetIssues(), IsEmpty());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Ast> checked_ast, result.ReleaseAst());
  EXPECT_THAT(checked_ast->GetReference(4),
              Pointee(Property(&Reference::name,
                               "google.protobuf.NullValue.NULL_VALUE")));
}

TEST_F(StandardLibraryDefinitionsTest, DefinesTypeType) {
  auto ast = std::make_unique<Ast>();

  auto& ident = ast->mutable_root_expr();
  ident.set_id(1);
  ident.mutable_ident_expr().set_name("type");

  ASSERT_OK_AND_ASSIGN(ValidationResult result,
                       stdlib_type_checker_->Check(std::move(ast)));

  EXPECT_THAT(result.GetIssues(), IsEmpty());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Ast> checked_ast, result.ReleaseAst());
  EXPECT_THAT(checked_ast->GetReference(1),
              Pointee(Property(&Reference::name, "type")));
  EXPECT_THAT(checked_ast->GetTypeOrDyn(1), Property(&AstType::has_type, true));
}

struct DefinitionsTestCase {
  std::string expr;
  bool type_check_success = true;
  CheckerOptions options;
};

class StdLibDefinitionsTest
    : public ::testing::TestWithParam<DefinitionsTestCase> {
 public:
};

// Basic coverage that the standard library definitions are defined.
// This is not intended to be exhaustive since it is expected to be covered by
// spec conformance tests.
//
// TODO(uncreated-issue/72): Tests are fairly minimal right now -- it's not possible to
// test thoroughly without a more complete implementation of the type checker.
// Type-parameterized functions are not yet checkable.
TEST_P(StdLibDefinitionsTest, Runner) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<TypeCheckerBuilder> builder,
      CreateTypeCheckerBuilder(GetSharedTestingDescriptorPool(),
                               GetParam().options));
  ASSERT_THAT(builder->AddLibrary(StandardCheckerLibrary()), IsOk());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<TypeChecker> type_checker,
                       builder->Build());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Ast> ast,
                       checker_internal::MakeTestParsedAst(GetParam().expr));

  ASSERT_OK_AND_ASSIGN(auto result, type_checker->Check(std::move(ast)));
  EXPECT_EQ(result.IsValid(), GetParam().type_check_success);
}

INSTANTIATE_TEST_SUITE_P(
    Strings, StdLibDefinitionsTest,
    ::testing::Values(DefinitionsTestCase{
                          /* .expr = */ "'123'.size()",
                      },
                      DefinitionsTestCase{
                          /* .expr = */ "size('123')",
                      },
                      DefinitionsTestCase{
                          /* .expr = */ "'123' + '123'",
                      },
                      DefinitionsTestCase{
                          /* .expr = */ "'123' + '123'",
                      },
                      DefinitionsTestCase{
                          /* .expr = */ "'123' + '123'",
                      },
                      DefinitionsTestCase{
                          /* .expr = */ "'123'.endsWith('123')",
                      },
                      DefinitionsTestCase{
                          /* .expr = */ "'123'.startsWith('123')",
                      },
                      DefinitionsTestCase{
                          /* .expr = */ "'123'.contains('123')",
                      },
                      DefinitionsTestCase{
                          /* .expr = */ "'123'.matches(r'123')",
                      },
                      DefinitionsTestCase{
                          /* .expr = */ "matches('123', r'123')",
                      }));

INSTANTIATE_TEST_SUITE_P(TypeCasts, StdLibDefinitionsTest,
                         ::testing::Values(DefinitionsTestCase{
                                               /* .expr = */ "int(1)",
                                           },
                                           DefinitionsTestCase{
                                               /* .expr = */ "uint(1)",
                                           },
                                           DefinitionsTestCase{
                                               /* .expr = */ "double(1)",
                                           },
                                           DefinitionsTestCase{
                                               /* .expr = */ "string(1)",
                                           },
                                           DefinitionsTestCase{
                                               /* .expr = */ "bool('true')",
                                           },
                                           DefinitionsTestCase{
                                               /* .expr = */ "timestamp(0)",
                                           },
                                           DefinitionsTestCase{
                                               /* .expr = */ "duration('1s')",
                                           },
                                           DefinitionsTestCase{
                                               /* .expr = */ "type(1)",
                                           }));

INSTANTIATE_TEST_SUITE_P(Arithmetic, StdLibDefinitionsTest,
                         ::testing::Values(DefinitionsTestCase{
                                               /* .expr = */ "1 + 2",
                                           },
                                           DefinitionsTestCase{
                                               /* .expr = */ "1 - 2",
                                           },
                                           DefinitionsTestCase{
                                               /* .expr = */ "1 / 2",
                                           },
                                           DefinitionsTestCase{
                                               /* .expr = */ "1 * 2",
                                           },
                                           DefinitionsTestCase{
                                               /* .expr = */ "2 % 1",
                                           },
                                           DefinitionsTestCase{
                                               /* .expr = */ "-1",
                                           }));

INSTANTIATE_TEST_SUITE_P(
    TimeArithmetic, StdLibDefinitionsTest,
    ::testing::Values(DefinitionsTestCase{
                          /* .expr = */ "timestamp(0) + duration('1s')",
                      },
                      DefinitionsTestCase{
                          /* .expr = */ "timestamp(0) - duration('1s')",
                      },
                      DefinitionsTestCase{
                          /* .expr = */ "timestamp(0) - timestamp(0)",
                      },
                      DefinitionsTestCase{
                          /* .expr = */ "duration('1s') + duration('1s')",
                      },
                      DefinitionsTestCase{
                          /* .expr = */ "duration('1s') - duration('1s')",
                      }));

INSTANTIATE_TEST_SUITE_P(NumericComparisons, StdLibDefinitionsTest,
                         ::testing::Values(DefinitionsTestCase{
                                               /* .expr = */ "1 > 2",
                                           },
                                           DefinitionsTestCase{
                                               /* .expr = */ "1 < 2",
                                           },
                                           DefinitionsTestCase{
                                               /* .expr = */ "1 >= 2",
                                           },
                                           DefinitionsTestCase{
                                               /* .expr = */ "1 <= 2",
                                           }));

INSTANTIATE_TEST_SUITE_P(
    CrossNumericComparisons, StdLibDefinitionsTest,
    ::testing::Values(
        DefinitionsTestCase{
            /* .expr = */ "1u < 2",
            /* .type_check_success = */ true,
            /* .options = */ {.enable_cross_numeric_comparisons = true}},
        DefinitionsTestCase{
            /* .expr = */ "1u > 2",
            /* .type_check_success = */ true,
            /* .options = */ {.enable_cross_numeric_comparisons = true}},
        DefinitionsTestCase{
            /* .expr = */ "1u <= 2",
            /* .type_check_success = */ true,
            /* .options = */ {.enable_cross_numeric_comparisons = true}},
        DefinitionsTestCase{
            /* .expr = */ "1u >= 2",
            /* .type_check_success = */ true,
            /* .options = */ {.enable_cross_numeric_comparisons = true}}));

INSTANTIATE_TEST_SUITE_P(
    TimeComparisons, StdLibDefinitionsTest,
    ::testing::Values(DefinitionsTestCase{
                          /* .expr = */ "duration('1s') < duration('1s')",
                      },
                      DefinitionsTestCase{
                          /* .expr = */ "duration('1s') > duration('1s')",
                      },
                      DefinitionsTestCase{
                          /* .expr = */ "duration('1s') <= duration('1s')",
                      },
                      DefinitionsTestCase{
                          /* .expr = */ "duration('1s') >= duration('1s')",
                      },
                      DefinitionsTestCase{
                          /* .expr = */ "timestamp(0) < timestamp(0)",
                      },
                      DefinitionsTestCase{
                          /* .expr = */ "timestamp(0) > timestamp(0)",
                      },
                      DefinitionsTestCase{
                          /* .expr = */ "timestamp(0) <= timestamp(0)",
                      },
                      DefinitionsTestCase{
                          /* .expr = */ "timestamp(0) >= timestamp(0)",
                      }));

INSTANTIATE_TEST_SUITE_P(
    TimeAccessors, StdLibDefinitionsTest,
    ::testing::Values(
        DefinitionsTestCase{
            /* .expr = */ "timestamp(0).getFullYear()",
        },
        DefinitionsTestCase{
            /* .expr = */ "timestamp(0).getFullYear('-08:00')",
        },
        DefinitionsTestCase{
            /* .expr = */ "timestamp(0).getMonth()",
        },
        DefinitionsTestCase{
            /* .expr = */ "timestamp(0).getMonth('-08:00')",
        },
        DefinitionsTestCase{
            /* .expr = */ "timestamp(0).getDayOfYear()",
        },
        DefinitionsTestCase{
            /* .expr = */ "timestamp(0).getDayOfYear('-08:00')",
        },
        DefinitionsTestCase{
            /* .expr = */ "timestamp(0).getDate()",
        },
        DefinitionsTestCase{
            /* .expr = */ "timestamp(0).getDate('-08:00')",
        },
        DefinitionsTestCase{
            /* .expr = */ "timestamp(0).getDayOfWeek()",
        },
        DefinitionsTestCase{
            /* .expr = */ "timestamp(0).getDayOfWeek('-08:00')",
        },
        DefinitionsTestCase{
            /* .expr = */ "timestamp(0).getHours()",
        },
        DefinitionsTestCase{
            /* .expr = */ "duration('1s').getHours()",
        },
        DefinitionsTestCase{
            /* .expr = */ "timestamp(0).getHours('-08:00')",
        },
        DefinitionsTestCase{
            /* .expr = */ "timestamp(0).getMinutes()",
        },
        DefinitionsTestCase{
            /* .expr = */ "duration('1s').getMinutes()",
        },
        DefinitionsTestCase{
            /* .expr = */ "timestamp(0).getMinutes('-08:00')",
        },
        DefinitionsTestCase{
            /* .expr = */ "timestamp(0).getSeconds()",
        },
        DefinitionsTestCase{
            /* .expr = */ "duration('1s').getSeconds()",
        },
        DefinitionsTestCase{
            /* .expr = */ "timestamp(0).getSeconds('-08:00')",
        },
        DefinitionsTestCase{
            /* .expr = */ "timestamp(0).getMilliseconds()",
        },
        DefinitionsTestCase{
            /* .expr = */ "duration('1s').getMilliseconds()",
        },
        DefinitionsTestCase{
            /* .expr = */ "timestamp(0).getMilliseconds('-08:00')",
        }));

INSTANTIATE_TEST_SUITE_P(Logic, StdLibDefinitionsTest,
                         ::testing::Values(DefinitionsTestCase{
                                               /* .expr = */ "true || false",
                                           },
                                           DefinitionsTestCase{
                                               /* .expr = */ "true && false",
                                           },
                                           DefinitionsTestCase{
                                               /* .expr = */ "!true",
                                           },
                                           DefinitionsTestCase{
                                               /* .expr = */ "true ? 1 : 2",
                                           }));

}  // namespace
}  // namespace cel
