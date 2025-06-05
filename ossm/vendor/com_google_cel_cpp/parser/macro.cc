// Copyright 2021 Google LLC
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

#include "parser/macro.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "common/expr.h"
#include "common/operators.h"
#include "internal/lexis.h"
#include "parser/macro_expr_factory.h"

namespace cel {

namespace {

using google::api::expr::common::CelOperator;

inline MacroExpander ToMacroExpander(GlobalMacroExpander expander) {
  ABSL_DCHECK(expander);
  return [expander = std::move(expander)](
             MacroExprFactory& factory,
             absl::optional<std::reference_wrapper<Expr>> target,
             absl::Span<Expr> arguments) -> absl::optional<Expr> {
    ABSL_DCHECK(!target.has_value());
    return (expander)(factory, arguments);
  };
}

inline MacroExpander ToMacroExpander(ReceiverMacroExpander expander) {
  ABSL_DCHECK(expander);
  return [expander = std::move(expander)](
             MacroExprFactory& factory,
             absl::optional<std::reference_wrapper<Expr>> target,
             absl::Span<Expr> arguments) -> absl::optional<Expr> {
    ABSL_DCHECK(target.has_value());
    return (expander)(factory, *target, arguments);
  };
}

absl::optional<Expr> ExpandHasMacro(MacroExprFactory& factory,
                                    absl::Span<Expr> args) {
  if (args.size() != 1) {
    return factory.ReportError("has() requires 1 arguments");
  }
  if (!args[0].has_select_expr() || args[0].select_expr().test_only()) {
    return factory.ReportErrorAt(args[0],
                                 "has() argument must be a field selection");
  }
  return factory.NewPresenceTest(
      args[0].mutable_select_expr().release_operand(),
      args[0].mutable_select_expr().release_field());
}

Macro MakeHasMacro() {
  auto macro_or_status = Macro::Global(CelOperator::HAS, 1, ExpandHasMacro);
  ABSL_CHECK_OK(macro_or_status);  // Crash OK
  return std::move(*macro_or_status);
}

absl::optional<Expr> ExpandAllMacro(MacroExprFactory& factory, Expr& target,
                                    absl::Span<Expr> args) {
  if (args.size() != 2) {
    return factory.ReportError("all() requires 2 arguments");
  }
  if (!args[0].has_ident_expr()) {
    return factory.ReportErrorAt(
        args[0], "all() variable name must be a simple identifier");
  }
  auto init = factory.NewBoolConst(true);
  auto condition =
      factory.NewCall(CelOperator::NOT_STRICTLY_FALSE, factory.NewAccuIdent());
  auto step = factory.NewCall(CelOperator::LOGICAL_AND, factory.NewAccuIdent(),
                              std::move(args[1]));
  auto result = factory.NewAccuIdent();
  return factory.NewComprehension(args[0].ident_expr().name(),
                                  std::move(target), kAccumulatorVariableName,
                                  std::move(init), std::move(condition),
                                  std::move(step), std::move(result));
}

Macro MakeAllMacro() {
  auto status_or_macro = Macro::Receiver(CelOperator::ALL, 2, ExpandAllMacro);
  ABSL_CHECK_OK(status_or_macro);  // Crash OK
  return std::move(*status_or_macro);
}

absl::optional<Expr> ExpandExistsMacro(MacroExprFactory& factory, Expr& target,
                                       absl::Span<Expr> args) {
  if (args.size() != 2) {
    return factory.ReportError("exists() requires 2 arguments");
  }
  if (!args[0].has_ident_expr()) {
    return factory.ReportErrorAt(
        args[0], "exists() variable name must be a simple identifier");
  }
  auto init = factory.NewBoolConst(false);
  auto condition = factory.NewCall(
      CelOperator::NOT_STRICTLY_FALSE,
      factory.NewCall(CelOperator::LOGICAL_NOT, factory.NewAccuIdent()));
  auto step = factory.NewCall(CelOperator::LOGICAL_OR, factory.NewAccuIdent(),
                              std::move(args[1]));
  auto result = factory.NewAccuIdent();
  return factory.NewComprehension(args[0].ident_expr().name(),
                                  std::move(target), kAccumulatorVariableName,
                                  std::move(init), std::move(condition),
                                  std::move(step), std::move(result));
}

Macro MakeExistsMacro() {
  auto status_or_macro =
      Macro::Receiver(CelOperator::EXISTS, 2, ExpandExistsMacro);
  ABSL_CHECK_OK(status_or_macro);  // Crash OK
  return std::move(*status_or_macro);
}

absl::optional<Expr> ExpandExistsOneMacro(MacroExprFactory& factory,
                                          Expr& target, absl::Span<Expr> args) {
  if (args.size() != 2) {
    return factory.ReportError("exists_one() requires 2 arguments");
  }
  if (!args[0].has_ident_expr()) {
    return factory.ReportErrorAt(
        args[0], "exists_one() variable name must be a simple identifier");
  }
  auto init = factory.NewIntConst(0);
  auto condition = factory.NewBoolConst(true);
  auto step =
      factory.NewCall(CelOperator::CONDITIONAL, std::move(args[1]),
                      factory.NewCall(CelOperator::ADD, factory.NewAccuIdent(),
                                      factory.NewIntConst(1)),
                      factory.NewAccuIdent());
  auto result = factory.NewCall(CelOperator::EQUALS, factory.NewAccuIdent(),
                                factory.NewIntConst(1));
  return factory.NewComprehension(args[0].ident_expr().name(),
                                  std::move(target), kAccumulatorVariableName,
                                  std::move(init), std::move(condition),
                                  std::move(step), std::move(result));
}

Macro MakeExistsOneMacro() {
  auto status_or_macro =
      Macro::Receiver(CelOperator::EXISTS_ONE, 2, ExpandExistsOneMacro);
  ABSL_CHECK_OK(status_or_macro);  // Crash OK
  return std::move(*status_or_macro);
}

absl::optional<Expr> ExpandMap2Macro(MacroExprFactory& factory, Expr& target,
                                     absl::Span<Expr> args) {
  if (args.size() != 2) {
    return factory.ReportError("map() requires 2 arguments");
  }
  if (!args[0].has_ident_expr()) {
    return factory.ReportErrorAt(
        args[0], "map() variable name must be a simple identifier");
  }
  auto init = factory.NewList();
  auto condition = factory.NewBoolConst(true);
  auto step = factory.NewCall(
      CelOperator::ADD, factory.NewAccuIdent(),
      factory.NewList(factory.NewListElement(std::move(args[1]))));
  return factory.NewComprehension(args[0].ident_expr().name(),
                                  std::move(target), kAccumulatorVariableName,
                                  std::move(init), std::move(condition),
                                  std::move(step), factory.NewAccuIdent());
}

Macro MakeMap2Macro() {
  auto status_or_macro = Macro::Receiver(CelOperator::MAP, 2, ExpandMap2Macro);
  ABSL_CHECK_OK(status_or_macro);  // Crash OK
  return std::move(*status_or_macro);
}

absl::optional<Expr> ExpandMap3Macro(MacroExprFactory& factory, Expr& target,
                                     absl::Span<Expr> args) {
  if (args.size() != 3) {
    return factory.ReportError("map() requires 3 arguments");
  }
  if (!args[0].has_ident_expr()) {
    return factory.ReportErrorAt(
        args[0], "map() variable name must be a simple identifier");
  }
  auto init = factory.NewList();
  auto condition = factory.NewBoolConst(true);
  auto step = factory.NewCall(
      CelOperator::ADD, factory.NewAccuIdent(),
      factory.NewList(factory.NewListElement(std::move(args[2]))));
  step = factory.NewCall(CelOperator::CONDITIONAL, std::move(args[1]),
                         std::move(step), factory.NewAccuIdent());
  return factory.NewComprehension(args[0].ident_expr().name(),
                                  std::move(target), kAccumulatorVariableName,
                                  std::move(init), std::move(condition),
                                  std::move(step), factory.NewAccuIdent());
}

Macro MakeMap3Macro() {
  auto status_or_macro = Macro::Receiver(CelOperator::MAP, 3, ExpandMap3Macro);
  ABSL_CHECK_OK(status_or_macro);  // Crash OK
  return std::move(*status_or_macro);
}

absl::optional<Expr> ExpandFilterMacro(MacroExprFactory& factory, Expr& target,
                                       absl::Span<Expr> args) {
  if (args.size() != 2) {
    return factory.ReportError("filter() requires 2 arguments");
  }
  if (!args[0].has_ident_expr()) {
    return factory.ReportErrorAt(
        args[0], "filter() variable name must be a simple identifier");
  }
  auto name = args[0].ident_expr().name();

  auto init = factory.NewList();
  auto condition = factory.NewBoolConst(true);
  auto step = factory.NewCall(
      CelOperator::ADD, factory.NewAccuIdent(),
      factory.NewList(factory.NewListElement(std::move(args[0]))));
  step = factory.NewCall(CelOperator::CONDITIONAL, std::move(args[1]),
                         std::move(step), factory.NewAccuIdent());
  return factory.NewComprehension(std::move(name), std::move(target),
                                  kAccumulatorVariableName, std::move(init),
                                  std::move(condition), std::move(step),
                                  factory.NewAccuIdent());
}

Macro MakeFilterMacro() {
  auto status_or_macro =
      Macro::Receiver(CelOperator::FILTER, 2, ExpandFilterMacro);
  ABSL_CHECK_OK(status_or_macro);  // Crash OK
  return std::move(*status_or_macro);
}

absl::optional<Expr> ExpandOptMapMacro(MacroExprFactory& factory, Expr& target,
                                       absl::Span<Expr> args) {
  if (args.size() != 2) {
    return factory.ReportError("optMap() requires 2 arguments");
  }
  if (!args[0].has_ident_expr()) {
    return factory.ReportErrorAt(
        args[0], "optMap() variable name must be a simple identifier");
  }
  auto var_name = args[0].ident_expr().name();

  auto target_copy = factory.Copy(target);
  std::vector<Expr> call_args;
  call_args.reserve(3);
  call_args.push_back(factory.NewMemberCall("hasValue", std::move(target)));
  auto iter_range = factory.NewList();
  auto accu_init = factory.NewMemberCall("value", std::move(target_copy));
  auto condition = factory.NewBoolConst(false);
  auto fold = factory.NewComprehension(
      "#unused", std::move(iter_range), std::move(var_name),
      std::move(accu_init), std::move(condition), std::move(args[0]),
      std::move(args[1]));
  call_args.push_back(factory.NewCall("optional.of", std::move(fold)));
  call_args.push_back(factory.NewCall("optional.none"));
  return factory.NewCall(CelOperator::CONDITIONAL, std::move(call_args));
}

Macro MakeOptMapMacro() {
  auto status_or_macro = Macro::Receiver("optMap", 2, ExpandOptMapMacro);
  ABSL_CHECK_OK(status_or_macro);  // Crash OK
  return std::move(*status_or_macro);
}

absl::optional<Expr> ExpandOptFlatMapMacro(MacroExprFactory& factory,
                                           Expr& target,
                                           absl::Span<Expr> args) {
  if (args.size() != 2) {
    return factory.ReportError("optFlatMap() requires 2 arguments");
  }
  if (!args[0].has_ident_expr()) {
    return factory.ReportErrorAt(
        args[0], "optFlatMap() variable name must be a simple identifier");
  }
  auto var_name = args[0].ident_expr().name();

  auto target_copy = factory.Copy(target);
  std::vector<Expr> call_args;
  call_args.reserve(3);
  call_args.push_back(factory.NewMemberCall("hasValue", std::move(target)));
  auto iter_range = factory.NewList();
  auto accu_init = factory.NewMemberCall("value", std::move(target_copy));
  auto condition = factory.NewBoolConst(false);
  call_args.push_back(factory.NewComprehension(
      "#unused", std::move(iter_range), std::move(var_name),
      std::move(accu_init), std::move(condition), std::move(args[0]),
      std::move(args[1])));
  call_args.push_back(factory.NewCall("optional.none"));
  return factory.NewCall(CelOperator::CONDITIONAL, std::move(call_args));
}

Macro MakeOptFlatMapMacro() {
  auto status_or_macro =
      Macro::Receiver("optFlatMap", 2, ExpandOptFlatMapMacro);
  ABSL_CHECK_OK(status_or_macro);  // Crash OK
  return std::move(*status_or_macro);
}

}  // namespace

absl::StatusOr<Macro> Macro::Global(absl::string_view name,
                                    size_t argument_count,
                                    GlobalMacroExpander expander) {
  if (!expander) {
    return absl::InvalidArgumentError(
        absl::StrCat("macro expander for `", name, "` cannot be empty"));
  }
  return Make(name, argument_count, ToMacroExpander(std::move(expander)),
              /*receiver_style=*/false, /*var_arg_style=*/false);
}

absl::StatusOr<Macro> Macro::GlobalVarArg(absl::string_view name,
                                          GlobalMacroExpander expander) {
  if (!expander) {
    return absl::InvalidArgumentError(
        absl::StrCat("macro expander for `", name, "` cannot be empty"));
  }
  return Make(name, 0, ToMacroExpander(std::move(expander)),
              /*receiver_style=*/false,
              /*var_arg_style=*/true);
}

absl::StatusOr<Macro> Macro::Receiver(absl::string_view name,
                                      size_t argument_count,
                                      ReceiverMacroExpander expander) {
  if (!expander) {
    return absl::InvalidArgumentError(
        absl::StrCat("macro expander for `", name, "` cannot be empty"));
  }
  return Make(name, argument_count, ToMacroExpander(std::move(expander)),
              /*receiver_style=*/true, /*var_arg_style=*/false);
}

absl::StatusOr<Macro> Macro::ReceiverVarArg(absl::string_view name,
                                            ReceiverMacroExpander expander) {
  if (!expander) {
    return absl::InvalidArgumentError(
        absl::StrCat("macro expander for `", name, "` cannot be empty"));
  }
  return Make(name, 0, ToMacroExpander(std::move(expander)),
              /*receiver_style=*/true,
              /*var_arg_style=*/true);
}

std::vector<Macro> Macro::AllMacros() {
  return {HasMacro(),  AllMacro(),  ExistsMacro(), ExistsOneMacro(),
          Map2Macro(), Map3Macro(), FilterMacro()};
}

std::string Macro::Key(absl::string_view name, size_t argument_count,
                       bool receiver_style, bool var_arg_style) {
  if (var_arg_style) {
    return absl::StrCat(name, ":*:", receiver_style ? "true" : "false");
  }
  return absl::StrCat(name, ":", argument_count, ":",
                      receiver_style ? "true" : "false");
}

absl::StatusOr<Macro> Macro::Make(absl::string_view name, size_t argument_count,
                                  MacroExpander expander, bool receiver_style,
                                  bool var_arg_style) {
  if (!internal::LexisIsIdentifier(name)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "macro function name `", name, "` is not a valid identifier"));
  }
  if (!expander) {
    return absl::InvalidArgumentError(
        absl::StrCat("macro expander for `", name, "` cannot be empty"));
  }
  return Macro(std::make_shared<Rep>(
      std::string(name),
      Key(name, argument_count, receiver_style, var_arg_style), argument_count,
      std::move(expander), receiver_style, var_arg_style));
}

const Macro& HasMacro() {
  static const absl::NoDestructor<Macro> macro(MakeHasMacro());
  return *macro;
}

const Macro& AllMacro() {
  static const absl::NoDestructor<Macro> macro(MakeAllMacro());
  return *macro;
}

const Macro& ExistsMacro() {
  static const absl::NoDestructor<Macro> macro(MakeExistsMacro());
  return *macro;
}

const Macro& ExistsOneMacro() {
  static const absl::NoDestructor<Macro> macro(MakeExistsOneMacro());
  return *macro;
}

const Macro& Map2Macro() {
  static const absl::NoDestructor<Macro> macro(MakeMap2Macro());
  return *macro;
}

const Macro& Map3Macro() {
  static const absl::NoDestructor<Macro> macro(MakeMap3Macro());
  return *macro;
}

const Macro& FilterMacro() {
  static const absl::NoDestructor<Macro> macro(MakeFilterMacro());
  return *macro;
}

const Macro& OptMapMacro() {
  static const absl::NoDestructor<Macro> macro(MakeOptMapMacro());
  return *macro;
}

const Macro& OptFlatMapMacro() {
  static const absl::NoDestructor<Macro> macro(MakeOptFlatMapMacro());
  return *macro;
}

}  // namespace cel
