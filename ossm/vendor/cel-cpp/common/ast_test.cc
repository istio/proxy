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

#include "common/ast.h"

#include <utility>

#include "absl/container/flat_hash_map.h"
#include "common/expr.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::testing::Pointee;
using ::testing::Truly;

TEST(AstImpl, RawExprCtor) {
  // arrange
  // make ast for 2 + 1 == 3
  Expr expr;
  auto& call = expr.mutable_call_expr();
  expr.set_id(5);
  call.set_function("_==_");
  auto& eq_lhs = call.mutable_args().emplace_back();
  eq_lhs.mutable_call_expr().set_function("_+_");
  eq_lhs.set_id(3);
  auto& sum_lhs = eq_lhs.mutable_call_expr().mutable_args().emplace_back();
  sum_lhs.mutable_const_expr().set_int_value(2);
  sum_lhs.set_id(1);
  auto& sum_rhs = eq_lhs.mutable_call_expr().mutable_args().emplace_back();
  sum_rhs.mutable_const_expr().set_int_value(1);
  sum_rhs.set_id(2);
  auto& eq_rhs = call.mutable_args().emplace_back();
  eq_rhs.mutable_const_expr().set_int_value(3);
  eq_rhs.set_id(4);

  SourceInfo source_info;
  source_info.mutable_positions()[5] = 6;

  // act
  Ast ast(std::move(expr), std::move(source_info));

  // assert
  ASSERT_FALSE(ast.is_checked());
  EXPECT_EQ(ast.GetTypeOrDyn(1), TypeSpec(DynTypeSpec()));
  EXPECT_EQ(ast.GetReturnType(), TypeSpec(DynTypeSpec()));
  EXPECT_EQ(ast.GetReference(1), nullptr);
  EXPECT_TRUE(ast.root_expr().has_call_expr());
  EXPECT_EQ(ast.root_expr().call_expr().function(), "_==_");
  EXPECT_EQ(ast.root_expr().id(), 5);  // Parser IDs leaf to root.
  EXPECT_EQ(ast.source_info().positions().at(5), 6);  // start pos of ==
}

TEST(AstImpl, CheckedExprCtor) {
  Expr expr;
  expr.mutable_ident_expr().set_name("int_value");
  expr.set_id(1);
  Reference ref;
  ref.set_name("com.int_value");
  Ast::ReferenceMap reference_map;
  reference_map[1] = Reference(ref);
  Ast::TypeMap type_map;
  type_map[1] = TypeSpec(PrimitiveType::kInt64);
  SourceInfo source_info;
  source_info.set_syntax_version("1.0");

  Ast ast(std::move(expr), std::move(source_info), std::move(reference_map),
          std::move(type_map), "1.0");

  ASSERT_TRUE(ast.is_checked());
  EXPECT_EQ(ast.GetTypeOrDyn(1), TypeSpec(PrimitiveType::kInt64));
  EXPECT_THAT(ast.GetReference(1), Pointee(Truly([&ref](const Reference& arg) {
                return arg.name() == ref.name();
              })));
  EXPECT_EQ(ast.GetReturnType(), TypeSpec(PrimitiveType::kInt64));
  EXPECT_TRUE(ast.root_expr().has_ident_expr());
  EXPECT_EQ(ast.root_expr().ident_expr().name(), "int_value");
  EXPECT_EQ(ast.root_expr().id(), 1);
  EXPECT_EQ(ast.source_info().syntax_version(), "1.0");
  EXPECT_EQ(ast.expr_version(), "1.0");
}

TEST(AstImpl, CheckedExprDeepCopy) {
  Expr root;
  root.set_id(3);
  root.mutable_call_expr().set_function("_==_");
  root.mutable_call_expr().mutable_args().resize(2);
  auto& lhs = root.mutable_call_expr().mutable_args()[0];
  auto& rhs = root.mutable_call_expr().mutable_args()[1];
  Ast::TypeMap type_map;
  Ast::ReferenceMap reference_map;
  SourceInfo source_info;

  type_map[3] = TypeSpec(PrimitiveType::kBool);

  lhs.mutable_ident_expr().set_name("int_value");
  lhs.set_id(1);
  Reference ref;
  ref.set_name("com.int_value");
  reference_map[1] = std::move(ref);
  type_map[1] = TypeSpec(PrimitiveType::kInt64);

  rhs.mutable_const_expr().set_int_value(2);
  rhs.set_id(2);
  type_map[2] = TypeSpec(PrimitiveType::kInt64);
  source_info.set_syntax_version("1.0");

  Ast ast(std::move(root), std::move(source_info), std::move(reference_map),
          std::move(type_map), "1.0");

  ASSERT_TRUE(ast.IsChecked());
  EXPECT_EQ(ast.GetTypeOrDyn(1), TypeSpec(PrimitiveType::kInt64));
  EXPECT_THAT(ast.GetReference(1), Pointee(Truly([](const Reference& arg) {
                return arg.name() == "com.int_value";
              })));
  EXPECT_EQ(ast.GetReturnType(), TypeSpec(PrimitiveType::kBool));
  EXPECT_TRUE(ast.root_expr().has_call_expr());
  EXPECT_EQ(ast.root_expr().call_expr().function(), "_==_");
  EXPECT_EQ(ast.root_expr().id(), 3);
  EXPECT_EQ(ast.source_info().syntax_version(), "1.0");
}

}  // namespace
}  // namespace cel
