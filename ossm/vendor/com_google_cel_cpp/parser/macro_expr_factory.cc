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

#include "parser/macro_expr_factory.h"

#include <utility>
#include <vector>

#include "absl/functional/overload.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "common/constant.h"
#include "common/expr.h"

namespace cel {

Expr MacroExprFactory::Copy(const Expr& expr) {
  // Copying logic is recursive at the moment, we alter it to be iterative in
  // the future.
  return absl::visit(
      absl::Overload(
          [this, &expr](const UnspecifiedExpr&) -> Expr {
            return NewUnspecified(CopyId(expr));
          },
          [this, &expr](const Constant& const_expr) -> Expr {
            return NewConst(CopyId(expr), const_expr);
          },
          [this, &expr](const IdentExpr& ident_expr) -> Expr {
            return NewIdent(CopyId(expr), ident_expr.name());
          },
          [this, &expr](const SelectExpr& select_expr) -> Expr {
            const auto id = CopyId(expr);
            return select_expr.test_only()
                       ? NewPresenceTest(id, Copy(select_expr.operand()),
                                         select_expr.field())
                       : NewSelect(id, Copy(select_expr.operand()),
                                   select_expr.field());
          },
          [this, &expr](const CallExpr& call_expr) -> Expr {
            const auto id = CopyId(expr);
            absl::optional<Expr> target;
            if (call_expr.has_target()) {
              target = Copy(call_expr.target());
            }
            std::vector<Expr> args;
            args.reserve(call_expr.args().size());
            for (const auto& arg : call_expr.args()) {
              args.push_back(Copy(arg));
            }
            return target.has_value()
                       ? NewMemberCall(id, call_expr.function(),
                                       std::move(*target), std::move(args))
                       : NewCall(id, call_expr.function(), std::move(args));
          },
          [this, &expr](const ListExpr& list_expr) -> Expr {
            const auto id = CopyId(expr);
            std::vector<ListExprElement> elements;
            elements.reserve(list_expr.elements().size());
            for (const auto& element : list_expr.elements()) {
              elements.push_back(Copy(element));
            }
            return NewList(id, std::move(elements));
          },
          [this, &expr](const StructExpr& struct_expr) -> Expr {
            const auto id = CopyId(expr);
            std::vector<StructExprField> fields;
            fields.reserve(struct_expr.fields().size());
            for (const auto& field : struct_expr.fields()) {
              fields.push_back(Copy(field));
            }
            return NewStruct(id, struct_expr.name(), std::move(fields));
          },
          [this, &expr](const MapExpr& map_expr) -> Expr {
            const auto id = CopyId(expr);
            std::vector<MapExprEntry> entries;
            entries.reserve(map_expr.entries().size());
            for (const auto& entry : map_expr.entries()) {
              entries.push_back(Copy(entry));
            }
            return NewMap(id, std::move(entries));
          },
          [this, &expr](const ComprehensionExpr& comprehension_expr) -> Expr {
            const auto id = CopyId(expr);
            auto iter_range = Copy(comprehension_expr.iter_range());
            auto accu_init = Copy(comprehension_expr.accu_init());
            auto loop_condition = Copy(comprehension_expr.loop_condition());
            auto loop_step = Copy(comprehension_expr.loop_step());
            auto result = Copy(comprehension_expr.result());
            return NewComprehension(
                id, comprehension_expr.iter_var(), std::move(iter_range),
                comprehension_expr.accu_var(), std::move(accu_init),
                std::move(loop_condition), std::move(loop_step),
                std::move(result));
          }),
      expr.kind());
}

ListExprElement MacroExprFactory::Copy(const ListExprElement& element) {
  return NewListElement(Copy(element.expr()), element.optional());
}

StructExprField MacroExprFactory::Copy(const StructExprField& field) {
  auto field_id = CopyId(field.id());
  auto field_value = Copy(field.value());
  return NewStructField(field_id, field.name(), std::move(field_value),
                        field.optional());
}

MapExprEntry MacroExprFactory::Copy(const MapExprEntry& entry) {
  auto entry_id = CopyId(entry.id());
  auto entry_key = Copy(entry.key());
  auto entry_value = Copy(entry.value());
  return NewMapEntry(entry_id, std::move(entry_key), std::move(entry_value),
                     entry.optional());
}

}  // namespace cel
