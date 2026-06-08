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
#include "common/ast_proto.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cel/expr/checked.pb.h"
#include "cel/expr/syntax.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "common/ast.h"
#include "common/decl.h"
#include "common/expr.h"
#include "common/source.h"
#include "common/type.h"
#include "compiler/compiler.h"
#include "compiler/compiler_factory.h"
#include "compiler/optional.h"
#include "compiler/standard_library.h"
#include "extensions/comprehensions_v2.h"
#include "internal/proto_matchers.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "google/protobuf/text_format.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::cel::PrimitiveType;
using ::cel::WellKnownTypeSpec;
using ::cel::internal::test::EqualsProto;
using ::cel::expr::CheckedExpr;
using ::cel::expr::ParsedExpr;
using ::testing::HasSubstr;

using TypePb = cel::expr::Type;

absl::StatusOr<TypeSpec> ConvertProtoTypeToNative(
    const cel::expr::Type& type) {
  CheckedExpr checked_expr;
  checked_expr.mutable_expr()->mutable_ident_expr()->set_name("foo");

  (*checked_expr.mutable_type_map())[1] = type;

  CEL_ASSIGN_OR_RETURN(auto ast, CreateAstFromCheckedExpr(checked_expr));

  const auto& type_map = ast->type_map();
  auto iter = type_map.find(1);
  if (iter != type_map.end()) {
    return iter->second;
  }
  return absl::InternalError("conversion failed but reported success");
}

TEST(AstConvertersTest, PrimitiveTypeUnspecifiedToNative) {
  cel::expr::Type type;
  type.set_primitive(cel::expr::Type::PRIMITIVE_TYPE_UNSPECIFIED);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_primitive());
  EXPECT_EQ(native_type->primitive(), PrimitiveType::kPrimitiveTypeUnspecified);
}

TEST(AstConvertersTest, PrimitiveTypeBoolToNative) {
  cel::expr::Type type;
  type.set_primitive(cel::expr::Type::BOOL);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_primitive());
  EXPECT_EQ(native_type->primitive(), PrimitiveType::kBool);
}

TEST(AstConvertersTest, PrimitiveTypeInt64ToNative) {
  cel::expr::Type type;
  type.set_primitive(cel::expr::Type::INT64);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_primitive());
  EXPECT_EQ(native_type->primitive(), PrimitiveType::kInt64);
}

TEST(AstConvertersTest, PrimitiveTypeUint64ToNative) {
  cel::expr::Type type;
  type.set_primitive(cel::expr::Type::UINT64);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_primitive());
  EXPECT_EQ(native_type->primitive(), PrimitiveType::kUint64);
}

TEST(AstConvertersTest, PrimitiveTypeDoubleToNative) {
  cel::expr::Type type;
  type.set_primitive(cel::expr::Type::DOUBLE);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_primitive());
  EXPECT_EQ(native_type->primitive(), PrimitiveType::kDouble);
}

TEST(AstConvertersTest, PrimitiveTypeStringToNative) {
  cel::expr::Type type;
  type.set_primitive(cel::expr::Type::STRING);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_primitive());
  EXPECT_EQ(native_type->primitive(), PrimitiveType::kString);
}

TEST(AstConvertersTest, PrimitiveTypeBytesToNative) {
  cel::expr::Type type;
  type.set_primitive(cel::expr::Type::BYTES);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_primitive());
  EXPECT_EQ(native_type->primitive(), PrimitiveType::kBytes);
}

TEST(AstConvertersTest, PrimitiveTypeError) {
  cel::expr::Type type;
  type.set_primitive(::cel::expr::Type_PrimitiveType(7));

  auto native_type = ConvertProtoTypeToNative(type);

  EXPECT_EQ(native_type.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(native_type.status().message(),
              ::testing::HasSubstr("Illegal type specified for "
                                   "cel::expr::Type::PrimitiveType."));
}

TEST(AstConvertersTest, WellKnownTypeUnspecifiedToNative) {
  cel::expr::Type type;
  type.set_well_known(cel::expr::Type::WELL_KNOWN_TYPE_UNSPECIFIED);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_well_known());
  EXPECT_EQ(native_type->well_known(),
            WellKnownTypeSpec::kWellKnownTypeUnspecified);
}

TEST(AstConvertersTest, WellKnownTypeAnyToNative) {
  cel::expr::Type type;
  type.set_well_known(cel::expr::Type::ANY);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_well_known());
  EXPECT_EQ(native_type->well_known(), WellKnownTypeSpec::kAny);
}

TEST(AstConvertersTest, WellKnownTypeTimestampToNative) {
  cel::expr::Type type;
  type.set_well_known(cel::expr::Type::TIMESTAMP);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_well_known());
  EXPECT_EQ(native_type->well_known(), WellKnownTypeSpec::kTimestamp);
}

TEST(AstConvertersTest, WellKnownTypeDuraionToNative) {
  cel::expr::Type type;
  type.set_well_known(cel::expr::Type::DURATION);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_well_known());
  EXPECT_EQ(native_type->well_known(), WellKnownTypeSpec::kDuration);
}

TEST(AstConvertersTest, WellKnownTypeError) {
  cel::expr::Type type;
  type.set_well_known(::cel::expr::Type_WellKnownType(4));

  auto native_type = ConvertProtoTypeToNative(type);

  EXPECT_EQ(native_type.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(native_type.status().message(),
              ::testing::HasSubstr("Illegal type specified for "
                                   "cel::expr::Type::WellKnownType."));
}

TEST(AstConvertersTest, ListTypeToNative) {
  cel::expr::Type type;
  type.mutable_list_type()->mutable_elem_type()->set_primitive(
      cel::expr::Type::BOOL);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_list_type());
  auto& native_list_type = native_type->list_type();
  ASSERT_TRUE(native_list_type.elem_type().has_primitive());
  EXPECT_EQ(native_list_type.elem_type().primitive(), PrimitiveType::kBool);
}

TEST(AstConvertersTest, MapTypeToNative) {
  cel::expr::Type type;
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
  cel::expr::Type type;
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
  cel::expr::Type type;
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
  cel::expr::Type type;
  type.mutable_dyn();

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_dyn());
}

TEST(AstConvertersTest, NullTypeToNative) {
  cel::expr::Type type;
  type.set_null(google::protobuf::NULL_VALUE);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_null());
  EXPECT_EQ(native_type->null(), NullTypeSpec());
}

TEST(AstConvertersTest, PrimitiveTypeWrapperToNative) {
  cel::expr::Type type;
  type.set_wrapper(cel::expr::Type::BOOL);

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_wrapper());
  EXPECT_EQ(native_type->wrapper(), PrimitiveType::kBool);
}

TEST(AstConvertersTest, MessageTypeToNative) {
  cel::expr::Type type;
  type.set_message_type("message");

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_message_type());
  EXPECT_EQ(native_type->message_type().type(), "message");
}

TEST(AstConvertersTest, ParamTypeToNative) {
  cel::expr::Type type;
  type.set_type_param("param");

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_type_param());
  EXPECT_EQ(native_type->type_param().type(), "param");
}

TEST(AstConvertersTest, NestedTypeToNative) {
  cel::expr::Type type;
  type.mutable_type()->mutable_dyn();

  auto native_type = ConvertProtoTypeToNative(type);

  ASSERT_TRUE(native_type->has_type());
  EXPECT_TRUE(native_type->type().has_dyn());
}

TEST(AstConvertersTest, TypeTypeDefault) {
  auto native_type = ConvertProtoTypeToNative(cel::expr::Type());

  ASSERT_THAT(native_type, IsOk());
  EXPECT_TRUE(absl::holds_alternative<UnsetTypeSpec>(native_type->type_kind()));
}

TEST(AstConvertersTest, ReferenceToNative) {
  cel::expr::CheckedExpr reference_wrapper;
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
        })pb",
      &reference_wrapper));

  ASSERT_OK_AND_ASSIGN(auto ast, CreateAstFromCheckedExpr(reference_wrapper));
  const auto& native_references = ast->reference_map();

  auto native_reference = native_references.at(1);

  EXPECT_EQ(native_reference.name(), "name");
  EXPECT_EQ(native_reference.overload_id(),
            std::vector<std::string>({"id1", "id2"}));
  EXPECT_TRUE(native_reference.value().bool_value());
}

TEST(AstConvertersTest, SourceInfoToNative) {
  cel::expr::ParsedExpr source_info_wrapper;
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
        })pb",
      &source_info_wrapper));

  ASSERT_OK_AND_ASSIGN(auto ast, CreateAstFromParsedExpr(source_info_wrapper));
  const auto& native_source_info = ast->source_info();

  EXPECT_EQ(native_source_info.syntax_version(), "version");
  EXPECT_EQ(native_source_info.location(), "location");
  EXPECT_EQ(native_source_info.line_offsets(), std::vector<int32_t>({1, 2}));
  EXPECT_EQ(native_source_info.positions().at(1), 2);
  EXPECT_EQ(native_source_info.positions().at(3), 4);
  ASSERT_TRUE(native_source_info.macro_calls().at(1).has_ident_expr());
  ASSERT_EQ(native_source_info.macro_calls().at(1).ident_expr().name(), "name");
}

TEST(AstConvertersTest, CheckedExprToAst) {
  CheckedExpr checked_expr;
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
  Ast ast;
  ast.mutable_root_expr().set_id(1);
  ast.mutable_root_expr().mutable_ident_expr().set_name("expr");

  ast.mutable_source_info().set_syntax_version("version");
  ast.mutable_source_info().set_location("location");
  ast.mutable_source_info().mutable_line_offsets().push_back(1);
  ast.mutable_source_info().mutable_line_offsets().push_back(2);
  ast.mutable_source_info().mutable_positions().insert({1, 2});
  ast.mutable_source_info().mutable_positions().insert({3, 4});

  Expr macro;
  macro.mutable_ident_expr().set_name("name");
  ast.mutable_source_info().mutable_macro_calls().insert({1, std::move(macro)});

  Reference reference;
  reference.set_name("name");
  reference.mutable_overload_id().push_back("id1");
  reference.mutable_overload_id().push_back("id2");
  reference.mutable_value().set_bool_value(true);

  TypeSpec type;
  type.set_type_kind(DynTypeSpec());

  ast.mutable_reference_map().insert({1, std::move(reference)});
  ast.mutable_type_map().insert({1, std::move(type)});

  ast.set_expr_version("version");
  ast.set_is_checked(true);

  CheckedExpr checked_expr;
  ASSERT_THAT(AstToCheckedExpr(ast, &checked_expr), IsOk());

  EXPECT_THAT(checked_expr, EqualsProto(R"pb(
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
  CheckedExpr checked_expr_;
};

TEST_P(CheckedExprToAstTypesTest, CheckedExprToAstTypes) {
  TypePb test_type;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(GetParam().type, &test_type));
  (*checked_expr_.mutable_type_map())[1] = test_type;

  ASSERT_OK_AND_ASSIGN(auto ast, CreateAstFromCheckedExpr(checked_expr_));

  CheckedExpr checked_expr;
  ASSERT_THAT(AstToCheckedExpr(*ast, &checked_expr), IsOk());

  EXPECT_THAT(checked_expr, EqualsProto(checked_expr_));
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
  ParsedExpr parsed_expr;
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

  ASSERT_OK_AND_ASSIGN(auto ast, CreateAstFromParsedExpr(parsed_expr));
}

TEST(AstConvertersTest, AstToParsedExprBasic) {
  Expr expr;
  expr.set_id(1);
  expr.mutable_ident_expr().set_name("expr");

  SourceInfo source_info;
  source_info.set_syntax_version("version");
  source_info.set_location("location");
  source_info.mutable_line_offsets().push_back(1);
  source_info.mutable_line_offsets().push_back(2);
  source_info.mutable_positions().insert({1, 2});
  source_info.mutable_positions().insert({3, 4});

  Expr macro;
  macro.mutable_ident_expr().set_name("name");
  source_info.mutable_macro_calls().insert({1, std::move(macro)});

  Ast ast(std::move(expr), std::move(source_info));

  ParsedExpr parsed_expr;
  ASSERT_THAT(AstToParsedExpr(ast, &parsed_expr), IsOk());

  EXPECT_THAT(parsed_expr, EqualsProto(R"pb(
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
  cel::expr::Expr expr;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        ident_expr { name: "expr" }
      )pb",
      &expr));

  ASSERT_OK_AND_ASSIGN(auto ast, CreateAstFromParsedExpr(expr));
}

TEST(AstConvertersTest, ExprAndSourceInfoToAst) {
  cel::expr::Expr expr;
  cel::expr::SourceInfo source_info;

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

  ASSERT_OK_AND_ASSIGN(auto ast, CreateAstFromParsedExpr(expr, &source_info));
}

TEST(AstConvertersTest, EmptyNodeRoundTrip) {
  ParsedExpr parsed_expr;
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
  ParsedExpr copy;
  ASSERT_THAT(AstToParsedExpr(*ast, &copy), IsOk());
  EXPECT_THAT(copy, EqualsProto(parsed_expr));
}

TEST(AstConvertersTest, DurationConstantRoundTrip) {
  ParsedExpr parsed_expr;
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

  ParsedExpr copy;
  ASSERT_THAT(AstToParsedExpr(*ast, &copy), IsOk());
  EXPECT_THAT(copy, EqualsProto(parsed_expr));
}

TEST(AstConvertersTest, TimestampConstantRoundTrip) {
  ParsedExpr parsed_expr;
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
  ParsedExpr copy;
  ASSERT_THAT(AstToParsedExpr(*ast, &copy), IsOk());
  EXPECT_THAT(copy, EqualsProto(parsed_expr));
}

struct ConversionRoundTripCase {
  absl::string_view expr;
};

class ConversionRoundTripTest
    : public testing::TestWithParam<ConversionRoundTripCase> {
 public:
  ConversionRoundTripTest() {
    auto builder =
        cel::NewCompilerBuilder(internal::GetTestingDescriptorPool()).value();
    builder->AddLibrary(cel::StandardCompilerLibrary()).IgnoreError();
    builder->AddLibrary(OptionalCompilerLibrary()).IgnoreError();
    builder->AddLibrary(extensions::ComprehensionsV2CompilerLibrary())
        .IgnoreError();
    builder->GetCheckerBuilder().set_container("cel.expr.conformance.proto3");
    builder->GetCheckerBuilder()
        .AddVariable(MakeVariableDecl("ident", IntType()))
        .IgnoreError();
    builder->GetCheckerBuilder()
        .AddVariable(MakeVariableDecl("map_ident", JsonMapType()))
        .IgnoreError();
    compiler_ = builder->Build().value();
  }

  absl::StatusOr<ParsedExpr> ParseToProto(absl::string_view expr) {
    CEL_ASSIGN_OR_RETURN(auto source, cel::NewSource(expr));

    CEL_ASSIGN_OR_RETURN(auto result, compiler_->GetParser().Parse(*source));
    ParsedExpr parsed_expr;

    CEL_RETURN_IF_ERROR(AstToParsedExpr(*result, &parsed_expr));
    return parsed_expr;
  }

  absl::StatusOr<CheckedExpr> CompileToProto(absl::string_view expr) {
    CEL_ASSIGN_OR_RETURN(auto result, compiler_->Compile(expr));
    if (!result.IsValid()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Compilation failed: '", expr, "': ", result.FormatError()));
    }
    CEL_ASSIGN_OR_RETURN(auto ast, result.ReleaseAst());
    CheckedExpr checked_expr;
    CEL_RETURN_IF_ERROR(AstToCheckedExpr(*ast, &checked_expr));
    return checked_expr;
  }

 protected:
  std::unique_ptr<Compiler> compiler_;
};

TEST_P(ConversionRoundTripTest, ParsedExprCopyable) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, ParseToProto(GetParam().expr));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Ast> ast,
                       CreateAstFromParsedExpr(parsed_expr));

  CheckedExpr expr_pb;
  EXPECT_THAT(AstToCheckedExpr(*ast, &expr_pb),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("AST is not type-checked")));
  ParsedExpr proto_out;
  ASSERT_THAT(AstToParsedExpr(*ast, &proto_out), IsOk());
  EXPECT_THAT(proto_out, EqualsProto(parsed_expr));
}

TEST_P(ConversionRoundTripTest, ExprCopyable) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, ParseToProto(GetParam().expr));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Ast> ast,
                       CreateAstFromParsedExpr(parsed_expr));

  Expr copy = ast->root_expr();
  ast->mutable_root_expr() = std::move(copy);

  ParsedExpr parsed_pb_out;
  CheckedExpr checked_pb_out;
  EXPECT_THAT(AstToCheckedExpr(*ast, &checked_pb_out),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("AST is not type-checked")));
  ASSERT_THAT(AstToParsedExpr(*ast, &parsed_pb_out), IsOk());
  EXPECT_THAT(parsed_pb_out, EqualsProto(parsed_expr));
}

TEST_P(ConversionRoundTripTest, CheckedExprRoundTrip) {
  ASSERT_OK_AND_ASSIGN(CheckedExpr checked_expr,
                       CompileToProto(GetParam().expr));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Ast> ast,
                       CreateAstFromCheckedExpr(checked_expr));

  CheckedExpr checked_pb_out;
  ASSERT_THAT(AstToCheckedExpr(*ast, &checked_pb_out), IsOk());
  EXPECT_THAT(checked_pb_out, EqualsProto(checked_expr));
}

TEST_P(ConversionRoundTripTest, CheckedExprCopyRoundTrip) {
  ASSERT_OK_AND_ASSIGN(CheckedExpr checked_expr,
                       CompileToProto(GetParam().expr));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Ast> ast,
                       CreateAstFromCheckedExpr(checked_expr));

  Ast copy = *ast;
  CheckedExpr checked_pb_out;
  ASSERT_THAT(AstToCheckedExpr(copy, &checked_pb_out), IsOk());
  EXPECT_THAT(checked_pb_out, EqualsProto(checked_expr));
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
         {R"cel([1, 2, 3].all(i, e, i == e - 1) == true)cel"},
         {R"cel(TestAllTypes{single_int64: 42}.single_int64 == 42)cel"},
         {R"cel([1, 2, 3].map(x, x + 2).size() == 3)cel"},
         {R"cel({"a": 1, "b": 2}["a"] == 1)cel"},
         {R"cel(ident == 42)cel"},
         {R"cel(map_ident.field == 42)cel"},
         {R"cel({?"abc": {}[?1]}.?abc.orValue(42) == 42)cel"},
         {R"cel([1, 2, ?optional.none()].size() == 2)cel"}}));

TEST(ExtensionConversionRoundTripTest, RoundTrip) {
  ParsedExpr parsed_expr;
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

  CheckedExpr expr_pb;
  EXPECT_THAT(AstToCheckedExpr(*ast, &expr_pb),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("AST is not type-checked")));
  ParsedExpr copy;
  ASSERT_THAT(AstToParsedExpr(*ast, &copy), IsOk());
  EXPECT_THAT(copy, EqualsProto(parsed_expr));
}

}  // namespace
}  // namespace cel
