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

#include "base/ast_internal/ast_impl.h"

#include <utility>

#include "absl/container/flat_hash_map.h"
#include "base/ast.h"
#include "base/ast_internal/expr.h"
#include "internal/testing.h"

namespace cel::ast_internal {
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
  AstImpl ast_impl(std::move(expr), std::move(source_info));
  Ast& ast = ast_impl;

  // assert
  ASSERT_FALSE(ast.IsChecked());
  EXPECT_EQ(ast_impl.GetType(1), Type(DynamicType()));
  EXPECT_EQ(ast_impl.GetReturnType(), Type(DynamicType()));
  EXPECT_EQ(ast_impl.GetReference(1), nullptr);
  EXPECT_TRUE(ast_impl.root_expr().has_call_expr());
  EXPECT_EQ(ast_impl.root_expr().call_expr().function(), "_==_");
  EXPECT_EQ(ast_impl.root_expr().id(), 5);  // Parser IDs leaf to root.
  EXPECT_EQ(ast_impl.source_info().positions().at(5), 6);  // start pos of ==
}

TEST(AstImpl, CheckedExprCtor) {
  Expr expr;
  expr.mutable_ident_expr().set_name("int_value");
  expr.set_id(1);
  Reference ref;
  ref.set_name("com.int_value");
  AstImpl::ReferenceMap reference_map;
  reference_map[1] = Reference(ref);
  AstImpl::TypeMap type_map;
  type_map[1] = Type(PrimitiveType::kInt64);
  SourceInfo source_info;
  source_info.set_syntax_version("1.0");

  AstImpl ast_impl(std::move(expr), std::move(source_info),
                   std::move(reference_map), std::move(type_map), "1.0");
  Ast& ast = ast_impl;

  ASSERT_TRUE(ast.IsChecked());
  EXPECT_EQ(ast_impl.GetType(1), Type(PrimitiveType::kInt64));
  EXPECT_THAT(ast_impl.GetReference(1),
              Pointee(Truly([&ref](const Reference& arg) {
                return arg.name() == ref.name();
              })));
  EXPECT_EQ(ast_impl.GetReturnType(), Type(PrimitiveType::kInt64));
  EXPECT_TRUE(ast_impl.root_expr().has_ident_expr());
  EXPECT_EQ(ast_impl.root_expr().ident_expr().name(), "int_value");
  EXPECT_EQ(ast_impl.root_expr().id(), 1);
  EXPECT_EQ(ast_impl.source_info().syntax_version(), "1.0");
  EXPECT_EQ(ast_impl.expr_version(), "1.0");
}

TEST(AstImpl, CheckedExprDeepCopy) {
  Expr root;
  root.set_id(3);
  root.mutable_call_expr().set_function("_==_");
  root.mutable_call_expr().mutable_args().resize(2);
  auto& lhs = root.mutable_call_expr().mutable_args()[0];
  auto& rhs = root.mutable_call_expr().mutable_args()[1];
  AstImpl::TypeMap type_map;
  AstImpl::ReferenceMap reference_map;
  SourceInfo source_info;

  type_map[3] = Type(PrimitiveType::kBool);

  lhs.mutable_ident_expr().set_name("int_value");
  lhs.set_id(1);
  Reference ref;
  ref.set_name("com.int_value");
  reference_map[1] = std::move(ref);
  type_map[1] = Type(PrimitiveType::kInt64);

  rhs.mutable_const_expr().set_int_value(2);
  rhs.set_id(2);
  type_map[2] = Type(PrimitiveType::kInt64);
  source_info.set_syntax_version("1.0");

  AstImpl ast_impl(std::move(root), std::move(source_info),
                   std::move(reference_map), std::move(type_map), "1.0");
  Ast& ast = ast_impl;

  ASSERT_TRUE(ast.IsChecked());
  EXPECT_EQ(ast_impl.GetType(1), Type(PrimitiveType::kInt64));
  EXPECT_THAT(ast_impl.GetReference(1), Pointee(Truly([](const Reference& arg) {
                return arg.name() == "com.int_value";
              })));
  EXPECT_EQ(ast_impl.GetReturnType(), Type(PrimitiveType::kBool));
  EXPECT_TRUE(ast_impl.root_expr().has_call_expr());
  EXPECT_EQ(ast_impl.root_expr().call_expr().function(), "_==_");
  EXPECT_EQ(ast_impl.root_expr().id(), 3);
  EXPECT_EQ(ast_impl.source_info().syntax_version(), "1.0");
}

}  // namespace
}  // namespace cel::ast_internal
