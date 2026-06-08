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

#include "extensions/comprehensions_v2_macros.h"

#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "common/expr.h"
#include "common/operators.h"
#include "internal/status_macros.h"
#include "parser/macro.h"
#include "parser/macro_expr_factory.h"
#include "parser/macro_registry.h"
#include "parser/options.h"
#include "parser/parser_interface.h"

namespace cel::extensions {

namespace {

using ::google::api::expr::common::CelOperator;

absl::optional<Expr> ExpandAllMacro2(MacroExprFactory& factory, Expr& target,
                                     absl::Span<Expr> args) {
  if (args.size() != 3) {
    return factory.ReportError("all() requires 3 arguments");
  }
  if (!args[0].has_ident_expr() || args[0].ident_expr().name().empty()) {
    return factory.ReportErrorAt(
        args[0], "all() first variable name must be a simple identifier");
  }
  if (!args[1].has_ident_expr() || args[1].ident_expr().name().empty()) {
    return factory.ReportErrorAt(
        args[1], "all() second variable name must be a simple identifier");
  }
  if (args[0].ident_expr().name() == args[1].ident_expr().name()) {
    return factory.ReportErrorAt(
        args[0],
        "all() second variable must be different from the first variable");
  }
  if (args[0].ident_expr().name() == kAccumulatorVariableName) {
    return factory.ReportErrorAt(
        args[0], absl::StrCat("all() first variable name cannot be ",
                              kAccumulatorVariableName));
  }
  if (args[1].ident_expr().name() == kAccumulatorVariableName) {
    return factory.ReportErrorAt(
        args[1], absl::StrCat("all() second variable name cannot be ",
                              kAccumulatorVariableName));
  }
  auto init = factory.NewBoolConst(true);
  auto condition =
      factory.NewCall(CelOperator::NOT_STRICTLY_FALSE, factory.NewAccuIdent());
  auto step = factory.NewCall(CelOperator::LOGICAL_AND, factory.NewAccuIdent(),
                              std::move(args[2]));
  auto result = factory.NewAccuIdent();
  return factory.NewComprehension(
      args[0].ident_expr().name(), args[1].ident_expr().name(),
      std::move(target), factory.AccuVarName(), std::move(init),
      std::move(condition), std::move(step), std::move(result));
}

Macro MakeAllMacro2() {
  auto status_or_macro = Macro::Receiver(CelOperator::ALL, 3, ExpandAllMacro2);
  ABSL_CHECK_OK(status_or_macro);  // Crash OK
  return std::move(*status_or_macro);
}

absl::optional<Expr> ExpandExistsMacro2(MacroExprFactory& factory, Expr& target,
                                        absl::Span<Expr> args) {
  if (args.size() != 3) {
    return factory.ReportError("exists() requires 3 arguments");
  }
  if (!args[0].has_ident_expr() || args[0].ident_expr().name().empty()) {
    return factory.ReportErrorAt(
        args[0], "exists() first variable name must be a simple identifier");
  }
  if (!args[1].has_ident_expr() || args[1].ident_expr().name().empty()) {
    return factory.ReportErrorAt(
        args[1], "exists() second variable name must be a simple identifier");
  }
  if (args[0].ident_expr().name() == args[1].ident_expr().name()) {
    return factory.ReportErrorAt(
        args[0],
        "exists() second variable must be different from the first variable");
  }
  if (args[0].ident_expr().name() == kAccumulatorVariableName) {
    return factory.ReportErrorAt(
        args[0], absl::StrCat("exists() first variable name cannot be ",
                              kAccumulatorVariableName));
  }
  if (args[1].ident_expr().name() == kAccumulatorVariableName) {
    return factory.ReportErrorAt(
        args[1], absl::StrCat("exists() second variable name cannot be ",
                              kAccumulatorVariableName));
  }
  auto init = factory.NewBoolConst(false);
  auto condition = factory.NewCall(
      CelOperator::NOT_STRICTLY_FALSE,
      factory.NewCall(CelOperator::LOGICAL_NOT, factory.NewAccuIdent()));
  auto step = factory.NewCall(CelOperator::LOGICAL_OR, factory.NewAccuIdent(),
                              std::move(args[2]));
  auto result = factory.NewAccuIdent();
  return factory.NewComprehension(
      args[0].ident_expr().name(), args[1].ident_expr().name(),
      std::move(target), factory.AccuVarName(), std::move(init),
      std::move(condition), std::move(step), std::move(result));
}

Macro MakeExistsMacro2() {
  auto status_or_macro =
      Macro::Receiver(CelOperator::EXISTS, 3, ExpandExistsMacro2);
  ABSL_CHECK_OK(status_or_macro);  // Crash OK
  return std::move(*status_or_macro);
}

absl::optional<Expr> ExpandExistsOneMacro2(MacroExprFactory& factory,
                                           Expr& target,
                                           absl::Span<Expr> args) {
  if (args.size() != 3) {
    return factory.ReportError("existsOne() requires 3 arguments");
  }
  if (!args[0].has_ident_expr() || args[0].ident_expr().name().empty()) {
    return factory.ReportErrorAt(
        args[0], "existsOne() first variable name must be a simple identifier");
  }
  if (!args[1].has_ident_expr() || args[1].ident_expr().name().empty()) {
    return factory.ReportErrorAt(
        args[1],
        "existsOne() second variable name must be a simple identifier");
  }
  if (args[0].ident_expr().name() == args[1].ident_expr().name()) {
    return factory.ReportErrorAt(
        args[0],
        "existsOne() second variable must be different "
        "from the first variable");
  }
  if (args[0].ident_expr().name() == kAccumulatorVariableName) {
    return factory.ReportErrorAt(
        args[0], absl::StrCat("existsOne() first variable name cannot be ",
                              kAccumulatorVariableName));
  }
  if (args[1].ident_expr().name() == kAccumulatorVariableName) {
    return factory.ReportErrorAt(
        args[1], absl::StrCat("existsOne() second variable name cannot be ",
                              kAccumulatorVariableName));
  }
  auto init = factory.NewIntConst(0);
  auto condition = factory.NewBoolConst(true);
  auto step =
      factory.NewCall(CelOperator::CONDITIONAL, std::move(args[2]),
                      factory.NewCall(CelOperator::ADD, factory.NewAccuIdent(),
                                      factory.NewIntConst(1)),
                      factory.NewAccuIdent());
  auto result = factory.NewCall(CelOperator::EQUALS, factory.NewAccuIdent(),
                                factory.NewIntConst(1));
  return factory.NewComprehension(
      args[0].ident_expr().name(), args[1].ident_expr().name(),
      std::move(target), factory.AccuVarName(), std::move(init),
      std::move(condition), std::move(step), std::move(result));
}

Macro MakeExistsOneMacro2() {
  auto status_or_macro = Macro::Receiver("existsOne", 3, ExpandExistsOneMacro2);
  ABSL_CHECK_OK(status_or_macro);  // Crash OK
  return std::move(*status_or_macro);
}

absl::optional<Expr> ExpandTransformList3Macro(MacroExprFactory& factory,
                                               Expr& target,
                                               absl::Span<Expr> args) {
  if (args.size() != 3) {
    return factory.ReportError("transformList() requires 3 arguments");
  }
  if (!args[0].has_ident_expr() || args[0].ident_expr().name().empty()) {
    return factory.ReportErrorAt(
        args[0],
        "transformList() first variable name must be a simple identifier");
  }
  if (!args[1].has_ident_expr() || args[1].ident_expr().name().empty()) {
    return factory.ReportErrorAt(
        args[1],
        "transformList() second variable name must be a simple identifier");
  }
  if (args[0].ident_expr().name() == args[1].ident_expr().name()) {
    return factory.ReportErrorAt(args[0],
                                 "transformList() second variable must be "
                                 "different from the first variable");
  }
  if (args[0].ident_expr().name() == kAccumulatorVariableName) {
    return factory.ReportErrorAt(
        args[0], absl::StrCat("transformList() first variable name cannot be ",
                              kAccumulatorVariableName));
  }
  if (args[1].ident_expr().name() == kAccumulatorVariableName) {
    return factory.ReportErrorAt(
        args[1], absl::StrCat("transformList() second variable name cannot be ",
                              kAccumulatorVariableName));
  }
  std::string iter_var = args[0].ident_expr().name();
  std::string iter_var2 = args[1].ident_expr().name();
  Expr step = factory.NewCall(
      CelOperator::ADD, factory.NewAccuIdent(),
      factory.NewList(factory.NewListElement(std::move(args[2]))));
  return factory.NewComprehension(std::move(iter_var), std::move(iter_var2),
                                  std::move(target), factory.AccuVarName(),
                                  factory.NewList(), factory.NewBoolConst(true),
                                  std::move(step), factory.NewAccuIdent());
}

Macro MakeTransformList3Macro() {
  auto status_or_macro =
      Macro::Receiver("transformList", 3, ExpandTransformList3Macro);
  ABSL_CHECK_OK(status_or_macro);  // Crash OK
  return std::move(*status_or_macro);
}

absl::optional<Expr> ExpandTransformList4Macro(MacroExprFactory& factory,
                                               Expr& target,
                                               absl::Span<Expr> args) {
  if (args.size() != 4) {
    return factory.ReportError("transformList() requires 4 arguments");
  }
  if (!args[0].has_ident_expr() || args[0].ident_expr().name().empty()) {
    return factory.ReportErrorAt(
        args[0],
        "transformList() first variable name must be a simple identifier");
  }
  if (!args[1].has_ident_expr() || args[1].ident_expr().name().empty()) {
    return factory.ReportErrorAt(
        args[1],
        "transformList() second variable name must be a simple identifier");
  }
  if (args[0].ident_expr().name() == args[1].ident_expr().name()) {
    return factory.ReportErrorAt(args[0],
                                 "transformList() second variable must be "
                                 "different from the first variable");
  }
  if (args[0].ident_expr().name() == kAccumulatorVariableName) {
    return factory.ReportErrorAt(
        args[0], absl::StrCat("transformList() first variable name cannot be ",
                              kAccumulatorVariableName));
  }
  if (args[1].ident_expr().name() == kAccumulatorVariableName) {
    return factory.ReportErrorAt(
        args[1], absl::StrCat("transformList() second variable name cannot be ",
                              kAccumulatorVariableName));
  }
  std::string iter_var = args[0].ident_expr().name();
  std::string iter_var2 = args[1].ident_expr().name();
  Expr step = factory.NewCall(
      CelOperator::ADD, factory.NewAccuIdent(),
      factory.NewList(factory.NewListElement(std::move(args[3]))));
  step = factory.NewCall(CelOperator::CONDITIONAL, std::move(args[2]),
                         std::move(step), factory.NewAccuIdent());
  return factory.NewComprehension(std::move(iter_var), std::move(iter_var2),
                                  std::move(target), factory.AccuVarName(),
                                  factory.NewList(), factory.NewBoolConst(true),
                                  std::move(step), factory.NewAccuIdent());
}

Macro MakeTransformList4Macro() {
  auto status_or_macro =
      Macro::Receiver("transformList", 4, ExpandTransformList4Macro);
  ABSL_CHECK_OK(status_or_macro);  // Crash OK
  return std::move(*status_or_macro);
}

absl::optional<Expr> ExpandTransformMap3Macro(MacroExprFactory& factory,
                                              Expr& target,
                                              absl::Span<Expr> args) {
  if (args.size() != 3) {
    return factory.ReportError("transformMap() requires 3 arguments");
  }
  if (!args[0].has_ident_expr() || args[0].ident_expr().name().empty()) {
    return factory.ReportErrorAt(
        args[0],
        "transformMap() first variable name must be a simple identifier");
  }
  if (!args[1].has_ident_expr() || args[1].ident_expr().name().empty()) {
    return factory.ReportErrorAt(
        args[1],
        "transformMap() second variable name must be a simple identifier");
  }
  if (args[0].ident_expr().name() == args[1].ident_expr().name()) {
    return factory.ReportErrorAt(args[0],
                                 "transformMap() second variable must be "
                                 "different from the first variable");
  }
  if (args[0].ident_expr().name() == kAccumulatorVariableName) {
    return factory.ReportErrorAt(
        args[0], absl::StrCat("transformMap() first variable name cannot be ",
                              kAccumulatorVariableName));
  }
  if (args[1].ident_expr().name() == kAccumulatorVariableName) {
    return factory.ReportErrorAt(
        args[1], absl::StrCat("transformMap() second variable name cannot be ",
                              kAccumulatorVariableName));
  }
  std::string iter_var = args[0].ident_expr().name();
  std::string iter_var2 = args[1].ident_expr().name();
  Expr step = factory.NewCall("cel.@mapInsert", factory.NewAccuIdent(),
                              std::move(args[0]), std::move(args[2]));
  return factory.NewComprehension(std::move(iter_var), std::move(iter_var2),
                                  std::move(target), factory.AccuVarName(),
                                  factory.NewMap(), factory.NewBoolConst(true),
                                  std::move(step), factory.NewAccuIdent());
}

Macro MakeTransformMap3Macro() {
  auto status_or_macro =
      Macro::Receiver("transformMap", 3, ExpandTransformMap3Macro);
  ABSL_CHECK_OK(status_or_macro);  // Crash OK
  return std::move(*status_or_macro);
}

absl::optional<Expr> ExpandTransformMap4Macro(MacroExprFactory& factory,
                                              Expr& target,
                                              absl::Span<Expr> args) {
  if (args.size() != 4) {
    return factory.ReportError("transformMap() requires 4 arguments");
  }
  if (!args[0].has_ident_expr() || args[0].ident_expr().name().empty()) {
    return factory.ReportErrorAt(
        args[0],
        "transformMap() first variable name must be a simple identifier");
  }
  if (!args[1].has_ident_expr() || args[1].ident_expr().name().empty()) {
    return factory.ReportErrorAt(
        args[1],
        "transformMap() second variable name must be a simple identifier");
  }
  if (args[0].ident_expr().name() == args[1].ident_expr().name()) {
    return factory.ReportErrorAt(args[0],
                                 "transformMap() second variable must be "
                                 "different from the first variable");
  }
  if (args[0].ident_expr().name() == kAccumulatorVariableName) {
    return factory.ReportErrorAt(
        args[0], absl::StrCat("transformMap() first variable name cannot be ",
                              kAccumulatorVariableName));
  }
  if (args[1].ident_expr().name() == kAccumulatorVariableName) {
    return factory.ReportErrorAt(
        args[1], absl::StrCat("transformMap() second variable name cannot be ",
                              kAccumulatorVariableName));
  }
  std::string iter_var = args[0].ident_expr().name();
  std::string iter_var2 = args[1].ident_expr().name();
  Expr step = factory.NewCall("cel.@mapInsert", factory.NewAccuIdent(),
                              std::move(args[0]), std::move(args[3]));
  step = factory.NewCall(CelOperator::CONDITIONAL, std::move(args[2]),
                         std::move(step), factory.NewAccuIdent());
  return factory.NewComprehension(std::move(iter_var), std::move(iter_var2),
                                  std::move(target), factory.AccuVarName(),
                                  factory.NewMap(), factory.NewBoolConst(true),
                                  std::move(step), factory.NewAccuIdent());
}

Macro MakeTransformMap4Macro() {
  auto status_or_macro =
      Macro::Receiver("transformMap", 4, ExpandTransformMap4Macro);
  ABSL_CHECK_OK(status_or_macro);  // Crash OK
  return std::move(*status_or_macro);
}

absl::optional<Expr> ExpandTransformMapEntry3Macro(MacroExprFactory& factory,
                                                   Expr& target,
                                                   absl::Span<Expr> args) {
  if (args.size() != 3) {
    return factory.ReportError("transformMapEntry() requires 3 arguments");
  }
  if (!args[0].has_ident_expr() || args[0].ident_expr().name().empty()) {
    return factory.ReportErrorAt(
        args[0],
        "transformMapEntry() first variable name must be a simple identifier");
  }
  if (!args[1].has_ident_expr() || args[1].ident_expr().name().empty()) {
    return factory.ReportErrorAt(
        args[1],
        "transformMapEntry() second variable name must be a simple identifier");
  }
  if (args[0].ident_expr().name() == args[1].ident_expr().name()) {
    return factory.ReportErrorAt(args[0],
                                 "transformMapEntry() second variable must be "
                                 "different from the first variable");
  }
  if (args[0].ident_expr().name() == kAccumulatorVariableName) {
    return factory.ReportErrorAt(
        args[0],
        absl::StrCat("transformMapEntry() first variable name cannot be ",
                     kAccumulatorVariableName));
  }
  if (args[1].ident_expr().name() == kAccumulatorVariableName) {
    return factory.ReportErrorAt(
        args[1],
        absl::StrCat("transformMapEntry() second variable name cannot be ",
                     kAccumulatorVariableName));
  }
  std::string iter_var = args[0].ident_expr().name();
  std::string iter_var2 = args[1].ident_expr().name();
  Expr step = factory.NewCall("cel.@mapInsert", factory.NewAccuIdent(),
                              std::move(args[2]));
  return factory.NewComprehension(std::move(iter_var), std::move(iter_var2),
                                  std::move(target), factory.AccuVarName(),
                                  factory.NewMap(), factory.NewBoolConst(true),
                                  std::move(step), factory.NewAccuIdent());
}

Macro MakeTransformMap3EntryMacro() {
  auto status_or_macro =
      Macro::Receiver("transformMapEntry", 3, ExpandTransformMapEntry3Macro);
  ABSL_CHECK_OK(status_or_macro);  // Crash OK
  return std::move(*status_or_macro);
}

absl::optional<Expr> ExpandTransformMapEntry4Macro(MacroExprFactory& factory,
                                                   Expr& target,
                                                   absl::Span<Expr> args) {
  if (args.size() != 4) {
    return factory.ReportError("transformMapEntry() requires 4 arguments");
  }
  if (!args[0].has_ident_expr() || args[0].ident_expr().name().empty()) {
    return factory.ReportErrorAt(
        args[0],
        "transformMapEntry() first variable name must be a simple identifier");
  }
  if (!args[1].has_ident_expr() || args[1].ident_expr().name().empty()) {
    return factory.ReportErrorAt(
        args[1],
        "transformMapEntry() second variable name must be a simple identifier");
  }
  if (args[0].ident_expr().name() == args[1].ident_expr().name()) {
    return factory.ReportErrorAt(args[0],
                                 "transformMapEntry() second variable must be "
                                 "different from the first variable");
  }
  if (args[0].ident_expr().name() == kAccumulatorVariableName) {
    return factory.ReportErrorAt(
        args[0],
        absl::StrCat("transformMapEntry() first variable name cannot be ",
                     kAccumulatorVariableName));
  }
  if (args[1].ident_expr().name() == kAccumulatorVariableName) {
    return factory.ReportErrorAt(
        args[1],
        absl::StrCat("transformMapEntry() second variable name cannot be ",
                     kAccumulatorVariableName));
  }
  std::string iter_var = args[0].ident_expr().name();
  std::string iter_var2 = args[1].ident_expr().name();
  Expr step = factory.NewCall("cel.@mapInsert", factory.NewAccuIdent(),
                              std::move(args[3]));
  step = factory.NewCall(CelOperator::CONDITIONAL, std::move(args[2]),
                         std::move(step), factory.NewAccuIdent());
  return factory.NewComprehension(std::move(iter_var), std::move(iter_var2),
                                  std::move(target), factory.AccuVarName(),
                                  factory.NewMap(), factory.NewBoolConst(true),
                                  std::move(step), factory.NewAccuIdent());
}

Macro MakeTransformMapEntry4Macro() {
  auto status_or_macro =
      Macro::Receiver("transformMapEntry", 4, ExpandTransformMapEntry4Macro);
  ABSL_CHECK_OK(status_or_macro);  // Crash OK
  return std::move(*status_or_macro);
}

const Macro& AllMacro2() {
  static const absl::NoDestructor<Macro> macro(MakeAllMacro2());
  return *macro;
}

const Macro& ExistsMacro2() {
  static const absl::NoDestructor<Macro> macro(MakeExistsMacro2());
  return *macro;
}

const Macro& ExistsOneMacro2() {
  static const absl::NoDestructor<Macro> macro(MakeExistsOneMacro2());
  return *macro;
}

const Macro& TransformList3Macro() {
  static const absl::NoDestructor<Macro> macro(MakeTransformList3Macro());
  return *macro;
}

const Macro& TransformList4Macro() {
  static const absl::NoDestructor<Macro> macro(MakeTransformList4Macro());
  return *macro;
}

const Macro& TransformMap3Macro() {
  static const absl::NoDestructor<Macro> macro(MakeTransformMap3Macro());
  return *macro;
}

const Macro& TransformMap4Macro() {
  static const absl::NoDestructor<Macro> macro(MakeTransformMap4Macro());
  return *macro;
}

const Macro& TransformMapEntry3Macro() {
  static const absl::NoDestructor<Macro> macro(MakeTransformMap3EntryMacro());
  return *macro;
}

const Macro& TransformMapEntry4Macro() {
  static const absl::NoDestructor<Macro> macro(MakeTransformMapEntry4Macro());
  return *macro;
}

}  // namespace

std::vector<Macro> AllMacros() {
  return {AllMacro2(),
          ExistsMacro2(),
          ExistsOneMacro2(),
          TransformList3Macro(),
          TransformList4Macro(),
          TransformMap3Macro(),
          TransformMap4Macro(),
          TransformMapEntry3Macro(),
          TransformMapEntry4Macro()};
}

// Registers the macros defined by the comprehension v2 extension.
absl::Status RegisterComprehensionsV2Macros(MacroRegistry& registry,
                                            const ParserOptions&) {
  for (const Macro& macro : AllMacros()) {
    CEL_RETURN_IF_ERROR(registry.RegisterMacro(macro));
  }

  return absl::OkStatus();
}

absl::Status RegisterComprehensionsV2Macros(ParserBuilder& parser_builder) {
  for (const Macro& macro : AllMacros()) {
    CEL_RETURN_IF_ERROR(parser_builder.AddMacro(macro));
  }

  return absl::OkStatus();
}

}  // namespace cel::extensions
