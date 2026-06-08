// Copyright 2024 Google LLC
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

#include "common/expr.h"

#include <utility>

#include "internal/testing.h"

namespace cel {
namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::SizeIs;
using ::testing::VariantWith;

Expr MakeUnspecifiedExpr(ExprId id) {
  Expr expr;
  expr.set_id(id);
  return expr;
}

ListExprElement MakeListExprElement(Expr expr, bool optional = false) {
  ListExprElement element;
  element.set_expr(std::move(expr));
  element.set_optional(optional);
  return element;
}

StructExprField MakeStructExprField(ExprId id, const char* name, Expr value,
                                    bool optional = false) {
  StructExprField field;
  field.set_id(id);
  field.set_name(name);
  field.set_value(std::move(value));
  field.set_optional(optional);
  return field;
}

MapExprEntry MakeMapExprEntry(ExprId id, Expr key, Expr value,
                              bool optional = false) {
  MapExprEntry entry;
  entry.set_id(id);
  entry.set_key(std::move(key));
  entry.set_value(std::move(value));
  entry.set_optional(optional);
  return entry;
}

TEST(UnspecifiedExpr, Equality) {
  EXPECT_EQ(UnspecifiedExpr{}, UnspecifiedExpr{});
}

TEST(IdentExpr, Name) {
  IdentExpr ident_expr;
  EXPECT_THAT(ident_expr.name(), IsEmpty());
  ident_expr.set_name("foo");
  EXPECT_THAT(ident_expr.name(), Eq("foo"));
  auto name = ident_expr.release_name();
  EXPECT_THAT(name, Eq("foo"));
  EXPECT_THAT(ident_expr.name(), IsEmpty());
}

TEST(IdentExpr, Equality) {
  EXPECT_EQ(IdentExpr{}, IdentExpr{});
  IdentExpr ident_expr;
  ident_expr.set_name("foo");
  EXPECT_NE(IdentExpr{}, ident_expr);
}

TEST(SelectExpr, Operand) {
  SelectExpr select_expr;
  EXPECT_THAT(select_expr.has_operand(), IsFalse());
  EXPECT_EQ(select_expr.operand(), Expr{});
  select_expr.set_operand(MakeUnspecifiedExpr(1));
  EXPECT_THAT(select_expr.has_operand(), IsTrue());
  EXPECT_EQ(select_expr.operand(), MakeUnspecifiedExpr(1));
  auto operand = select_expr.release_operand();
  EXPECT_THAT(select_expr.has_operand(), IsFalse());
  EXPECT_EQ(select_expr.operand(), Expr{});
}

TEST(SelectExpr, Field) {
  SelectExpr select_expr;
  EXPECT_THAT(select_expr.field(), IsEmpty());
  select_expr.set_field("foo");
  EXPECT_THAT(select_expr.field(), Eq("foo"));
  auto field = select_expr.release_field();
  EXPECT_THAT(field, Eq("foo"));
  EXPECT_THAT(select_expr.field(), IsEmpty());
}

TEST(SelectExpr, TestOnly) {
  SelectExpr select_expr;
  EXPECT_THAT(select_expr.test_only(), IsFalse());
  select_expr.set_test_only(true);
  EXPECT_THAT(select_expr.test_only(), IsTrue());
}

TEST(SelectExpr, Equality) {
  EXPECT_EQ(SelectExpr{}, SelectExpr{});
  SelectExpr select_expr;
  select_expr.set_test_only(true);
  EXPECT_NE(SelectExpr{}, select_expr);
}

TEST(CallExpr, Function) {
  CallExpr call_expr;
  EXPECT_THAT(call_expr.function(), IsEmpty());
  call_expr.set_function("foo");
  EXPECT_THAT(call_expr.function(), Eq("foo"));
  auto function = call_expr.release_function();
  EXPECT_THAT(function, Eq("foo"));
  EXPECT_THAT(call_expr.function(), IsEmpty());
}

TEST(CallExpr, Target) {
  CallExpr call_expr;
  EXPECT_THAT(call_expr.has_target(), IsFalse());
  EXPECT_EQ(call_expr.target(), Expr{});
  call_expr.set_target(MakeUnspecifiedExpr(1));
  EXPECT_THAT(call_expr.has_target(), IsTrue());
  EXPECT_EQ(call_expr.target(), MakeUnspecifiedExpr(1));
  auto operand = call_expr.release_target();
  EXPECT_THAT(call_expr.has_target(), IsFalse());
  EXPECT_EQ(call_expr.target(), Expr{});
}

TEST(CallExpr, Args) {
  CallExpr call_expr;
  EXPECT_THAT(call_expr.args(), IsEmpty());
  call_expr.mutable_args().push_back(MakeUnspecifiedExpr(1));
  ASSERT_THAT(call_expr.args(), SizeIs(1));
  EXPECT_EQ(call_expr.args()[0], MakeUnspecifiedExpr(1));
  auto args = call_expr.release_args();
  static_cast<void>(args);
  EXPECT_THAT(call_expr.args(), IsEmpty());
}

TEST(CallExpr, Equality) {
  EXPECT_EQ(CallExpr{}, CallExpr{});
  CallExpr call_expr;
  call_expr.mutable_args().push_back(MakeUnspecifiedExpr(1));
  EXPECT_NE(CallExpr{}, call_expr);
}

TEST(ListExprElement, Expr) {
  ListExprElement element;
  EXPECT_THAT(element.has_expr(), IsFalse());
  EXPECT_EQ(element.expr(), Expr{});
  element.set_expr(MakeUnspecifiedExpr(1));
  EXPECT_THAT(element.has_expr(), IsTrue());
  EXPECT_EQ(element.expr(), MakeUnspecifiedExpr(1));
  auto operand = element.release_expr();
  EXPECT_THAT(element.has_expr(), IsFalse());
  EXPECT_EQ(element.expr(), Expr{});
}

TEST(ListExprElement, Optional) {
  ListExprElement element;
  EXPECT_THAT(element.optional(), IsFalse());
  element.set_optional(true);
  EXPECT_THAT(element.optional(), IsTrue());
}

TEST(ListExprElement, Equality) {
  EXPECT_EQ(ListExprElement{}, ListExprElement{});
  ListExprElement element;
  element.set_optional(true);
  EXPECT_NE(ListExprElement{}, element);
}

TEST(ListExpr, Elements) {
  ListExpr list_expr;
  EXPECT_THAT(list_expr.elements(), IsEmpty());
  list_expr.mutable_elements().push_back(
      MakeListExprElement(MakeUnspecifiedExpr(1)));
  ASSERT_THAT(list_expr.elements(), SizeIs(1));
  EXPECT_EQ(list_expr.elements()[0],
            MakeListExprElement(MakeUnspecifiedExpr(1)));
  auto elements = list_expr.release_elements();
  static_cast<void>(elements);
  EXPECT_THAT(list_expr.elements(), IsEmpty());
}

TEST(ListExpr, Equality) {
  EXPECT_EQ(ListExpr{}, ListExpr{});
  ListExpr list_expr;
  list_expr.mutable_elements().push_back(
      MakeListExprElement(MakeUnspecifiedExpr(0), true));
  EXPECT_NE(ListExpr{}, list_expr);
}

TEST(StructExprField, Id) {
  StructExprField field;
  EXPECT_THAT(field.id(), Eq(0));
  field.set_id(1);
  EXPECT_THAT(field.id(), Eq(1));
}

TEST(StructExprField, Name) {
  StructExprField field;
  EXPECT_THAT(field.name(), IsEmpty());
  field.set_name("foo");
  EXPECT_THAT(field.name(), Eq("foo"));
  auto name = field.release_name();
  EXPECT_THAT(name, Eq("foo"));
  EXPECT_THAT(field.name(), IsEmpty());
}

TEST(StructExprField, Value) {
  StructExprField field;
  EXPECT_THAT(field.has_value(), IsFalse());
  EXPECT_EQ(field.value(), Expr{});
  field.set_value(MakeUnspecifiedExpr(1));
  EXPECT_THAT(field.has_value(), IsTrue());
  EXPECT_EQ(field.value(), MakeUnspecifiedExpr(1));
  auto value = field.release_value();
  EXPECT_THAT(field.has_value(), IsFalse());
  EXPECT_EQ(field.value(), Expr{});
}

TEST(StructExprField, Optional) {
  StructExprField field;
  EXPECT_THAT(field.optional(), IsFalse());
  field.set_optional(true);
  EXPECT_THAT(field.optional(), IsTrue());
}

TEST(StructExprField, Equality) {
  EXPECT_EQ(StructExprField{}, StructExprField{});
  StructExprField field;
  field.set_optional(true);
  EXPECT_NE(StructExprField{}, field);
}

TEST(StructExpr, Name) {
  StructExpr struct_expr;
  EXPECT_THAT(struct_expr.name(), IsEmpty());
  struct_expr.set_name("foo");
  EXPECT_THAT(struct_expr.name(), Eq("foo"));
  auto name = struct_expr.release_name();
  EXPECT_THAT(name, Eq("foo"));
  EXPECT_THAT(struct_expr.name(), IsEmpty());
}

TEST(StructExpr, Fields) {
  StructExpr struct_expr;
  EXPECT_THAT(struct_expr.fields(), IsEmpty());
  struct_expr.mutable_fields().push_back(
      MakeStructExprField(1, "foo", MakeUnspecifiedExpr(1)));
  ASSERT_THAT(struct_expr.fields(), SizeIs(1));
  EXPECT_EQ(struct_expr.fields()[0],
            MakeStructExprField(1, "foo", MakeUnspecifiedExpr(1)));
  auto fields = struct_expr.release_fields();
  static_cast<void>(fields);
  EXPECT_THAT(struct_expr.fields(), IsEmpty());
}

TEST(StructExpr, Equality) {
  EXPECT_EQ(StructExpr{}, StructExpr{});
  StructExpr struct_expr;
  struct_expr.mutable_fields().push_back(
      MakeStructExprField(0, "", MakeUnspecifiedExpr(0), true));
  EXPECT_NE(StructExpr{}, struct_expr);
}

TEST(MapExprEntry, Id) {
  MapExprEntry entry;
  EXPECT_THAT(entry.id(), Eq(0));
  entry.set_id(1);
  EXPECT_THAT(entry.id(), Eq(1));
}

TEST(MapExprEntry, Key) {
  MapExprEntry entry;
  EXPECT_THAT(entry.has_key(), IsFalse());
  EXPECT_EQ(entry.key(), Expr{});
  entry.set_key(MakeUnspecifiedExpr(1));
  EXPECT_THAT(entry.has_key(), IsTrue());
  EXPECT_EQ(entry.key(), MakeUnspecifiedExpr(1));
  auto key = entry.release_key();
  static_cast<void>(key);
  EXPECT_THAT(entry.has_key(), IsFalse());
  EXPECT_EQ(entry.key(), Expr{});
}

TEST(MapExprEntry, Value) {
  MapExprEntry entry;
  EXPECT_THAT(entry.has_value(), IsFalse());
  EXPECT_EQ(entry.value(), Expr{});
  entry.set_value(MakeUnspecifiedExpr(1));
  EXPECT_THAT(entry.has_value(), IsTrue());
  EXPECT_EQ(entry.value(), MakeUnspecifiedExpr(1));
  auto value = entry.release_value();
  static_cast<void>(value);
  EXPECT_THAT(entry.has_value(), IsFalse());
  EXPECT_EQ(entry.value(), Expr{});
}

TEST(MapExprEntry, Optional) {
  MapExprEntry entry;
  EXPECT_THAT(entry.optional(), IsFalse());
  entry.set_optional(true);
  EXPECT_THAT(entry.optional(), IsTrue());
}

TEST(MapExprEntry, Equality) {
  EXPECT_EQ(StructExprField{}, StructExprField{});
  StructExprField field;
  field.set_optional(true);
  EXPECT_NE(StructExprField{}, field);
}

TEST(MapExpr, Entries) {
  MapExpr map_expr;
  EXPECT_THAT(map_expr.entries(), IsEmpty());
  map_expr.mutable_entries().push_back(
      MakeMapExprEntry(1, MakeUnspecifiedExpr(1), MakeUnspecifiedExpr(1)));
  ASSERT_THAT(map_expr.entries(), SizeIs(1));
  EXPECT_EQ(map_expr.entries()[0], MakeMapExprEntry(1, MakeUnspecifiedExpr(1),
                                                    MakeUnspecifiedExpr(1)));
  auto entries = map_expr.release_entries();
  static_cast<void>(entries);
  EXPECT_THAT(map_expr.entries(), IsEmpty());
}

TEST(MapExpr, Equality) {
  EXPECT_EQ(MapExpr{}, MapExpr{});
  MapExpr map_expr;
  map_expr.mutable_entries().push_back(MakeMapExprEntry(
      0, MakeUnspecifiedExpr(0), MakeUnspecifiedExpr(0), true));
  EXPECT_NE(MapExpr{}, map_expr);
}

TEST(ComprehensionExpr, IterVar) {
  ComprehensionExpr comprehension_expr;
  EXPECT_THAT(comprehension_expr.iter_var(), IsEmpty());
  comprehension_expr.set_iter_var("foo");
  EXPECT_THAT(comprehension_expr.iter_var(), Eq("foo"));
  auto iter_var = comprehension_expr.release_iter_var();
  EXPECT_THAT(iter_var, Eq("foo"));
  EXPECT_THAT(comprehension_expr.iter_var(), IsEmpty());
}

TEST(ComprehensionExpr, IterRange) {
  ComprehensionExpr comprehension_expr;
  EXPECT_THAT(comprehension_expr.has_iter_range(), IsFalse());
  EXPECT_EQ(comprehension_expr.iter_range(), Expr{});
  comprehension_expr.set_iter_range(MakeUnspecifiedExpr(1));
  EXPECT_THAT(comprehension_expr.has_iter_range(), IsTrue());
  EXPECT_EQ(comprehension_expr.iter_range(), MakeUnspecifiedExpr(1));
  auto operand = comprehension_expr.release_iter_range();
  EXPECT_THAT(comprehension_expr.has_iter_range(), IsFalse());
  EXPECT_EQ(comprehension_expr.iter_range(), Expr{});
}

TEST(ComprehensionExpr, AccuVar) {
  ComprehensionExpr comprehension_expr;
  EXPECT_THAT(comprehension_expr.accu_var(), IsEmpty());
  comprehension_expr.set_accu_var("foo");
  EXPECT_THAT(comprehension_expr.accu_var(), Eq("foo"));
  auto accu_var = comprehension_expr.release_accu_var();
  EXPECT_THAT(accu_var, Eq("foo"));
  EXPECT_THAT(comprehension_expr.accu_var(), IsEmpty());
}

TEST(ComprehensionExpr, AccuInit) {
  ComprehensionExpr comprehension_expr;
  EXPECT_THAT(comprehension_expr.has_accu_init(), IsFalse());
  EXPECT_EQ(comprehension_expr.accu_init(), Expr{});
  comprehension_expr.set_accu_init(MakeUnspecifiedExpr(1));
  EXPECT_THAT(comprehension_expr.has_accu_init(), IsTrue());
  EXPECT_EQ(comprehension_expr.accu_init(), MakeUnspecifiedExpr(1));
  auto operand = comprehension_expr.release_accu_init();
  EXPECT_THAT(comprehension_expr.has_accu_init(), IsFalse());
  EXPECT_EQ(comprehension_expr.accu_init(), Expr{});
}

TEST(ComprehensionExpr, LoopCondition) {
  ComprehensionExpr comprehension_expr;
  EXPECT_THAT(comprehension_expr.has_loop_condition(), IsFalse());
  EXPECT_EQ(comprehension_expr.loop_condition(), Expr{});
  comprehension_expr.set_loop_condition(MakeUnspecifiedExpr(1));
  EXPECT_THAT(comprehension_expr.has_loop_condition(), IsTrue());
  EXPECT_EQ(comprehension_expr.loop_condition(), MakeUnspecifiedExpr(1));
  auto operand = comprehension_expr.release_loop_condition();
  EXPECT_THAT(comprehension_expr.has_loop_condition(), IsFalse());
  EXPECT_EQ(comprehension_expr.loop_condition(), Expr{});
}

TEST(ComprehensionExpr, LoopStep) {
  ComprehensionExpr comprehension_expr;
  EXPECT_THAT(comprehension_expr.has_loop_step(), IsFalse());
  EXPECT_EQ(comprehension_expr.loop_step(), Expr{});
  comprehension_expr.set_loop_step(MakeUnspecifiedExpr(1));
  EXPECT_THAT(comprehension_expr.has_loop_step(), IsTrue());
  EXPECT_EQ(comprehension_expr.loop_step(), MakeUnspecifiedExpr(1));
  auto operand = comprehension_expr.release_loop_step();
  EXPECT_THAT(comprehension_expr.has_loop_step(), IsFalse());
  EXPECT_EQ(comprehension_expr.loop_step(), Expr{});
}

TEST(ComprehensionExpr, Result) {
  ComprehensionExpr comprehension_expr;
  EXPECT_THAT(comprehension_expr.has_result(), IsFalse());
  EXPECT_EQ(comprehension_expr.result(), Expr{});
  comprehension_expr.set_result(MakeUnspecifiedExpr(1));
  EXPECT_THAT(comprehension_expr.has_result(), IsTrue());
  EXPECT_EQ(comprehension_expr.result(), MakeUnspecifiedExpr(1));
  auto operand = comprehension_expr.release_result();
  EXPECT_THAT(comprehension_expr.has_result(), IsFalse());
  EXPECT_EQ(comprehension_expr.result(), Expr{});
}

TEST(ComprehensionExpr, Equality) {
  EXPECT_EQ(ComprehensionExpr{}, ComprehensionExpr{});
  ComprehensionExpr comprehension_expr;
  comprehension_expr.set_result(MakeUnspecifiedExpr(1));
  EXPECT_NE(ComprehensionExpr{}, comprehension_expr);
}

TEST(Expr, Unspecified) {
  Expr expr;
  EXPECT_THAT(expr.id(), Eq(ExprId{0}));
  EXPECT_THAT(expr.kind(), VariantWith<UnspecifiedExpr>(_));
  EXPECT_EQ(expr.kind_case(), ExprKindCase::kUnspecifiedExpr);
  EXPECT_EQ(expr, Expr{});
}

TEST(Expr, Ident) {
  Expr expr;
  EXPECT_THAT(expr.has_ident_expr(), IsFalse());
  EXPECT_EQ(expr.ident_expr(), IdentExpr{});
  auto& ident_expr = expr.mutable_ident_expr();
  EXPECT_THAT(expr.has_ident_expr(), IsTrue());
  EXPECT_NE(expr, Expr{});
  ident_expr.set_name("foo");
  EXPECT_NE(expr.ident_expr(), IdentExpr{});
  EXPECT_EQ(expr.kind_case(), ExprKindCase::kIdentExpr);
  static_cast<void>(expr.release_ident_expr());
  EXPECT_THAT(expr.has_ident_expr(), IsFalse());
  EXPECT_EQ(expr.ident_expr(), IdentExpr{});
  EXPECT_EQ(expr, Expr{});
}

TEST(Expr, Select) {
  Expr expr;
  EXPECT_THAT(expr.has_select_expr(), IsFalse());
  EXPECT_EQ(expr.select_expr(), SelectExpr{});
  auto& select_expr = expr.mutable_select_expr();
  EXPECT_THAT(expr.has_select_expr(), IsTrue());
  EXPECT_NE(expr, Expr{});
  select_expr.set_field("foo");
  EXPECT_NE(expr.select_expr(), SelectExpr{});
  EXPECT_EQ(expr.kind_case(), ExprKindCase::kSelectExpr);
  static_cast<void>(expr.release_select_expr());
  EXPECT_THAT(expr.has_select_expr(), IsFalse());
  EXPECT_EQ(expr.select_expr(), SelectExpr{});
  EXPECT_EQ(expr, Expr{});
}

TEST(Expr, Call) {
  Expr expr;
  EXPECT_THAT(expr.has_call_expr(), IsFalse());
  EXPECT_EQ(expr.call_expr(), CallExpr{});
  auto& call_expr = expr.mutable_call_expr();
  EXPECT_THAT(expr.has_call_expr(), IsTrue());
  EXPECT_NE(expr, Expr{});
  call_expr.set_function("foo");
  EXPECT_NE(expr.call_expr(), CallExpr{});
  EXPECT_EQ(expr.kind_case(), ExprKindCase::kCallExpr);
  static_cast<void>(expr.release_call_expr());
  EXPECT_THAT(expr.has_call_expr(), IsFalse());
  EXPECT_EQ(expr.call_expr(), CallExpr{});
  EXPECT_EQ(expr, Expr{});
}

TEST(Expr, List) {
  Expr expr;
  EXPECT_THAT(expr.has_list_expr(), IsFalse());
  EXPECT_EQ(expr.list_expr(), ListExpr{});
  auto& list_expr = expr.mutable_list_expr();
  EXPECT_THAT(expr.has_list_expr(), IsTrue());
  EXPECT_NE(expr, Expr{});
  list_expr.mutable_elements().push_back(MakeListExprElement(Expr{}, true));
  EXPECT_NE(expr.list_expr(), ListExpr{});
  EXPECT_EQ(expr.kind_case(), ExprKindCase::kListExpr);
  static_cast<void>(expr.release_list_expr());
  EXPECT_THAT(expr.has_list_expr(), IsFalse());
  EXPECT_EQ(expr.list_expr(), ListExpr{});
  EXPECT_EQ(expr, Expr{});
}

TEST(Expr, Struct) {
  Expr expr;
  EXPECT_THAT(expr.has_struct_expr(), IsFalse());
  EXPECT_EQ(expr.struct_expr(), StructExpr{});
  auto& struct_expr = expr.mutable_struct_expr();
  EXPECT_THAT(expr.has_struct_expr(), IsTrue());
  EXPECT_NE(expr, Expr{});
  struct_expr.set_name("foo");
  EXPECT_NE(expr.struct_expr(), StructExpr{});
  EXPECT_EQ(expr.kind_case(), ExprKindCase::kStructExpr);
  static_cast<void>(expr.release_struct_expr());
  EXPECT_THAT(expr.has_struct_expr(), IsFalse());
  EXPECT_EQ(expr.struct_expr(), StructExpr{});
  EXPECT_EQ(expr, Expr{});
}

TEST(Expr, Map) {
  Expr expr;
  EXPECT_THAT(expr.has_map_expr(), IsFalse());
  EXPECT_EQ(expr.map_expr(), MapExpr{});
  auto& map_expr = expr.mutable_map_expr();
  EXPECT_THAT(expr.has_map_expr(), IsTrue());
  EXPECT_NE(expr, Expr{});
  map_expr.mutable_entries().push_back(MakeMapExprEntry(1, Expr{}, Expr{}));
  EXPECT_NE(expr.map_expr(), MapExpr{});
  EXPECT_EQ(expr.kind_case(), ExprKindCase::kMapExpr);
  static_cast<void>(expr.release_map_expr());
  EXPECT_THAT(expr.has_map_expr(), IsFalse());
  EXPECT_EQ(expr.map_expr(), MapExpr{});
  EXPECT_EQ(expr, Expr{});
}

TEST(Expr, Comprehension) {
  Expr expr;
  EXPECT_THAT(expr.has_comprehension_expr(), IsFalse());
  EXPECT_EQ(expr.comprehension_expr(), ComprehensionExpr{});
  auto& comprehension_expr = expr.mutable_comprehension_expr();
  EXPECT_THAT(expr.has_comprehension_expr(), IsTrue());
  EXPECT_NE(expr, Expr{});
  comprehension_expr.set_iter_var("foo");
  EXPECT_NE(expr.comprehension_expr(), ComprehensionExpr{});
  EXPECT_EQ(expr.kind_case(), ExprKindCase::kComprehensionExpr);
  static_cast<void>(expr.release_comprehension_expr());
  EXPECT_THAT(expr.has_comprehension_expr(), IsFalse());
  EXPECT_EQ(expr.comprehension_expr(), ComprehensionExpr{});
  EXPECT_EQ(expr, Expr{});
}

TEST(Expr, CopyCtor) {
  Expr expr;
  expr.mutable_select_expr().set_field("foo");
  Expr& operand = expr.mutable_select_expr().mutable_operand();
  operand.mutable_ident_expr().set_name("bar");
  Expr expr_copy = expr;
  EXPECT_EQ(expr_copy.select_expr().field(), "foo");
  EXPECT_EQ(expr_copy.select_expr().operand().ident_expr().name(), "bar");
}

TEST(Expr, CopyAssignChildReference) {
  Expr expr;
  expr.mutable_select_expr().set_field("foo");
  Expr& operand = expr.mutable_select_expr().mutable_operand();
  operand.mutable_call_expr().set_function("bar");
  auto& args = operand.mutable_call_expr().mutable_args();
  args.emplace_back().mutable_ident_expr().set_name("baz");
  args.emplace_back().mutable_ident_expr().set_name("qux");
  expr = expr.mutable_select_expr().mutable_operand();
  EXPECT_EQ(expr.call_expr().function(), "bar");
  EXPECT_EQ(expr.call_expr().args().size(), 2);
  EXPECT_EQ(expr.call_expr().args()[0].ident_expr().name(), "baz");
  EXPECT_EQ(expr.call_expr().args()[1].ident_expr().name(), "qux");
}

TEST(Expr, Id) {
  Expr expr;
  EXPECT_THAT(expr.id(), Eq(0));
  expr.set_id(1);
  EXPECT_THAT(expr.id(), Eq(1));
}

}  // namespace
}  // namespace cel
