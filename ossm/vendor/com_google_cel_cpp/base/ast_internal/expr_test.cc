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

#include "base/ast_internal/expr.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/types/variant.h"
#include "common/expr.h"
#include "internal/testing.h"

namespace cel {
namespace ast_internal {
namespace {


TEST(AstTest, ListTypeMutableConstruction) {
  ListType type;
  type.mutable_elem_type() = Type(PrimitiveType::kBool);
  EXPECT_EQ(absl::get<PrimitiveType>(type.elem_type().type_kind()),
            PrimitiveType::kBool);
}

TEST(AstTest, MapTypeMutableConstruction) {
  MapType type;
  type.mutable_key_type() = Type(PrimitiveType::kBool);
  type.mutable_value_type() = Type(PrimitiveType::kBool);
  EXPECT_EQ(absl::get<PrimitiveType>(type.key_type().type_kind()),
            PrimitiveType::kBool);
  EXPECT_EQ(absl::get<PrimitiveType>(type.value_type().type_kind()),
            PrimitiveType::kBool);
}

TEST(AstTest, MapTypeComparatorKeyType) {
  MapType type;
  type.mutable_key_type() = Type(PrimitiveType::kBool);
  EXPECT_FALSE(type == MapType());
}

TEST(AstTest, MapTypeComparatorValueType) {
  MapType type;
  type.mutable_value_type() = Type(PrimitiveType::kBool);
  EXPECT_FALSE(type == MapType());
}

TEST(AstTest, FunctionTypeMutableConstruction) {
  FunctionType type;
  type.mutable_result_type() = Type(PrimitiveType::kBool);
  EXPECT_EQ(absl::get<PrimitiveType>(type.result_type().type_kind()),
            PrimitiveType::kBool);
}

TEST(AstTest, FunctionTypeComparatorArgTypes) {
  FunctionType type;
  type.mutable_arg_types().emplace_back(Type());
  EXPECT_FALSE(type == FunctionType());
}

TEST(AstTest, ListTypeDefaults) { EXPECT_EQ(ListType().elem_type(), Type()); }

TEST(AstTest, MapTypeDefaults) {
  EXPECT_EQ(MapType().key_type(), Type());
  EXPECT_EQ(MapType().value_type(), Type());
}

TEST(AstTest, FunctionTypeDefaults) {
  EXPECT_EQ(FunctionType().result_type(), Type());
}

TEST(AstTest, TypeDefaults) {
  EXPECT_EQ(Type().null(), nullptr);
  EXPECT_EQ(Type().primitive(), PrimitiveType::kPrimitiveTypeUnspecified);
  EXPECT_EQ(Type().wrapper(), PrimitiveType::kPrimitiveTypeUnspecified);
  EXPECT_EQ(Type().well_known(), WellKnownType::kWellKnownTypeUnspecified);
  EXPECT_EQ(Type().list_type(), ListType());
  EXPECT_EQ(Type().map_type(), MapType());
  EXPECT_EQ(Type().function(), FunctionType());
  EXPECT_EQ(Type().message_type(), MessageType());
  EXPECT_EQ(Type().type_param(), ParamType());
  EXPECT_EQ(Type().type(), Type());
  EXPECT_EQ(Type().error_type(), ErrorType());
  EXPECT_EQ(Type().abstract_type(), AbstractType());
}

TEST(AstTest, TypeComparatorTest) {
  Type type;
  type.set_type_kind(std::make_unique<Type>(PrimitiveType::kBool));

  EXPECT_TRUE(type == Type(std::make_unique<Type>(PrimitiveType::kBool)));
  EXPECT_FALSE(type == Type(PrimitiveType::kBool));
  EXPECT_FALSE(type == Type(std::unique_ptr<Type>()));
  EXPECT_FALSE(type == Type(std::make_unique<Type>(PrimitiveType::kInt64)));
}

TEST(AstTest, ExprMutableConstruction) {
  Expr expr;
  expr.mutable_const_expr().set_bool_value(true);
  ASSERT_TRUE(expr.has_const_expr());
  EXPECT_TRUE(expr.const_expr().bool_value());
  expr.mutable_ident_expr().set_name("expr");
  ASSERT_TRUE(expr.has_ident_expr());
  EXPECT_FALSE(expr.has_const_expr());
  EXPECT_EQ(expr.ident_expr().name(), "expr");
  expr.mutable_select_expr().set_field("field");
  ASSERT_TRUE(expr.has_select_expr());
  EXPECT_FALSE(expr.has_ident_expr());
  EXPECT_EQ(expr.select_expr().field(), "field");
  expr.mutable_call_expr().set_function("function");
  ASSERT_TRUE(expr.has_call_expr());
  EXPECT_FALSE(expr.has_select_expr());
  EXPECT_EQ(expr.call_expr().function(), "function");
  expr.mutable_list_expr();
  EXPECT_TRUE(expr.has_list_expr());
  EXPECT_FALSE(expr.has_call_expr());
  expr.mutable_struct_expr().set_name("name");
  ASSERT_TRUE(expr.has_struct_expr());
  EXPECT_EQ(expr.struct_expr().name(), "name");
  EXPECT_FALSE(expr.has_list_expr());
  expr.mutable_comprehension_expr().set_accu_var("accu_var");
  ASSERT_TRUE(expr.has_comprehension_expr());
  EXPECT_FALSE(expr.has_list_expr());
  EXPECT_EQ(expr.comprehension_expr().accu_var(), "accu_var");
}

TEST(AstTest, ReferenceConstantDefaultValue) {
  Reference reference;
  EXPECT_EQ(reference.value(), Constant());
}

TEST(AstTest, TypeCopyable) {
  Type type = Type(PrimitiveType::kBool);
  Type type2 = type;
  EXPECT_TRUE(type2.has_primitive());
  EXPECT_EQ(type2, type);

  type = Type(ListType(std::make_unique<Type>(PrimitiveType::kBool)));
  type2 = type;
  EXPECT_TRUE(type2.has_list_type());
  EXPECT_EQ(type2, type);

  type = Type(MapType(std::make_unique<Type>(PrimitiveType::kBool),
                      std::make_unique<Type>(PrimitiveType::kBool)));
  type2 = type;
  EXPECT_TRUE(type2.has_map_type());
  EXPECT_EQ(type2, type);

  type = Type(FunctionType(std::make_unique<Type>(PrimitiveType::kBool), {}));
  type2 = type;
  EXPECT_TRUE(type2.has_function());
  EXPECT_EQ(type2, type);

  type = Type(AbstractType("optional", {Type(PrimitiveType::kBool)}));
  type2 = type;
  EXPECT_TRUE(type2.has_abstract_type());
  EXPECT_EQ(type2, type);
}

TEST(AstTest, TypeMoveable) {
  Type type = Type(PrimitiveType::kBool);
  Type type2 = type;
  Type type3 = std::move(type);
  EXPECT_TRUE(type2.has_primitive());
  EXPECT_EQ(type2, type3);

  type = Type(ListType(std::make_unique<Type>(PrimitiveType::kBool)));
  type2 = type;
  type3 = std::move(type);
  EXPECT_TRUE(type2.has_list_type());
  EXPECT_EQ(type2, type3);

  type = Type(MapType(std::make_unique<Type>(PrimitiveType::kBool),
                      std::make_unique<Type>(PrimitiveType::kBool)));
  type2 = type;
  type3 = std::move(type);
  EXPECT_TRUE(type2.has_map_type());
  EXPECT_EQ(type2, type3);

  type = Type(FunctionType(std::make_unique<Type>(PrimitiveType::kBool), {}));
  type2 = type;
  type3 = std::move(type);
  EXPECT_TRUE(type2.has_function());
  EXPECT_EQ(type2, type3);

  type = Type(AbstractType("optional", {Type(PrimitiveType::kBool)}));
  type2 = type;
  type3 = std::move(type);
  EXPECT_TRUE(type2.has_abstract_type());
  EXPECT_EQ(type2, type3);
}

TEST(AstTest, NestedTypeKindCopyAssignable) {
  ListType list_type(std::make_unique<Type>(PrimitiveType::kBool));
  ListType list_type2;
  list_type2 = list_type;

  EXPECT_EQ(list_type2, list_type);

  MapType map_type(std::make_unique<Type>(PrimitiveType::kBool),
                   std::make_unique<Type>(PrimitiveType::kBool));
  MapType map_type2;
  map_type2 = map_type;

  AbstractType abstract_type(
      "abstract", {Type(PrimitiveType::kBool), Type(PrimitiveType::kBool)});
  AbstractType abstract_type2;
  abstract_type2 = abstract_type;

  EXPECT_EQ(abstract_type2, abstract_type);

  FunctionType function_type(
      std::make_unique<Type>(PrimitiveType::kBool),
      {Type(PrimitiveType::kBool), Type(PrimitiveType::kBool)});
  FunctionType function_type2;
  function_type2 = function_type;

  EXPECT_EQ(function_type2, function_type);
}

TEST(AstTest, ExtensionSupported) {
  SourceInfo source_info;

  source_info.mutable_extensions().push_back(
      Extension("constant_folding", nullptr, {}));

  EXPECT_EQ(source_info.extensions()[0],
            Extension("constant_folding", nullptr, {}));
}

TEST(AstTest, ExtensionEquality) {
  Extension extension1("constant_folding", nullptr, {});

  EXPECT_EQ(extension1, Extension("constant_folding", nullptr, {}));

  EXPECT_NE(extension1,
            Extension("constant_folding",
                      std::make_unique<Extension::Version>(1, 0), {}));
  EXPECT_NE(extension1, Extension("constant_folding", nullptr,
                                  {Extension::Component::kRuntime}));

  EXPECT_EQ(extension1,
            Extension("constant_folding",
                      std::make_unique<Extension::Version>(0, 0), {}));
}

}  // namespace
}  // namespace ast_internal
}  // namespace cel
