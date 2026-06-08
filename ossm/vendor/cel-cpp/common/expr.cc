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
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/functional/overload.h"
#include "absl/types/variant.h"
#include "common/constant.h"

namespace cel {

namespace {

struct CopyStackRecord {
  const Expr* src;
  Expr* dst;
};

void CopyNode(CopyStackRecord element, std::vector<CopyStackRecord>& stack) {
  const Expr* src = element.src;
  Expr* dst = element.dst;
  dst->set_id(src->id());
  absl::visit(
      absl::Overload(
          [=](const UnspecifiedExpr&) {
            dst->mutable_kind().emplace<UnspecifiedExpr>();
          },
          [=](const IdentExpr& i) {
            dst->mutable_ident_expr().set_name(i.name());
          },
          [=](const Constant& c) { dst->mutable_const_expr() = c; },
          [&](const SelectExpr& s) {
            dst->mutable_select_expr().set_field(s.field());
            dst->mutable_select_expr().set_test_only(s.test_only());

            if (s.has_operand()) {
              stack.push_back({&s.operand(),
                               &dst->mutable_select_expr().mutable_operand()});
            }
          },
          [&](const CallExpr& c) {
            dst->mutable_call_expr().set_function(c.function());
            if (c.has_target()) {
              stack.push_back(
                  {&c.target(), &dst->mutable_call_expr().mutable_target()});
            }
            dst->mutable_call_expr().mutable_args().resize(c.args().size());
            for (int i = 0; i < dst->mutable_call_expr().mutable_args().size();
                 ++i) {
              stack.push_back(
                  {&c.args()[i], &dst->mutable_call_expr().mutable_args()[i]});
            }
          },
          [&](const ListExpr& c) {
            auto& dst_list = dst->mutable_list_expr();
            dst_list.mutable_elements().resize(c.elements().size());
            for (int i = 0; i < src->list_expr().elements().size(); ++i) {
              dst_list.mutable_elements()[i].set_optional(
                  c.elements()[i].optional());
              stack.push_back({&c.elements()[i].expr(),
                               &dst_list.mutable_elements()[i].mutable_expr()});
            }
          },
          [&](const StructExpr& s) {
            auto& dst_struct = dst->mutable_struct_expr();
            dst_struct.mutable_fields().resize(s.fields().size());
            dst_struct.set_name(s.name());
            for (int i = 0; i < s.fields().size(); ++i) {
              dst_struct.mutable_fields()[i].set_optional(
                  s.fields()[i].optional());
              dst_struct.mutable_fields()[i].set_name(s.fields()[i].name());
              dst_struct.mutable_fields()[i].set_id(s.fields()[i].id());
              stack.push_back(
                  {&s.fields()[i].value(),
                   &dst_struct.mutable_fields()[i].mutable_value()});
            }
          },
          [&](const MapExpr& c) {
            auto& dst_map = dst->mutable_map_expr();
            dst_map.mutable_entries().resize(c.entries().size());
            for (int i = 0; i < c.entries().size(); ++i) {
              dst_map.mutable_entries()[i].set_optional(
                  c.entries()[i].optional());
              dst_map.mutable_entries()[i].set_id(c.entries()[i].id());
              stack.push_back({&c.entries()[i].key(),
                               &dst_map.mutable_entries()[i].mutable_key()});
              stack.push_back({&c.entries()[i].value(),
                               &dst_map.mutable_entries()[i].mutable_value()});
            }
          },
          [&](const ComprehensionExpr& c) {
            auto& dst_comprehension = dst->mutable_comprehension_expr();
            dst_comprehension.set_iter_var(c.iter_var());
            dst_comprehension.set_iter_var2(c.iter_var2());
            dst_comprehension.set_accu_var(c.accu_var());
            if (c.has_accu_init()) {
              stack.push_back(
                  {&c.accu_init(), &dst_comprehension.mutable_accu_init()});
            }
            if (c.has_iter_range()) {
              stack.push_back(
                  {&c.iter_range(), &dst_comprehension.mutable_iter_range()});
            }
            if (c.has_loop_condition()) {
              stack.push_back({&c.loop_condition(),
                               &dst_comprehension.mutable_loop_condition()});
            }
            if (c.has_loop_step()) {
              stack.push_back(
                  {&c.loop_step(), &dst_comprehension.mutable_loop_step()});
            }
            if (c.has_result()) {
              stack.push_back(
                  {&c.result(), &dst_comprehension.mutable_result()});
            }
          }),
      src->kind());
}

void CloneImpl(const Expr& expr, Expr& dst) {
  std::vector<CopyStackRecord> stack;
  stack.push_back({&expr, &dst});
  while (!stack.empty()) {
    CopyStackRecord element = stack.back();
    stack.pop_back();
    CopyNode(element, stack);
  }
}

}  // namespace

const UnspecifiedExpr& UnspecifiedExpr::default_instance() {
  static const absl::NoDestructor<UnspecifiedExpr> instance;
  return *instance;
}

const IdentExpr& IdentExpr::default_instance() {
  static const absl::NoDestructor<IdentExpr> instance;
  return *instance;
}

const SelectExpr& SelectExpr::default_instance() {
  static const absl::NoDestructor<SelectExpr> instance;
  return *instance;
}

const CallExpr& CallExpr::default_instance() {
  static const absl::NoDestructor<CallExpr> instance;
  return *instance;
}

const ListExpr& ListExpr::default_instance() {
  static const absl::NoDestructor<ListExpr> instance;
  return *instance;
}

const StructExpr& StructExpr::default_instance() {
  static const absl::NoDestructor<StructExpr> instance;
  return *instance;
}

const MapExpr& MapExpr::default_instance() {
  static const absl::NoDestructor<MapExpr> instance;
  return *instance;
}

const ComprehensionExpr& ComprehensionExpr::default_instance() {
  static const absl::NoDestructor<ComprehensionExpr> instance;
  return *instance;
}

const Expr& Expr::default_instance() {
  static const absl::NoDestructor<Expr> instance;
  return *instance;
}

Expr& Expr::operator=(const Expr& other) {
  if (this == &other) {
    return *this;
  }
  Expr tmp;
  CloneImpl(other, tmp);
  *this = std::move(tmp);
  return *this;
}

Expr::Expr(const Expr& other) { CloneImpl(other, *this); }

}  // namespace cel
