// Copyright 2022 Google LLC
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

#include "extensions/protobuf/ast_converters.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "base/ast_internal/ast_impl.h"
#include "base/ast_internal/expr.h"
#include "internal/proto_matchers.h"
#include "internal/testing.h"
#include "parser/options.h"
#include "parser/parser.h"
#include "google/protobuf/text_format.h"

namespace cel::extensions {
namespace internal {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::cel::ast_internal::NullValue;
using ::cel::ast_internal::PrimitiveType;
using ::cel::ast_internal::WellKnownType;

TEST(AstConvertersTest, SourceInfoToNative) {
  google::api::expr::v1alpha1::SourceInfo source_info;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        syntax_version: "version"
        location: "location"
        line_offsets: 1
        line_offsets: 2
        positions { key: 1 value: 2 }
        positions { key: 3 value: 4 }
        macro_calls {
          key: 1
          value { ident_expr { name: "name" } }
        }
      )pb",
      &source_info));

  auto native_source_info = ConvertProtoSourceInfoToNative(source_info);

  EXPECT_EQ(native_source_info->syntax_version(), "version");
  EXPECT_EQ(native_source_info->location(), "location");
  EXPECT_EQ(native_source_info->line_offsets(), std::vector<int32_t>({1, 2}));
  EXPECT_EQ(native_source_info->positions().at(1), 2);
  EXPECT_EQ(native_source_info->positions().at(3), 4);
  ASSERT_TRUE(native_source_info->macro_calls().at(1).has_ident_expr());
  ASSERT_EQ(native_source_info->macro_calls().at(1).ident_expr().name(),
            "name");
}

TEST(AstConvertersTest, PrimitiveTypeUnspecifiedToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_primitive(google::api::expr::v1alpha1::Type::PRIMITIVE_TYPE_UNSPECIFIED);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_primitive());
  EXPECT_EQ(native_type->primitive(), PrimitiveType::kPrimitiveTypeUnspecified);
}

TEST(AstConvertersTest, PrimitiveTypeBoolToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_primitive(google::api::expr::v1alpha1::Type::BOOL);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_primitive());
  EXPECT_EQ(native_type->primitive(), PrimitiveType::kBool);
}

TEST(AstConvertersTest, PrimitiveTypeInt64ToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_primitive(google::api::expr::v1alpha1::Type::INT64);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_primitive());
  EXPECT_EQ(native_type->primitive(), PrimitiveType::kInt64);
}

TEST(AstConvertersTest, PrimitiveTypeUint64ToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_primitive(google::api::expr::v1alpha1::Type::UINT64);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_primitive());
  EXPECT_EQ(native_type->primitive(), PrimitiveType::kUint64);
}

TEST(AstConvertersTest, PrimitiveTypeDoubleToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_primitive(google::api::expr::v1alpha1::Type::DOUBLE);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_primitive());
  EXPECT_EQ(native_type->primitive(), PrimitiveType::kDouble);
}

TEST(AstConvertersTest, PrimitiveTypeStringToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_primitive(google::api::expr::v1alpha1::Type::STRING);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_primitive());
  EXPECT_EQ(native_type->primitive(), PrimitiveType::kString);
}

TEST(AstConvertersTest, PrimitiveTypeBytesToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_primitive(google::api::expr::v1alpha1::Type::BYTES);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_primitive());
  EXPECT_EQ(native_type->primitive(), PrimitiveType::kBytes);
}

TEST(AstConvertersTest, PrimitiveTypeError) {
  google::api::expr::v1alpha1::Type type;
  type.set_primitive(::google::api::expr::v1alpha1::Type_PrimitiveType(7));

  auto native_type = ConvertProtoTypeToNative(type);

  EXPECT_EQ(native_type.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(native_type.status().message(),
              ::testing::HasSubstr("Illegal type specified for "
                                   "google::api::expr::v1alpha1::Type::PrimitiveType."));
}

TEST(AstConvertersTest, WellKnownTypeUnspecifiedToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_well_known(google::api::expr::v1alpha1::Type::WELL_KNOWN_TYPE_UNSPECIFIED);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_well_known());
  EXPECT_EQ(native_type->well_known(),
            WellKnownType::kWellKnownTypeUnspecified);
}

TEST(AstConvertersTest, WellKnownTypeAnyToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_well_known(google::api::expr::v1alpha1::Type::ANY);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_well_known());
  EXPECT_EQ(native_type->well_known(), WellKnownType::kAny);
}

TEST(AstConvertersTest, WellKnownTypeTimestampToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_well_known(google::api::expr::v1alpha1::Type::TIMESTAMP);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_well_known());
  EXPECT_EQ(native_type->well_known(), WellKnownType::kTimestamp);
}

TEST(AstConvertersTest, WellKnownTypeDuraionToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_well_known(google::api::expr::v1alpha1::Type::DURATION);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_well_known());
  EXPECT_EQ(native_type->well_known(), WellKnownType::kDuration);
}

TEST(AstConvertersTest, WellKnownTypeError) {
  google::api::expr::v1alpha1::Type type;
  type.set_well_known(::google::api::expr::v1alpha1::Type_WellKnownType(4));

  auto native_type = ConvertProtoTypeToNative(type);

  EXPECT_EQ(native_type.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(native_type.status().message(),
              ::testing::HasSubstr("Illegal type specified for "
                                   "google::api::expr::v1alpha1::Type::WellKnownType."));
}

TEST(AstConvertersTest, ListTypeToNative) {
  google::api::expr::v1alpha1::Type type;
  type.mutable_list_type()->mutable_elem_type()->set_primitive(
      google::api::expr::v1alpha1::Type::BOOL);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_list_type());
  auto& native_list_type = native_type->list_type();
  ASSERT_TRUE(native_list_type.elem_type().has_primitive());
  EXPECT_EQ(native_list_type.elem_type().primitive(), PrimitiveType::kBool);
}

TEST(AstConvertersTest, MapTypeToNative) {
  google::api::expr::v1alpha1::Type type;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        map_type {
          key_type { primitive: BOOL }
          value_type { primitive: DOUBLE }
        }
      )pb",
      &type));

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_map_type());
  auto& native_map_type = native_type->map_type();
  ASSERT_TRUE(native_map_type.key_type().has_primitive());
  EXPECT_EQ(native_map_type.key_type().primitive(), PrimitiveType::kBool);
  ASSERT_TRUE(native_map_type.value_type().has_primitive());
  EXPECT_EQ(native_map_type.value_type().primitive(), PrimitiveType::kDouble);
}

TEST(AstConvertersTest, FunctionTypeToNative) {
  google::api::expr::v1alpha1::Type type;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        function {
          result_type { primitive: BOOL }
          arg_types { primitive: DOUBLE }
          arg_types { primitive: STRING }
        }
      )pb",
      &type));

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_function());
  auto& native_function_type = native_type->function();
  ASSERT_TRUE(native_function_type.result_type().has_primitive());
  EXPECT_EQ(native_function_type.result_type().primitive(),
            PrimitiveType::kBool);
  ASSERT_TRUE(native_function_type.arg_types().at(0).has_primitive());
  EXPECT_EQ(native_function_type.arg_types().at(0).primitive(),
            PrimitiveType::kDouble);
  ASSERT_TRUE(native_function_type.arg_types().at(1).has_primitive());
  EXPECT_EQ(native_function_type.arg_types().at(1).primitive(),
            PrimitiveType::kString);
}

TEST(AstConvertersTest, AbstractTypeToNative) {
  google::api::expr::v1alpha1::Type type;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        abstract_type {
          name: "name"
          parameter_types { primitive: DOUBLE }
          parameter_types { primitive: STRING }
        }
      )pb",
      &type));

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_abstract_type());
  auto& native_abstract_type = native_type->abstract_type();
  EXPECT_EQ(native_abstract_type.name(), "name");
  ASSERT_TRUE(native_abstract_type.parameter_types().at(0).has_primitive());
  EXPECT_EQ(native_abstract_type.parameter_types().at(0).primitive(),
            PrimitiveType::kDouble);
  ASSERT_TRUE(native_abstract_type.parameter_types().at(1).has_primitive());
  EXPECT_EQ(native_abstract_type.parameter_types().at(1).primitive(),
            PrimitiveType::kString);
}

TEST(AstConvertersTest, DynamicTypeToNative) {
  google::api::expr::v1alpha1::Type type;
  type.mutable_dyn();

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_dyn());
}

TEST(AstConvertersTest, NullTypeToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_null(google::protobuf::NULL_VALUE);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_null());
  EXPECT_EQ(native_type->null(), nullptr);
}

TEST(AstConvertersTest, PrimitiveTypeWrapperToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_wrapper(google::api::expr::v1alpha1::Type::BOOL);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_wrapper());
  EXPECT_EQ(native_type->wrapper(), PrimitiveType::kBool);
}

TEST(AstConvertersTest, MessageTypeToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_message_type("message");

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_message_type());
  EXPECT_EQ(native_type->message_type().type(), "message");
}

TEST(AstConvertersTest, ParamTypeToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_type_param("param");

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_type_param());
  EXPECT_EQ(native_type->type_param().type(), "param");
}

TEST(AstConvertersTest, NestedTypeToNative) {
  google::api::expr::v1alpha1::Type type;
  type.mutable_type()->mutable_dyn();

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_type());
  EXPECT_TRUE(native_type->type().has_dyn());
}

TEST(AstConvertersTest, TypeTypeDefault) {
  auto native_type = ConvertProtoTypeToNative(google::api::expr::v1alpha1::Type());

  ASSERT_THAT(native_type, IsOk());
  EXPECT_TRUE(absl::holds_alternative<ast_internal::UnspecifiedType>(
      native_type->type_kind()));
}

TEST(AstConvertersTest, ReferenceToNative) {
  google::api::expr::v1alpha1::Reference reference;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "name"
        overload_id: "id1"
        overload_id: "id2"
        value { bool_value: true }
      )pb",
      &reference));

  auto native_reference = ConvertProtoReferenceToNative(reference);

  EXPECT_EQ(native_reference->name(), "name");
  EXPECT_EQ(native_reference->overload_id(),
            std::vector<std::string>({"id1", "id2"}));
  EXPECT_TRUE(native_reference->value().bool_value());
}

}  // namespace
}  // namespace internal

namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::cel::internal::test::EqualsProto;
using ::google::api::expr::parser::Parse;
using ::testing::HasSubstr;

using ParsedExprPb = google::api::expr::v1alpha1::ParsedExpr;
using CheckedExprPb = google::api::expr::v1alpha1::CheckedExpr;
using TypePb = google::api::expr::v1alpha1::Type;

TEST(AstConvertersTest, CheckedExprToAst) {
  CheckedExprPb checked_expr;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        reference_map {
          key: 1
          value {
            name: "name"
            overload_id: "id1"
            overload_id: "id2"
            value { bool_value: true }
          }
        }
        type_map {
          key: 1
          value { dyn {} }
        }
        source_info {
          syntax_version: "version"
          location: "location"
          line_offsets: 1
          line_offsets: 2
          positions { key: 1 value: 2 }
          positions { key: 3 value: 4 }
          macro_calls {
            key: 1
            value { ident_expr { name: "name" } }
          }
        }
        expr_version: "version"
        expr { ident_expr { name: "expr" } }
      )pb",
      &checked_expr));

  ASSERT_OK_AND_ASSIGN(auto ast, CreateAstFromCheckedExpr(checked_expr));

  ASSERT_TRUE(ast->IsChecked());
}

TEST(AstConvertersTest, AstToCheckedExprBasic) {
  ast_internal::AstImpl ast;
  ast.root_expr().set_id(1);
  ast.root_expr().mutable_ident_expr().set_name("expr");

  ast.source_info().set_syntax_version("version");
  ast.source_info().set_location("location");
  ast.source_info().mutable_line_offsets().push_back(1);
  ast.source_info().mutable_line_offsets().push_back(2);
  ast.source_info().mutable_positions().insert({1, 2});
  ast.source_info().mutable_positions().insert({3, 4});

  ast_internal::Expr macro;
  macro.mutable_ident_expr().set_name("name");
  ast.source_info().mutable_macro_calls().insert({1, std::move(macro)});

  ast_internal::AstImpl::TypeMap type_map;
  ast_internal::AstImpl::ReferenceMap reference_map;

  ast_internal::Reference reference;
  reference.set_name("name");
  reference.mutable_overload_id().push_back("id1");
  reference.mutable_overload_id().push_back("id2");
  reference.mutable_value().set_bool_value(true);

  ast_internal::Type type;
  type.set_type_kind(ast_internal::DynamicType());

  ast.reference_map().insert({1, std::move(reference)});
  ast.type_map().insert({1, std::move(type)});

  ast.set_expr_version("version");
  ast.set_is_checked(true);

  ASSERT_OK_AND_ASSIGN(auto checked_pb, CreateCheckedExprFromAst(ast));

  EXPECT_THAT(checked_pb, EqualsProto(R"pb(
                reference_map {
                  key: 1
                  value {
                    name: "name"
                    overload_id: "id1"
                    overload_id: "id2"
                    value { bool_value: true }
                  }
                }
                type_map {
                  key: 1
                  value { dyn {} }
                }
                source_info {
                  syntax_version: "version"
                  location: "location"
                  line_offsets: 1
                  line_offsets: 2
                  positions { key: 1 value: 2 }
                  positions { key: 3 value: 4 }
                  macro_calls {
                    key: 1
                    value { ident_expr { name: "name" } }
                  }
                }
                expr_version: "version"
                expr {
                  id: 1
                  ident_expr { name: "expr" }
                }
              )pb"));
}

constexpr absl::string_view kTypesTestCheckedExpr =
    R"pb(reference_map: {
           key: 1
           value: { name: "x" }
         }
         type_map: {
           key: 1
           value: { primitive: INT64 }
         }
         source_info: {
           location: "<input>"
           line_offsets: 2
           positions: { key: 1 value: 0 }
         }
         expr: {
           id: 1
           ident_expr: { name: "x" }
         })pb";

struct CheckedExprToAstTypesTestCase {
  absl::string_view type;
};

class CheckedExprToAstTypesTest
    : public testing::TestWithParam<CheckedExprToAstTypesTestCase> {
 public:
  void SetUp() override {
    ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kTypesTestCheckedExpr,
                                                    &checked_expr_));
  }

 protected:
  CheckedExprPb checked_expr_;
};

TEST_P(CheckedExprToAstTypesTest, CheckedExprToAstTypes) {
  TypePb test_type;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(GetParam().type, &test_type));
  (*checked_expr_.mutable_type_map())[1] = test_type;

  ASSERT_OK_AND_ASSIGN(auto ast, CreateAstFromCheckedExpr(checked_expr_));

  EXPECT_THAT(CreateCheckedExprFromAst(*ast),
              IsOkAndHolds(EqualsProto(checked_expr_)));
}

INSTANTIATE_TEST_SUITE_P(
    Types, CheckedExprToAstTypesTest,
    testing::ValuesIn<CheckedExprToAstTypesTestCase>({
        {R"pb(list_type { elem_type { primitive: INT64 } })pb"},
        {R"pb(map_type {
                key_type { primitive: STRING }
                value_type { primitive: INT64 }
              })pb"},
        {R"pb(message_type: "com.example.TestType")pb"},
        {R"pb(primitive: BOOL)pb"},
        {R"pb(primitive: INT64)pb"},
        {R"pb(primitive: UINT64)pb"},
        {R"pb(primitive: DOUBLE)pb"},
        {R"pb(primitive: STRING)pb"},
        {R"pb(primitive: BYTES)pb"},
        {R"pb(wrapper: BOOL)pb"},
        {R"pb(wrapper: INT64)pb"},
        {R"pb(wrapper: UINT64)pb"},
        {R"pb(wrapper: DOUBLE)pb"},
        {R"pb(wrapper: STRING)pb"},
        {R"pb(wrapper: BYTES)pb"},
        {R"pb(well_known: TIMESTAMP)pb"},
        {R"pb(well_known: DURATION)pb"},
        {R"pb(well_known: ANY)pb"},
        {R"pb(dyn {})pb"},
        {R"pb(error {})pb"},
        {R"pb(null: NULL_VALUE)pb"},
        {R"pb(
           abstract_type {
             name: "MyType"
             parameter_types { primitive: INT64 }
           }
         )pb"},
        {R"pb(
           type { primitive: INT64 }
         )pb"},
        {R"pb(
           type { type {} }
         )pb"},
        {R"pb(type_param: "T")pb"},
        {R"pb(
           function {
             result_type { primitive: INT64 }
             arg_types { primitive: INT64 }
           }
         )pb"},
    }));

TEST(AstConvertersTest, ParsedExprToAst) {
  ParsedExprPb parsed_expr;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        source_info {
          syntax_version: "version"
          location: "location"
          line_offsets: 1
          line_offsets: 2
          positions { key: 1 value: 2 }
          positions { key: 3 value: 4 }
          macro_calls {
            key: 1
            value { ident_expr { name: "name" } }
          }
        }
        expr { ident_expr { name: "expr" } }
      )pb",
      &parsed_expr));

  ASSERT_OK_AND_ASSIGN(auto ast,
                       cel::extensions::CreateAstFromParsedExpr(parsed_expr));
}

TEST(AstConvertersTest, AstToParsedExprBasic) {
  ast_internal::Expr expr;
  expr.set_id(1);
  expr.mutable_ident_expr().set_name("expr");

  ast_internal::SourceInfo source_info;
  source_info.set_syntax_version("version");
  source_info.set_location("location");
  source_info.mutable_line_offsets().push_back(1);
  source_info.mutable_line_offsets().push_back(2);
  source_info.mutable_positions().insert({1, 2});
  source_info.mutable_positions().insert({3, 4});

  ast_internal::Expr macro;
  macro.mutable_ident_expr().set_name("name");
  source_info.mutable_macro_calls().insert({1, std::move(macro)});

  ast_internal::AstImpl ast(std::move(expr), std::move(source_info));

  ASSERT_OK_AND_ASSIGN(auto checked_pb, CreateParsedExprFromAst(ast));

  EXPECT_THAT(checked_pb, EqualsProto(R"pb(
                source_info {
                  syntax_version: "version"
                  location: "location"
                  line_offsets: 1
                  line_offsets: 2
                  positions { key: 1 value: 2 }
                  positions { key: 3 value: 4 }
                  macro_calls {
                    key: 1
                    value { ident_expr { name: "name" } }
                  }
                }
                expr {
                  id: 1
                  ident_expr { name: "expr" }
                }
              )pb"));
}

TEST(AstConvertersTest, ExprToAst) {
  google::api::expr::v1alpha1::Expr expr;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        ident_expr { name: "expr" }
      )pb",
      &expr));

  ASSERT_OK_AND_ASSIGN(auto ast,
                       cel::extensions::CreateAstFromParsedExpr(expr));
}

TEST(AstConvertersTest, ExprAndSourceInfoToAst) {
  google::api::expr::v1alpha1::Expr expr;
  google::api::expr::v1alpha1::SourceInfo source_info;

  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        syntax_version: "version"
        location: "location"
        line_offsets: 1
        line_offsets: 2
        positions { key: 1 value: 2 }
        positions { key: 3 value: 4 }
        macro_calls {
          key: 1
          value { ident_expr { name: "name" } }
        }
      )pb",
      &source_info));
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        ident_expr { name: "expr" }
      )pb",
      &expr));

  ASSERT_OK_AND_ASSIGN(
      auto ast, cel::extensions::CreateAstFromParsedExpr(expr, &source_info));
}

TEST(AstConvertersTest, EmptyNodeRoundTrip) {
  ParsedExprPb parsed_expr;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        expr {
          id: 1
          select_expr {
            operand {
              id: 2
              # no kind set.
            }
            field: "field"
          }
        }
        source_info {}
      )pb",
      &parsed_expr));

  ASSERT_OK_AND_ASSIGN(auto ast, CreateAstFromParsedExpr(parsed_expr));
  ASSERT_OK_AND_ASSIGN(ParsedExprPb copy, CreateParsedExprFromAst(*ast));
  EXPECT_THAT(copy, EqualsProto(parsed_expr));
}

TEST(AstConvertersTest, DurationConstantRoundTrip) {
  ParsedExprPb parsed_expr;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        expr {
          id: 1
          const_expr {
            # deprecated, but support existing ASTs.
            duration_value { seconds: 10 }
          }
        }
        source_info {}
      )pb",
      &parsed_expr));

  ASSERT_OK_AND_ASSIGN(auto ast, CreateAstFromParsedExpr(parsed_expr));
  ASSERT_OK_AND_ASSIGN(ParsedExprPb copy, CreateParsedExprFromAst(*ast));
  EXPECT_THAT(copy, EqualsProto(parsed_expr));
}

TEST(AstConvertersTest, TimestampConstantRoundTrip) {
  ParsedExprPb parsed_expr;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        expr {
          id: 1
          const_expr {
            # deprecated, but support existing ASTs.
            timestamp_value { seconds: 10 }
          }
        }
        source_info {}
      )pb",
      &parsed_expr));

  ASSERT_OK_AND_ASSIGN(auto ast, CreateAstFromParsedExpr(parsed_expr));
  ASSERT_OK_AND_ASSIGN(ParsedExprPb copy, CreateParsedExprFromAst(*ast));
  EXPECT_THAT(copy, EqualsProto(parsed_expr));
}

struct ConversionRoundTripCase {
  absl::string_view expr;
};

class ConversionRoundTripTest
    : public testing::TestWithParam<ConversionRoundTripCase> {
 public:
  ConversionRoundTripTest() {
    options_.add_macro_calls = true;
    options_.enable_optional_syntax = true;
  }

 protected:
  ParserOptions options_;
};

TEST_P(ConversionRoundTripTest, ParsedExprCopyable) {
  ASSERT_OK_AND_ASSIGN(ParsedExprPb parsed_expr,
                       Parse(GetParam().expr, "<input>", options_));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Ast> ast,
                       CreateAstFromParsedExpr(parsed_expr));

  const auto& impl = ast_internal::AstImpl::CastFromPublicAst(*ast);

  EXPECT_THAT(CreateCheckedExprFromAst(impl),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("AST is not type-checked")));
  EXPECT_THAT(CreateParsedExprFromAst(impl),
              IsOkAndHolds(EqualsProto(parsed_expr)));
}

TEST_P(ConversionRoundTripTest, CheckedExprCopyable) {
  ASSERT_OK_AND_ASSIGN(ParsedExprPb parsed_expr,
                       Parse(GetParam().expr, "<input>", options_));

  CheckedExprPb checked_expr;
  *checked_expr.mutable_expr() = parsed_expr.expr();
  *checked_expr.mutable_source_info() = parsed_expr.source_info();

  int64_t root_id = checked_expr.expr().id();
  (*checked_expr.mutable_reference_map())[root_id].add_overload_id("_==_");
  (*checked_expr.mutable_type_map())[root_id].set_primitive(TypePb::BOOL);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Ast> ast,
                       CreateAstFromCheckedExpr(checked_expr));

  const auto& impl = ast_internal::AstImpl::CastFromPublicAst(*ast);

  EXPECT_THAT(CreateCheckedExprFromAst(impl),
              IsOkAndHolds(EqualsProto(checked_expr)));
}

INSTANTIATE_TEST_SUITE_P(
    ExpressionCases, ConversionRoundTripTest,
    testing::ValuesIn<ConversionRoundTripCase>(
        {{R"cel(null == null)cel"},
         {R"cel(1 == 2)cel"},
         {R"cel(1u == 2u)cel"},
         {R"cel(1.1 == 2.1)cel"},
         {R"cel(b"1" == b"2")cel"},
         {R"cel("42" == "42")cel"},
         {R"cel("s".startsWith("s") == true)cel"},
         {R"cel([1, 2, 3] == [1, 2, 3])cel"},
         {R"cel(TestAllTypes{single_int64: 42}.single_int64 == 42)cel"},
         {R"cel([1, 2, 3].map(x, x + 2).size() == 3)cel"},
         {R"cel({"a": 1, "b": 2}["a"] == 1)cel"},
         {R"cel(ident == 42)cel"},
         {R"cel(ident.field == 42)cel"},
         {R"cel({?"abc": {}[?1]}.?abc.orValue(42) == 42)cel"},
         {R"cel([1, 2, ?optional.none()].size() == 2)cel"}}));

TEST(ExtensionConversionRoundTripTest, RoundTrip) {
  ParsedExprPb parsed_expr;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        expr {
          id: 1
          ident_expr { name: "unused" }
        }
        source_info {
          extensions {
            id: "extension"
            version { major: 1 minor: 2 }
            affected_components: COMPONENT_UNSPECIFIED
            affected_components: COMPONENT_PARSER
            affected_components: COMPONENT_TYPE_CHECKER
            affected_components: COMPONENT_RUNTIME
          }
        }
      )pb",
      &parsed_expr));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Ast> ast,
                       CreateAstFromParsedExpr(parsed_expr));

  const auto& impl = ast_internal::AstImpl::CastFromPublicAst(*ast);

  EXPECT_THAT(CreateCheckedExprFromAst(impl),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("AST is not type-checked")));
  EXPECT_THAT(CreateParsedExprFromAst(impl),
              IsOkAndHolds(EqualsProto(parsed_expr)));
}

}  // namespace
}  // namespace cel::extensions
