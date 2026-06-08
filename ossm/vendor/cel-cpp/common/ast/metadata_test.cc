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

#include "common/ast/metadata.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/types/variant.h"
#include "common/expr.h"
#include "internal/testing.h"

namespace cel {
namespace {

TEST(AstTest, ListTypeSpecMutableConstruction) {
  ListTypeSpec type;
  type.mutable_elem_type() = TypeSpec(PrimitiveType::kBool);
  EXPECT_EQ(absl::get<PrimitiveType>(type.elem_type().type_kind()),
            PrimitiveType::kBool);
}

TEST(AstTest, MapTypeSpecMutableConstruction) {
  MapTypeSpec type;
  type.mutable_key_type() = TypeSpec(PrimitiveType::kBool);
  type.mutable_value_type() = TypeSpec(PrimitiveType::kBool);
  EXPECT_EQ(absl::get<PrimitiveType>(type.key_type().type_kind()),
            PrimitiveType::kBool);
  EXPECT_EQ(absl::get<PrimitiveType>(type.value_type().type_kind()),
            PrimitiveType::kBool);
}

TEST(AstTest, MapTypeSpecComparatorKeyType) {
  MapTypeSpec type;
  type.mutable_key_type() = TypeSpec(PrimitiveType::kBool);
  EXPECT_FALSE(type == MapTypeSpec());
}

TEST(AstTest, MapTypeSpecComparatorValueType) {
  MapTypeSpec type;
  type.mutable_value_type() = TypeSpec(PrimitiveType::kBool);
  EXPECT_FALSE(type == MapTypeSpec());
}

TEST(AstTest, FunctionTypeSpecMutableConstruction) {
  FunctionTypeSpec type;
  type.mutable_result_type() = TypeSpec(PrimitiveType::kBool);
  EXPECT_EQ(absl::get<PrimitiveType>(type.result_type().type_kind()),
            PrimitiveType::kBool);
}

TEST(AstTest, FunctionTypeSpecComparatorArgTypes) {
  FunctionTypeSpec type;
  type.mutable_arg_types().emplace_back(TypeSpec());
  EXPECT_FALSE(type == FunctionTypeSpec());
}

TEST(AstTest, ListTypeSpecDefaults) {
  EXPECT_EQ(ListTypeSpec().elem_type(), TypeSpec());
}

TEST(AstTest, MapTypeSpecDefaults) {
  EXPECT_EQ(MapTypeSpec().key_type(), TypeSpec());
  EXPECT_EQ(MapTypeSpec().value_type(), TypeSpec());
}

TEST(AstTest, FunctionTypeSpecDefaults) {
  EXPECT_EQ(FunctionTypeSpec().result_type(), TypeSpec());
}

TEST(AstTest, TypeDefaults) {
  EXPECT_EQ(TypeSpec().null(), NullTypeSpec());
  EXPECT_EQ(TypeSpec().primitive(), PrimitiveType::kPrimitiveTypeUnspecified);
  EXPECT_EQ(TypeSpec().wrapper(), PrimitiveType::kPrimitiveTypeUnspecified);
  EXPECT_EQ(TypeSpec().well_known(),
            WellKnownTypeSpec::kWellKnownTypeUnspecified);
  EXPECT_EQ(TypeSpec().list_type(), ListTypeSpec());
  EXPECT_EQ(TypeSpec().map_type(), MapTypeSpec());
  EXPECT_EQ(TypeSpec().function(), FunctionTypeSpec());
  EXPECT_EQ(TypeSpec().message_type(), MessageTypeSpec());
  EXPECT_EQ(TypeSpec().type_param(), ParamTypeSpec());
  EXPECT_EQ(TypeSpec().type(), TypeSpec());
  EXPECT_EQ(TypeSpec().error_type(), ErrorTypeSpec());
  EXPECT_EQ(TypeSpec().abstract_type(), AbstractType());
}

TEST(AstTest, TypeComparatorTest) {
  TypeSpec type;
  type.set_type_kind(std::make_unique<TypeSpec>(PrimitiveType::kBool));

  EXPECT_TRUE(type ==
              TypeSpec(std::make_unique<TypeSpec>(PrimitiveType::kBool)));
  EXPECT_FALSE(type == TypeSpec(PrimitiveType::kBool));
  EXPECT_FALSE(type == TypeSpec(std::unique_ptr<TypeSpec>()));
  EXPECT_FALSE(type ==
               TypeSpec(std::make_unique<TypeSpec>(PrimitiveType::kInt64)));
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
  TypeSpec type = TypeSpec(PrimitiveType::kBool);
  TypeSpec type2 = type;
  EXPECT_TRUE(type2.has_primitive());
  EXPECT_EQ(type2, type);

  type =
      TypeSpec(ListTypeSpec(std::make_unique<TypeSpec>(PrimitiveType::kBool)));
  type2 = type;
  EXPECT_TRUE(type2.has_list_type());
  EXPECT_EQ(type2, type);

  type =
      TypeSpec(MapTypeSpec(std::make_unique<TypeSpec>(PrimitiveType::kBool),
                           std::make_unique<TypeSpec>(PrimitiveType::kBool)));
  type2 = type;
  EXPECT_TRUE(type2.has_map_type());
  EXPECT_EQ(type2, type);

  type = TypeSpec(
      FunctionTypeSpec(std::make_unique<TypeSpec>(PrimitiveType::kBool), {}));
  type2 = type;
  EXPECT_TRUE(type2.has_function());
  EXPECT_EQ(type2, type);

  type = TypeSpec(AbstractType("optional", {TypeSpec(PrimitiveType::kBool)}));
  type2 = type;
  EXPECT_TRUE(type2.has_abstract_type());
  EXPECT_EQ(type2, type);
}

TEST(AstTest, TypeMoveable) {
  TypeSpec type = TypeSpec(PrimitiveType::kBool);
  TypeSpec type2 = type;
  TypeSpec type3 = std::move(type);
  EXPECT_TRUE(type2.has_primitive());
  EXPECT_EQ(type2, type3);

  type =
      TypeSpec(ListTypeSpec(std::make_unique<TypeSpec>(PrimitiveType::kBool)));
  type2 = type;
  type3 = std::move(type);
  EXPECT_TRUE(type2.has_list_type());
  EXPECT_EQ(type2, type3);

  type =
      TypeSpec(MapTypeSpec(std::make_unique<TypeSpec>(PrimitiveType::kBool),
                           std::make_unique<TypeSpec>(PrimitiveType::kBool)));
  type2 = type;
  type3 = std::move(type);
  EXPECT_TRUE(type2.has_map_type());
  EXPECT_EQ(type2, type3);

  type = TypeSpec(
      FunctionTypeSpec(std::make_unique<TypeSpec>(PrimitiveType::kBool), {}));
  type2 = type;
  type3 = std::move(type);
  EXPECT_TRUE(type2.has_function());
  EXPECT_EQ(type2, type3);

  type = TypeSpec(AbstractType("optional", {TypeSpec(PrimitiveType::kBool)}));
  type2 = type;
  type3 = std::move(type);
  EXPECT_TRUE(type2.has_abstract_type());
  EXPECT_EQ(type2, type3);
}

TEST(AstTest, NestedTypeKindCopyAssignable) {
  ListTypeSpec list_type(std::make_unique<TypeSpec>(PrimitiveType::kBool));
  ListTypeSpec list_type2;
  list_type2 = list_type;

  EXPECT_EQ(list_type2, list_type);

  MapTypeSpec map_type(std::make_unique<TypeSpec>(PrimitiveType::kBool),
                       std::make_unique<TypeSpec>(PrimitiveType::kBool));
  MapTypeSpec map_type2;
  map_type2 = map_type;

  AbstractType abstract_type("abstract", {TypeSpec(PrimitiveType::kBool),
                                          TypeSpec(PrimitiveType::kBool)});
  AbstractType abstract_type2;
  abstract_type2 = abstract_type;

  EXPECT_EQ(abstract_type2, abstract_type);

  FunctionTypeSpec function_type(
      std::make_unique<TypeSpec>(PrimitiveType::kBool),
      {TypeSpec(PrimitiveType::kBool), TypeSpec(PrimitiveType::kBool)});
  FunctionTypeSpec function_type2;
  function_type2 = function_type;

  EXPECT_EQ(function_type2, function_type);
}

TEST(AstTest, ExtensionSupported) {
  SourceInfo source_info;

  source_info.mutable_extensions().push_back(
      ExtensionSpec("constant_folding", nullptr, {}));

  EXPECT_EQ(source_info.extensions()[0],
            ExtensionSpec("constant_folding", nullptr, {}));
}

TEST(AstTest, ExtensionSpecEquality) {
  ExtensionSpec extension1("constant_folding", nullptr, {});

  EXPECT_EQ(extension1, ExtensionSpec("constant_folding", nullptr, {}));

  EXPECT_NE(extension1,
            ExtensionSpec("constant_folding",
                          std::make_unique<ExtensionSpec::Version>(1, 0), {}));
  EXPECT_NE(extension1, ExtensionSpec("constant_folding", nullptr,
                                      {ExtensionSpec::Component::kRuntime}));

  EXPECT_EQ(extension1,
            ExtensionSpec("constant_folding",
                          std::make_unique<ExtensionSpec::Version>(0, 0), {}));
}

}  // namespace
}  // namespace cel
