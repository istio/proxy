// Copyright 2023 Google LLC
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

#include "extensions/math_ext_macros.h"

#include <utility>
#include <vector>

#include "absl/functional/overload.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "common/ast.h"
#include "common/constant.h"
#include "parser/macro.h"
#include "parser/macro_expr_factory.h"

namespace cel::extensions {

namespace {

static constexpr absl::string_view kMathNamespace = "math";
static constexpr absl::string_view kLeast = "least";
static constexpr absl::string_view kGreatest = "greatest";

static constexpr char kMathMin[] = "math.@min";
static constexpr char kMathMax[] = "math.@max";

bool IsTargetNamespace(const Expr &target) {
  return target.has_ident_expr() &&
         target.ident_expr().name() == kMathNamespace;
}

bool IsValidArgType(const Expr &arg) {
  return absl::visit(
      absl::Overload([](const UnspecifiedExpr &) -> bool { return false; },
                     [](const Constant &const_expr) -> bool {
                       return const_expr.has_double_value() ||
                              const_expr.has_int_value() ||
                              const_expr.has_uint_value();
                     },
                     [](const ListExpr &) -> bool { return false; },
                     [](const StructExpr &) -> bool { return false; },
                     [](const MapExpr &) -> bool { return false; },
                     // This is intended for call and select expressions.
                     [](const auto &) -> bool { return true; }),
      arg.kind());
}

absl::optional<Expr> CheckInvalidArgs(MacroExprFactory &factory,
                                      absl::string_view macro,
                                      absl::Span<const Expr> arguments) {
  for (const auto &argument : arguments) {
    if (!IsValidArgType(argument)) {
      return factory.ReportErrorAt(
          argument,
          absl::StrCat(macro, " simple literal arguments must be numeric"));
    }
  }

  return absl::nullopt;
}

bool IsListLiteralWithValidArgs(const Expr &arg) {
  if (const auto *list_expr = arg.has_list_expr() ? &arg.list_expr() : nullptr;
      list_expr) {
    if (list_expr->elements().empty()) {
      return false;
    }
    for (const auto &element : list_expr->elements()) {
      if (!IsValidArgType(element.expr())) {
        return false;
      }
    }
    return true;
  }
  return false;
}

}  // namespace

std::vector<Macro> math_macros() {
  absl::StatusOr<Macro> least = Macro::ReceiverVarArg(
      kLeast,
      [](MacroExprFactory &factory, Expr &target,
         absl::Span<Expr> arguments) -> absl::optional<Expr> {
        if (!IsTargetNamespace(target)) {
          return absl::nullopt;
        }

        switch (arguments.size()) {
          case 0:
            return factory.ReportErrorAt(
                target, "math.least() requires at least one argument.");
          case 1: {
            if (!IsListLiteralWithValidArgs(arguments[0]) &&
                !IsValidArgType(arguments[0])) {
              return factory.ReportErrorAt(
                  arguments[0], "math.least() invalid single argument value.");
            }

            return factory.NewCall(kMathMin, arguments);
          }
          case 2: {
            if (auto error =
                    CheckInvalidArgs(factory, "math.least()", arguments);
                error) {
              return std::move(*error);
            }
            return factory.NewCall(kMathMin, arguments);
          }
          default:
            if (auto error =
                    CheckInvalidArgs(factory, "math.least()", arguments);
                error) {
              return std::move(*error);
            }
            std::vector<ListExprElement> elements;
            elements.reserve(arguments.size());
            for (auto &argument : arguments) {
              elements.push_back(factory.NewListElement(std::move(argument)));
            }
            return factory.NewCall(kMathMin,
                                   factory.NewList(std::move(elements)));
        }
      });
  absl::StatusOr<Macro> greatest = Macro::ReceiverVarArg(
      kGreatest,
      [](MacroExprFactory &factory, Expr &target,
         absl::Span<Expr> arguments) -> absl::optional<Expr> {
        if (!IsTargetNamespace(target)) {
          return absl::nullopt;
        }

        switch (arguments.size()) {
          case 0: {
            return factory.ReportErrorAt(
                target, "math.greatest() requires at least one argument.");
          }
          case 1: {
            if (!IsListLiteralWithValidArgs(arguments[0]) &&
                !IsValidArgType(arguments[0])) {
              return factory.ReportErrorAt(
                  arguments[0],
                  "math.greatest() invalid single argument value.");
            }

            return factory.NewCall(kMathMax, arguments);
          }
          case 2: {
            if (auto error =
                    CheckInvalidArgs(factory, "math.greatest()", arguments);
                error) {
              return std::move(*error);
            }
            return factory.NewCall(kMathMax, arguments);
          }
          default: {
            if (auto error =
                    CheckInvalidArgs(factory, "math.greatest()", arguments);
                error) {
              return std::move(*error);
            }
            std::vector<ListExprElement> elements;
            elements.reserve(arguments.size());
            for (auto &argument : arguments) {
              elements.push_back(factory.NewListElement(std::move(argument)));
            }
            return factory.NewCall(kMathMax,
                                   factory.NewList(std::move(elements)));
          }
        }
      });

  return {*least, *greatest};
}

}  // namespace cel::extensions
