// Copyright 2020 Google LLC
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

#include "eval/compiler/qualified_reference_resolver.h"

#include <memory>
#include <string>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "base/ast.h"
#include "base/ast_internal/ast_impl.h"
#include "base/ast_internal/expr.h"
#include "base/builtins.h"
#include "common/memory.h"
#include "common/type_factory.h"
#include "common/type_manager.h"
#include "common/values/legacy_value_manager.h"
#include "eval/compiler/resolver.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_function_registry.h"
#include "extensions/protobuf/ast_converters.h"
#include "internal/casts.h"
#include "internal/testing.h"
#include "runtime/internal/issue_collector.h"
#include "runtime/runtime_issue.h"
#include "runtime/type_registry.h"
#include "google/protobuf/text_format.h"

namespace google::api::expr::runtime {

namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::cel::Ast;
using ::cel::RuntimeIssue;
using ::cel::ast_internal::AstImpl;
using ::cel::ast_internal::Expr;
using ::cel::ast_internal::SourceInfo;
using ::cel::extensions::internal::ConvertProtoExprToNative;
using ::cel::runtime_internal::IssueCollector;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

// foo.bar.var1 && bar.foo.var2
constexpr char kExpr[] = R"(
  id: 1
  call_expr {
    function: "_&&_"
    args {
      id: 2
      select_expr {
        field: "var1"
        operand {
          id: 3
          select_expr {
            field: "bar"
            operand {
              id: 4
              ident_expr { name: "foo" }
            }
          }
        }
      }
    }
    args {
      id: 5
      select_expr {
        field: "var2"
        operand {
          id: 6
          select_expr {
            field: "foo"
            operand {
              id: 7
              ident_expr { name: "bar" }
            }
          }
        }
      }
    }
  }
)";

MATCHER_P(StatusCodeIs, x, "") {
  const absl::Status& status = arg;
  return status.code() == x;
}

std::unique_ptr<AstImpl> ParseTestProto(const std::string& pb) {
  google::api::expr::v1alpha1::Expr expr;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(pb, &expr));
  return absl::WrapUnique(cel::internal::down_cast<AstImpl*>(
      cel::extensions::CreateAstFromParsedExpr(expr).value().release()));
}

std::vector<absl::Status> ExtractIssuesStatus(const IssueCollector& issues) {
  std::vector<absl::Status> issues_status;
  for (const auto& issue : issues.issues()) {
    issues_status.push_back(issue.ToStatus());
  }
  return issues_status;
}

TEST(ResolveReferences, Basic) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(kExpr);
  expr_ast->reference_map()[2].set_name("foo.bar.var1");
  expr_ast->reference_map()[5].set_name("bar.foo.var2");
  IssueCollector issues(RuntimeIssue::Severity::kError);
  CelFunctionRegistry func_registry;
  cel::TypeRegistry type_registry;
  cel::common_internal::LegacyValueManager value_factory(
      cel::MemoryManagerRef::ReferenceCounting(),
      type_registry.GetComposedTypeProvider());
  Resolver registry("", func_registry.InternalGetRegistry(), type_registry,
                    value_factory, type_registry.resolveable_enums());

  auto result = ResolveReferences(registry, issues, *expr_ast);
  ASSERT_THAT(result, IsOkAndHolds(true));
  google::api::expr::v1alpha1::Expr expected_expr;
  google::protobuf::TextFormat::ParseFromString(R"pb(
                                        id: 1
                                        call_expr {
                                          function: "_&&_"
                                          args {
                                            id: 2
                                            ident_expr { name: "foo.bar.var1" }
                                          }
                                          args {
                                            id: 5
                                            ident_expr { name: "bar.foo.var2" }
                                          }
                                        })pb",
                                      &expected_expr);
  EXPECT_EQ(expr_ast->root_expr(),
            ConvertProtoExprToNative(expected_expr).value());
}

TEST(ResolveReferences, ReturnsFalseIfNoChanges) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(kExpr);
  IssueCollector issues(RuntimeIssue::Severity::kError);
  CelFunctionRegistry func_registry;
  cel::TypeRegistry type_registry;
  cel::common_internal::LegacyValueManager value_factory(
      cel::MemoryManagerRef::ReferenceCounting(),
      type_registry.GetComposedTypeProvider());
  Resolver registry("", func_registry.InternalGetRegistry(), type_registry,
                    value_factory, type_registry.resolveable_enums());

  auto result = ResolveReferences(registry, issues, *expr_ast);
  ASSERT_THAT(result, IsOkAndHolds(false));

  // reference to the same name also doesn't count as a rewrite.
  expr_ast->reference_map()[4].set_name("foo");
  expr_ast->reference_map()[7].set_name("bar");

  result = ResolveReferences(registry, issues, *expr_ast);
  ASSERT_THAT(result, IsOkAndHolds(false));
}

TEST(ResolveReferences, NamespacedIdent) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(kExpr);
  SourceInfo source_info;
  IssueCollector issues(RuntimeIssue::Severity::kError);
  CelFunctionRegistry func_registry;
  cel::TypeRegistry type_registry;
  cel::common_internal::LegacyValueManager value_factory(
      cel::MemoryManagerRef::ReferenceCounting(),
      type_registry.GetComposedTypeProvider());
  Resolver registry("", func_registry.InternalGetRegistry(), type_registry,
                    value_factory, type_registry.resolveable_enums());
  expr_ast->reference_map()[2].set_name("foo.bar.var1");
  expr_ast->reference_map()[7].set_name("namespace_x.bar");

  auto result = ResolveReferences(registry, issues, *expr_ast);
  ASSERT_THAT(result, IsOkAndHolds(true));
  google::api::expr::v1alpha1::Expr expected_expr;
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        id: 1
        call_expr {
          function: "_&&_"
          args {
            id: 2
            ident_expr { name: "foo.bar.var1" }
          }
          args {
            id: 5
            select_expr {
              field: "var2"
              operand {
                id: 6
                select_expr {
                  field: "foo"
                  operand {
                    id: 7
                    ident_expr { name: "namespace_x.bar" }
                  }
                }
              }
            }
          }
        })pb",
      &expected_expr);
  EXPECT_EQ(expr_ast->root_expr(),
            ConvertProtoExprToNative(expected_expr).value());
}

TEST(ResolveReferences, WarningOnPresenceTest) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(R"pb(
    id: 1
    select_expr {
      field: "var1"
      test_only: true
      operand {
        id: 2
        select_expr {
          field: "bar"
          operand {
            id: 3
            ident_expr { name: "foo" }
          }
        }
      }
    })pb");
  SourceInfo source_info;

  IssueCollector issues(RuntimeIssue::Severity::kError);
  CelFunctionRegistry func_registry;
  cel::TypeRegistry type_registry;
  cel::common_internal::LegacyValueManager value_factory(
      cel::MemoryManagerRef::ReferenceCounting(),
      type_registry.GetComposedTypeProvider());
  Resolver registry("", func_registry.InternalGetRegistry(), type_registry,
                    value_factory, type_registry.resolveable_enums());
  expr_ast->reference_map()[1].set_name("foo.bar.var1");

  auto result = ResolveReferences(registry, issues, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(false));
  EXPECT_THAT(
      ExtractIssuesStatus(issues),
      testing::ElementsAre(Eq(absl::Status(
          absl::StatusCode::kInvalidArgument,
          "Reference map points to a presence test -- has(container.attr)"))));
}

// foo.bar.var1 == bar.foo.Enum.ENUM_VAL1
constexpr char kEnumExpr[] = R"(
  id: 1
  call_expr {
    function: "_==_"
    args {
      id: 2
      select_expr {
        field: "var1"
        operand {
          id: 3
          select_expr {
            field: "bar"
            operand {
              id: 4
              ident_expr { name: "foo" }
            }
          }
        }
      }
    }
    args {
      id: 5
      ident_expr { name: "bar.foo.Enum.ENUM_VAL1" }
    }
  }
)";

TEST(ResolveReferences, EnumConstReferenceUsed) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(kEnumExpr);
  SourceInfo source_info;

  CelFunctionRegistry func_registry;
  ASSERT_OK(RegisterBuiltinFunctions(&func_registry));
  cel::TypeRegistry type_registry;
  cel::common_internal::LegacyValueManager value_factory(
      cel::MemoryManagerRef::ReferenceCounting(),
      type_registry.GetComposedTypeProvider());
  Resolver registry("", func_registry.InternalGetRegistry(), type_registry,
                    value_factory, type_registry.resolveable_enums());
  expr_ast->reference_map()[2].set_name("foo.bar.var1");
  expr_ast->reference_map()[5].set_name("bar.foo.Enum.ENUM_VAL1");
  expr_ast->reference_map()[5].mutable_value().set_int64_value(9);
  IssueCollector issues(RuntimeIssue::Severity::kError);

  auto result = ResolveReferences(registry, issues, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(true));
  google::api::expr::v1alpha1::Expr expected_expr;
  google::protobuf::TextFormat::ParseFromString(R"pb(
                                        id: 1
                                        call_expr {
                                          function: "_==_"
                                          args {
                                            id: 2
                                            ident_expr { name: "foo.bar.var1" }
                                          }
                                          args {
                                            id: 5
                                            const_expr { int64_value: 9 }
                                          }
                                        })pb",
                                      &expected_expr);
  EXPECT_EQ(expr_ast->root_expr(),
            ConvertProtoExprToNative(expected_expr).value());
}

TEST(ResolveReferences, EnumConstReferenceUsedSelect) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(kEnumExpr);
  SourceInfo source_info;

  CelFunctionRegistry func_registry;
  ASSERT_OK(RegisterBuiltinFunctions(&func_registry));
  cel::TypeRegistry type_registry;
  cel::common_internal::LegacyValueManager value_factory(
      cel::MemoryManagerRef::ReferenceCounting(),
      type_registry.GetComposedTypeProvider());
  Resolver registry("", func_registry.InternalGetRegistry(), type_registry,
                    value_factory, type_registry.resolveable_enums());
  expr_ast->reference_map()[2].set_name("foo.bar.var1");
  expr_ast->reference_map()[2].mutable_value().set_int64_value(2);
  expr_ast->reference_map()[5].set_name("bar.foo.Enum.ENUM_VAL1");
  expr_ast->reference_map()[5].mutable_value().set_int64_value(9);
  IssueCollector issues(RuntimeIssue::Severity::kError);

  auto result = ResolveReferences(registry, issues, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(true));
  google::api::expr::v1alpha1::Expr expected_expr;
  google::protobuf::TextFormat::ParseFromString(R"pb(
                                        id: 1
                                        call_expr {
                                          function: "_==_"
                                          args {
                                            id: 2
                                            const_expr { int64_value: 2 }
                                          }
                                          args {
                                            id: 5
                                            const_expr { int64_value: 9 }
                                          }
                                        })pb",
                                      &expected_expr);
  EXPECT_EQ(expr_ast->root_expr(),
            ConvertProtoExprToNative(expected_expr).value());
}

TEST(ResolveReferences, ConstReferenceSkipped) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(kExpr);
  SourceInfo source_info;

  CelFunctionRegistry func_registry;
  ASSERT_OK(RegisterBuiltinFunctions(&func_registry));
  cel::TypeRegistry type_registry;
  cel::common_internal::LegacyValueManager value_factory(
      cel::MemoryManagerRef::ReferenceCounting(),
      type_registry.GetComposedTypeProvider());
  Resolver registry("", func_registry.InternalGetRegistry(), type_registry,
                    value_factory, type_registry.resolveable_enums());
  expr_ast->reference_map()[2].set_name("foo.bar.var1");
  expr_ast->reference_map()[2].mutable_value().set_bool_value(true);
  expr_ast->reference_map()[5].set_name("bar.foo.var2");
  IssueCollector issues(RuntimeIssue::Severity::kError);

  auto result = ResolveReferences(registry, issues, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(true));
  google::api::expr::v1alpha1::Expr expected_expr;
  google::protobuf::TextFormat::ParseFromString(R"pb(
                                        id: 1
                                        call_expr {
                                          function: "_&&_"
                                          args {
                                            id: 2
                                            select_expr {
                                              field: "var1"
                                              operand {
                                                id: 3
                                                select_expr {
                                                  field: "bar"
                                                  operand {
                                                    id: 4
                                                    ident_expr { name: "foo" }
                                                  }
                                                }
                                              }
                                            }
                                          }
                                          args {
                                            id: 5
                                            ident_expr { name: "bar.foo.var2" }
                                          }
                                        })pb",
                                      &expected_expr);
  EXPECT_EQ(expr_ast->root_expr(),
            ConvertProtoExprToNative(expected_expr).value());
}

constexpr char kExtensionAndExpr[] = R"(
id: 1
call_expr {
  function: "boolean_and"
  args {
    id: 2
    const_expr {
      bool_value: true
    }
  }
  args {
    id: 3
    const_expr {
      bool_value: false
    }
  }
})";

TEST(ResolveReferences, FunctionReferenceBasic) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(kExtensionAndExpr);
  SourceInfo source_info;

  CelFunctionRegistry func_registry;
  ASSERT_OK(func_registry.RegisterLazyFunction(
      CelFunctionDescriptor("boolean_and", false,
                            {
                                CelValue::Type::kBool,
                                CelValue::Type::kBool,
                            })));
  cel::TypeRegistry type_registry;
  cel::common_internal::LegacyValueManager value_factory(
      cel::MemoryManagerRef::ReferenceCounting(),
      type_registry.GetComposedTypeProvider());
  Resolver registry("", func_registry.InternalGetRegistry(), type_registry,
                    value_factory, type_registry.resolveable_enums());
  IssueCollector issues(RuntimeIssue::Severity::kError);
  expr_ast->reference_map()[1].mutable_overload_id().push_back(
      "udf_boolean_and");

  auto result = ResolveReferences(registry, issues, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(false));
}

TEST(ResolveReferences, FunctionReferenceMissingOverloadDetected) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(kExtensionAndExpr);
  SourceInfo source_info;

  CelFunctionRegistry func_registry;
  cel::TypeRegistry type_registry;
  cel::common_internal::LegacyValueManager value_factory(
      cel::MemoryManagerRef::ReferenceCounting(),
      type_registry.GetComposedTypeProvider());
  Resolver registry("", func_registry.InternalGetRegistry(), type_registry,
                    value_factory, type_registry.resolveable_enums());
  IssueCollector issues(RuntimeIssue::Severity::kError);
  expr_ast->reference_map()[1].mutable_overload_id().push_back(
      "udf_boolean_and");

  auto result = ResolveReferences(registry, issues, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(false));
  EXPECT_THAT(ExtractIssuesStatus(issues),
              ElementsAre(StatusCodeIs(absl::StatusCode::kInvalidArgument)));
}

TEST(ResolveReferences, SpecialBuiltinsNotWarned) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(R"pb(
    id: 1
    call_expr {
      function: "*"
      args {
        id: 2
        const_expr { bool_value: true }
      }
      args {
        id: 3
        const_expr { bool_value: false }
      }
    })pb");
  SourceInfo source_info;

  std::vector<const char*> special_builtins{
      cel::builtin::kAnd, cel::builtin::kOr, cel::builtin::kTernary,
      cel::builtin::kIndex};
  for (const char* builtin_fn : special_builtins) {
    // Builtins aren't in the function registry.
    CelFunctionRegistry func_registry;
    cel::TypeRegistry type_registry;
    cel::common_internal::LegacyValueManager value_factory(
        cel::MemoryManagerRef::ReferenceCounting(),
        type_registry.GetComposedTypeProvider());
    Resolver registry("", func_registry.InternalGetRegistry(), type_registry,
                      value_factory, type_registry.resolveable_enums());
    IssueCollector issues(RuntimeIssue::Severity::kError);
    expr_ast->reference_map()[1].mutable_overload_id().push_back(
        absl::StrCat("builtin.", builtin_fn));
    expr_ast->root_expr().mutable_call_expr().set_function(builtin_fn);

    auto result = ResolveReferences(registry, issues, *expr_ast);

    ASSERT_THAT(result, IsOkAndHolds(false));
    EXPECT_THAT(ExtractIssuesStatus(issues), IsEmpty());
  }
}

TEST(ResolveReferences,
     FunctionReferenceMissingOverloadDetectedAndMissingReference) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(kExtensionAndExpr);
  SourceInfo source_info;

  CelFunctionRegistry func_registry;
  cel::TypeRegistry type_registry;
  cel::common_internal::LegacyValueManager value_factory(
      cel::MemoryManagerRef::ReferenceCounting(),
      type_registry.GetComposedTypeProvider());
  Resolver registry("", func_registry.InternalGetRegistry(), type_registry,
                    value_factory, type_registry.resolveable_enums());
  IssueCollector issues(RuntimeIssue::Severity::kError);
  expr_ast->reference_map()[1].set_name("udf_boolean_and");

  auto result = ResolveReferences(registry, issues, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(false));
  EXPECT_THAT(
      ExtractIssuesStatus(issues),
      UnorderedElementsAre(
          Eq(absl::InvalidArgumentError(
              "No overload found in reference resolve step for boolean_and")),
          Eq(absl::InvalidArgumentError(
              "Reference map doesn't provide overloads for boolean_and"))));
}

TEST(ResolveReferences, EmulatesEagerFailing) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(kExtensionAndExpr);
  SourceInfo source_info;

  CelFunctionRegistry func_registry;
  cel::TypeRegistry type_registry;
  cel::common_internal::LegacyValueManager value_factory(
      cel::MemoryManagerRef::ReferenceCounting(),
      type_registry.GetComposedTypeProvider());
  Resolver registry("", func_registry.InternalGetRegistry(), type_registry,
                    value_factory, type_registry.resolveable_enums());
  IssueCollector issues(RuntimeIssue::Severity::kWarning);
  expr_ast->reference_map()[1].set_name("udf_boolean_and");

  EXPECT_THAT(
      ResolveReferences(registry, issues, *expr_ast),
      StatusIs(absl::StatusCode::kInvalidArgument,
               "Reference map doesn't provide overloads for boolean_and"));
}

TEST(ResolveReferences, FunctionReferenceToWrongExprKind) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(kExtensionAndExpr);
  SourceInfo source_info;

  IssueCollector issues(RuntimeIssue::Severity::kError);
  CelFunctionRegistry func_registry;
  cel::TypeRegistry type_registry;
  cel::common_internal::LegacyValueManager value_factory(
      cel::MemoryManagerRef::ReferenceCounting(),
      type_registry.GetComposedTypeProvider());
  Resolver registry("", func_registry.InternalGetRegistry(), type_registry,
                    value_factory, type_registry.resolveable_enums());
  expr_ast->reference_map()[2].mutable_overload_id().push_back(
      "udf_boolean_and");

  auto result = ResolveReferences(registry, issues, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(false));
  EXPECT_THAT(ExtractIssuesStatus(issues),
              ElementsAre(StatusCodeIs(absl::StatusCode::kInvalidArgument)));
}

constexpr char kReceiverCallExtensionAndExpr[] = R"(
id: 1
call_expr {
  function: "boolean_and"
  target {
    id: 2
    ident_expr {
      name: "ext"
    }
  }
  args {
    id: 3
    const_expr {
      bool_value: false
    }
  }
})";

TEST(ResolveReferences, FunctionReferenceWithTargetNoChange) {
  std::unique_ptr<AstImpl> expr_ast =
      ParseTestProto(kReceiverCallExtensionAndExpr);
  SourceInfo source_info;

  IssueCollector issues(RuntimeIssue::Severity::kError);
  CelFunctionRegistry func_registry;
  ASSERT_OK(func_registry.RegisterLazyFunction(CelFunctionDescriptor(
      "boolean_and", true, {CelValue::Type::kBool, CelValue::Type::kBool})));
  cel::TypeRegistry type_registry;
  cel::common_internal::LegacyValueManager value_factory(
      cel::MemoryManagerRef::ReferenceCounting(),
      type_registry.GetComposedTypeProvider());
  Resolver registry("", func_registry.InternalGetRegistry(), type_registry,
                    value_factory, type_registry.resolveable_enums());
  expr_ast->reference_map()[1].mutable_overload_id().push_back(
      "udf_boolean_and");

  auto result = ResolveReferences(registry, issues, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(false));
  EXPECT_THAT(ExtractIssuesStatus(issues), IsEmpty());
}

TEST(ResolveReferences,
     FunctionReferenceWithTargetNoChangeMissingOverloadDetected) {
  std::unique_ptr<AstImpl> expr_ast =
      ParseTestProto(kReceiverCallExtensionAndExpr);
  SourceInfo source_info;

  IssueCollector issues(RuntimeIssue::Severity::kError);
  CelFunctionRegistry func_registry;
  cel::TypeRegistry type_registry;
  cel::common_internal::LegacyValueManager value_factory(
      cel::MemoryManagerRef::ReferenceCounting(),
      type_registry.GetComposedTypeProvider());
  Resolver registry("", func_registry.InternalGetRegistry(), type_registry,
                    value_factory, type_registry.resolveable_enums());
  expr_ast->reference_map()[1].mutable_overload_id().push_back(
      "udf_boolean_and");

  auto result = ResolveReferences(registry, issues, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(false));
  EXPECT_THAT(ExtractIssuesStatus(issues),
              ElementsAre(StatusCodeIs(absl::StatusCode::kInvalidArgument)));
}

TEST(ResolveReferences, FunctionReferenceWithTargetToNamespacedFunction) {
  std::unique_ptr<AstImpl> expr_ast =
      ParseTestProto(kReceiverCallExtensionAndExpr);
  SourceInfo source_info;

  IssueCollector issues(RuntimeIssue::Severity::kError);
  CelFunctionRegistry func_registry;
  ASSERT_OK(func_registry.RegisterLazyFunction(CelFunctionDescriptor(
      "ext.boolean_and", false, {CelValue::Type::kBool})));
  cel::TypeRegistry type_registry;
  cel::common_internal::LegacyValueManager value_factory(
      cel::MemoryManagerRef::ReferenceCounting(),
      type_registry.GetComposedTypeProvider());
  Resolver registry("", func_registry.InternalGetRegistry(), type_registry,
                    value_factory, type_registry.resolveable_enums());
  expr_ast->reference_map()[1].mutable_overload_id().push_back(
      "udf_boolean_and");

  auto result = ResolveReferences(registry, issues, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(true));
  google::api::expr::v1alpha1::Expr expected_expr;
  google::protobuf::TextFormat::ParseFromString(R"pb(
                                        id: 1
                                        call_expr {
                                          function: "ext.boolean_and"
                                          args {
                                            id: 3
                                            const_expr { bool_value: false }
                                          }
                                        }
                                      )pb",
                                      &expected_expr);
  EXPECT_EQ(expr_ast->root_expr(),
            ConvertProtoExprToNative(expected_expr).value());
  EXPECT_THAT(ExtractIssuesStatus(issues), IsEmpty());
}

TEST(ResolveReferences,
     FunctionReferenceWithTargetToNamespacedFunctionInContainer) {
  std::unique_ptr<AstImpl> expr_ast =
      ParseTestProto(kReceiverCallExtensionAndExpr);
  SourceInfo source_info;

  expr_ast->reference_map()[1].mutable_overload_id().push_back(
      "udf_boolean_and");
  IssueCollector issues(RuntimeIssue::Severity::kError);
  CelFunctionRegistry func_registry;
  ASSERT_OK(func_registry.RegisterLazyFunction(CelFunctionDescriptor(
      "com.google.ext.boolean_and", false, {CelValue::Type::kBool})));
  cel::TypeRegistry type_registry;
  cel::common_internal::LegacyValueManager value_factory(
      cel::MemoryManagerRef::ReferenceCounting(),
      type_registry.GetComposedTypeProvider());
  Resolver registry("com.google", func_registry.InternalGetRegistry(),
                    type_registry, value_factory,
                    type_registry.resolveable_enums());
  auto result = ResolveReferences(registry, issues, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(true));
  google::api::expr::v1alpha1::Expr expected_expr;
  google::protobuf::TextFormat::ParseFromString(R"pb(
                                        id: 1
                                        call_expr {
                                          function: "com.google.ext.boolean_and"
                                          args {
                                            id: 3
                                            const_expr { bool_value: false }
                                          }
                                        }
                                      )pb",
                                      &expected_expr);
  EXPECT_EQ(expr_ast->root_expr(),
            ConvertProtoExprToNative(expected_expr).value());
  EXPECT_THAT(ExtractIssuesStatus(issues), IsEmpty());
}

// has(ext.option).boolean_and(false)
constexpr char kReceiverCallHasExtensionAndExpr[] = R"(
id: 1
call_expr {
  function: "boolean_and"
  target {
    id: 2
    select_expr {
      test_only: true
      field: "option"
      operand {
        id: 3
        ident_expr {
          name: "ext"
        }
      }
    }
  }
  args {
    id: 4
    const_expr {
      bool_value: false
    }
  }
})";

TEST(ResolveReferences, FunctionReferenceWithHasTargetNoChange) {
  std::unique_ptr<AstImpl> expr_ast =
      ParseTestProto(kReceiverCallHasExtensionAndExpr);
  SourceInfo source_info;

  IssueCollector issues(RuntimeIssue::Severity::kError);
  CelFunctionRegistry func_registry;
  ASSERT_OK(func_registry.RegisterLazyFunction(CelFunctionDescriptor(
      "boolean_and", true, {CelValue::Type::kBool, CelValue::Type::kBool})));
  ASSERT_OK(func_registry.RegisterLazyFunction(CelFunctionDescriptor(
      "ext.option.boolean_and", true, {CelValue::Type::kBool})));
  cel::TypeRegistry type_registry;
  cel::common_internal::LegacyValueManager value_factory(
      cel::MemoryManagerRef::ReferenceCounting(),
      type_registry.GetComposedTypeProvider());
  Resolver registry("", func_registry.InternalGetRegistry(), type_registry,
                    value_factory, type_registry.resolveable_enums());
  expr_ast->reference_map()[1].mutable_overload_id().push_back(
      "udf_boolean_and");

  auto result = ResolveReferences(registry, issues, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(false));
  // The target is unchanged because it is a test_only select.
  google::api::expr::v1alpha1::Expr expected_expr;
  google::protobuf::TextFormat::ParseFromString(kReceiverCallHasExtensionAndExpr,
                                      &expected_expr);
  EXPECT_EQ(expr_ast->root_expr(),
            ConvertProtoExprToNative(expected_expr).value());
  EXPECT_THAT(ExtractIssuesStatus(issues), IsEmpty());
}

constexpr char kComprehensionExpr[] = R"(
id:17
comprehension_expr: {
  iter_var:"i"
  iter_range:{
    id:1
    list_expr:{
      elements:{
        id:2
        const_expr:{int64_value:1}
      }
      elements:{
        id:3
        ident_expr:{name:"ENUM"}
      }
      elements:{
        id:4
        const_expr:{int64_value:3}
      }
    }
  }
  accu_var:"__result__"
  accu_init: {
    id:10
    const_expr:{bool_value:false}
  }
  loop_condition:{
    id:13
    call_expr:{
      function:"@not_strictly_false"
      args:{
        id:12
        call_expr:{
          function:"!_"
          args:{
            id:11
            ident_expr:{name:"__result__"}
          }
        }
      }
    }
  }
  loop_step:{
    id:15
    call_expr: {
      function:"_||_"
      args:{
        id:14
        ident_expr: {name:"__result__"}
      }
      args:{
        id:8
        call_expr:{
          function:"_==_"
          args:{
            id:7 ident_expr:{name:"ENUM"}
          }
          args:{
            id:9 ident_expr:{name:"i"}
          }
        }
      }
    }
  }
  result:{id:16 ident_expr:{name:"__result__"}}
}
)";
TEST(ResolveReferences, EnumConstReferenceUsedInComprehension) {
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(kComprehensionExpr);

  SourceInfo source_info;

  CelFunctionRegistry func_registry;
  ASSERT_OK(RegisterBuiltinFunctions(&func_registry));
  cel::TypeRegistry type_registry;
  cel::common_internal::LegacyValueManager value_factory(
      cel::MemoryManagerRef::ReferenceCounting(),
      type_registry.GetComposedTypeProvider());
  Resolver registry("", func_registry.InternalGetRegistry(), type_registry,
                    value_factory, type_registry.resolveable_enums());
  expr_ast->reference_map()[3].set_name("ENUM");
  expr_ast->reference_map()[3].mutable_value().set_int64_value(2);
  expr_ast->reference_map()[7].set_name("ENUM");
  expr_ast->reference_map()[7].mutable_value().set_int64_value(2);
  IssueCollector issues(RuntimeIssue::Severity::kError);

  auto result = ResolveReferences(registry, issues, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(true));
  google::api::expr::v1alpha1::Expr expected_expr;
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        id: 17
        comprehension_expr {
          iter_var: "i"
          iter_range {
            id: 1
            list_expr {
              elements {
                id: 2
                const_expr { int64_value: 1 }
              }
              elements {
                id: 3
                const_expr { int64_value: 2 }
              }
              elements {
                id: 4
                const_expr { int64_value: 3 }
              }
            }
          }
          accu_var: "__result__"
          accu_init {
            id: 10
            const_expr { bool_value: false }
          }
          loop_condition {
            id: 13
            call_expr {
              function: "@not_strictly_false"
              args {
                id: 12
                call_expr {
                  function: "!_"
                  args {
                    id: 11
                    ident_expr { name: "__result__" }
                  }
                }
              }
            }
          }
          loop_step {
            id: 15
            call_expr {
              function: "_||_"
              args {
                id: 14
                ident_expr { name: "__result__" }
              }
              args {
                id: 8
                call_expr {
                  function: "_==_"
                  args {
                    id: 7
                    const_expr { int64_value: 2 }
                  }
                  args {
                    id: 9
                    ident_expr { name: "i" }
                  }
                }
              }
            }
          }
          result {
            id: 16
            ident_expr { name: "__result__" }
          }
        })pb",
      &expected_expr);
  EXPECT_EQ(expr_ast->root_expr(),
            ConvertProtoExprToNative(expected_expr).value());
}

TEST(ResolveReferences, ReferenceToId0Warns) {
  // ID 0 is unsupported since it is not normally used by parsers and is
  // ambiguous as an intentional ID or default for unset field.
  std::unique_ptr<AstImpl> expr_ast = ParseTestProto(R"pb(
    id: 0
    select_expr {
      operand {
        id: 1
        ident_expr { name: "pkg" }
      }
      field: "var"
    })pb");

  SourceInfo source_info;

  CelFunctionRegistry func_registry;
  ASSERT_OK(RegisterBuiltinFunctions(&func_registry));
  cel::TypeRegistry type_registry;
  cel::common_internal::LegacyValueManager value_factory(
      cel::MemoryManagerRef::ReferenceCounting(),
      type_registry.GetComposedTypeProvider());
  Resolver registry("", func_registry.InternalGetRegistry(), type_registry,
                    value_factory, type_registry.resolveable_enums());
  expr_ast->reference_map()[0].set_name("pkg.var");
  IssueCollector issues(RuntimeIssue::Severity::kError);

  auto result = ResolveReferences(registry, issues, *expr_ast);

  ASSERT_THAT(result, IsOkAndHolds(false));
  google::api::expr::v1alpha1::Expr expected_expr;
  google::protobuf::TextFormat::ParseFromString(R"pb(
                                        id: 0
                                        select_expr {
                                          operand {
                                            id: 1
                                            ident_expr { name: "pkg" }
                                          }
                                          field: "var"
                                        })pb",
                                      &expected_expr);
  EXPECT_EQ(expr_ast->root_expr(),
            ConvertProtoExprToNative(expected_expr).value());
  EXPECT_THAT(
      ExtractIssuesStatus(issues),
      Contains(StatusIs(
          absl::StatusCode::kInvalidArgument,
          "reference map entries for expression id 0 are not supported")));
}
}  // namespace

}  // namespace google::api::expr::runtime
