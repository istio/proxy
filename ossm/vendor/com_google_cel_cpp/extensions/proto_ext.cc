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

#include "extensions/proto_ext.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/functional/overload.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "common/ast.h"
#include "parser/macro.h"
#include "parser/macro_expr_factory.h"

namespace cel::extensions {

namespace {

static constexpr char kProtoNamespace[] = "proto";
static constexpr char kGetExt[] = "getExt";
static constexpr char kHasExt[] = "hasExt";

absl::optional<std::string> ValidateExtensionIdentifier(const Expr& expr) {
  return absl::visit(
      absl::Overload(
          [](const SelectExpr& select_expr) -> absl::optional<std::string> {
            if (select_expr.test_only()) {
              return absl::nullopt;
            }
            auto op_name = ValidateExtensionIdentifier(select_expr.operand());
            if (!op_name.has_value()) {
              return absl::nullopt;
            }
            return absl::StrCat(*op_name, ".", select_expr.field());
          },
          [](const IdentExpr& ident_expr) -> absl::optional<std::string> {
            return ident_expr.name();
          },
          [](const auto&) -> absl::optional<std::string> {
            return absl::nullopt;
          }),
      expr.kind());
}

absl::optional<std::string> GetExtensionFieldName(const Expr& expr) {
  if (const auto* select_expr =
          expr.has_select_expr() ? &expr.select_expr() : nullptr;
      select_expr) {
    return ValidateExtensionIdentifier(expr);
  }
  return absl::nullopt;
}

bool IsExtensionCall(const Expr& target) {
  if (const auto* ident_expr =
          target.has_ident_expr() ? &target.ident_expr() : nullptr;
      ident_expr) {
    return ident_expr->name() == kProtoNamespace;
  }
  return false;
}

}  // namespace

std::vector<Macro> proto_macros() {
  absl::StatusOr<Macro> getExt = Macro::Receiver(
      kGetExt, 2,
      [](MacroExprFactory& factory, Expr& target,
         absl::Span<Expr> arguments) -> absl::optional<Expr> {
        if (!IsExtensionCall(target)) {
          return absl::nullopt;
        }
        auto extFieldName = GetExtensionFieldName(arguments[1]);
        if (!extFieldName.has_value()) {
          return factory.ReportErrorAt(arguments[1], "invalid extension field");
        }
        return factory.NewSelect(std::move(arguments[0]),
                                 std::move(*extFieldName));
      });
  absl::StatusOr<Macro> hasExt = Macro::Receiver(
      kHasExt, 2,
      [](MacroExprFactory& factory, Expr& target,
         absl::Span<Expr> arguments) -> absl::optional<Expr> {
        if (!IsExtensionCall(target)) {
          return absl::nullopt;
        }
        auto extFieldName = GetExtensionFieldName(arguments[1]);
        if (!extFieldName.has_value()) {
          return factory.ReportErrorAt(arguments[1], "invalid extension field");
        }
        return factory.NewPresenceTest(std::move(arguments[0]),
                                       std::move(*extFieldName));
      });
  return {*hasExt, *getExt};
}

}  // namespace cel::extensions
