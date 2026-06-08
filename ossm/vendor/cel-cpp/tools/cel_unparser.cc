// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tools/cel_unparser.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

#include "cel/expr/syntax.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/operators.h"
#include "internal/status_macros.h"
#include "internal/strings.h"
#include "re2/re2.h"

namespace google::api::expr {
namespace {

using ::cel::expr::CheckedExpr;
using ::cel::expr::Constant;
using ::cel::expr::Expr;
using ::cel::expr::ParsedExpr;
using ::cel::expr::SourceInfo;
using ::google::api::expr::common::CelOperator;
using ::google::api::expr::common::IsOperatorLeftRecursive;
using ::google::api::expr::common::IsOperatorLowerPrecedence;
using ::google::api::expr::common::IsOperatorSamePrecedence;
using ::google::api::expr::common::LookupBinaryOperator;
using ::google::api::expr::common::LookupUnaryOperator;

constexpr absl::string_view kLeftParen = "(";
constexpr absl::string_view kRightParen = ")";
constexpr absl::string_view kLeftBracket = "[";
constexpr absl::string_view kRightBracket = "]";
constexpr absl::string_view kLeftBrace = "{";
constexpr absl::string_view kRightBrace = "}";
constexpr absl::string_view kSpace = " ";
constexpr absl::string_view kDot = ".";
constexpr absl::string_view kColon = ":";
constexpr absl::string_view kComma = ",";
constexpr absl::string_view kBackQuote = "`";
constexpr absl::string_view kQuestionMark = "?";

static const LazyRE2 kSimpleIdentifierPattern = {R"([a-zA-Z_][a-zA-Z0-9_]*)"};

const absl::flat_hash_set<std::string>& ReservedFieldIdentifiers() {
  static const absl::NoDestructor<absl::flat_hash_set<std::string>>
      kReservedFieldIdentifiers(
          []() { return absl::flat_hash_set<std::string>{"in"}; }());
  return *kReservedFieldIdentifiers;
}

std::string FormatField(absl::string_view field) {
  if (ReservedFieldIdentifiers().contains(field) ||
      !RE2::FullMatch(field, *kSimpleIdentifierPattern)) {
    return absl::StrCat(kBackQuote, field, kBackQuote);
  }
  return std::string(field);
}

class Unparser {
 public:
  static absl::StatusOr<std::string> Unparse(const Expr& expr,
                                             const SourceInfo& source_info) {
    Unparser unparser(expr, source_info);
    return unparser.DoUnparse();
  }

 private:
  const Expr& expr_;
  const SourceInfo& source_info_;
  std::string output_;

  Unparser(const Expr& expr, const SourceInfo& source_info)
      : expr_(expr), source_info_(source_info) {}

  absl::StatusOr<std::string> DoUnparse() {
    CEL_RETURN_IF_ERROR(Visit(expr_));
    absl::StripAsciiWhitespace(&output_);
    return std::move(output_);
  }

  absl::Status Visit(const Expr& expr);

  absl::Status VisitConst(const Constant& expr);

  absl::Status VisitIdent(const Expr::Ident& expr);

  absl::Status VisitSelect(const Expr::Select& expr);

  absl::Status VisitOptSelect(const Expr::Call& expr);

  absl::Status VisitCall(const Expr::Call& expr);

  absl::Status VisitCreateList(const Expr::CreateList& expr);

  absl::Status VisitCreateStruct(const Expr::CreateStruct& expr);

  absl::Status VisitComprehension(const Expr::Comprehension& expr);

  absl::Status VisitAllMacro(const Expr::Comprehension& expr);

  absl::Status VisitExistsMacro(const Expr::Comprehension& expr);

  absl::Status VisitExistsOneMacro(const Expr::Comprehension& expr);

  absl::Status VisitMapMacro(const Expr::Comprehension& expr);

  absl::Status VisitUnary(const Expr::Call& expr, const std::string& op);

  absl::Status VisitBinary(const Expr::Call& expr, const std::string& op);

  absl::Status VisitMaybeNested(const Expr& expr, bool nested);

  absl::Status VisitIndex(const Expr::Call& expr);

  absl::Status VisitOptIndex(const Expr::Call& expr);

  absl::Status VisitTernary(const Expr::Call& expr);

  bool IsComplexOperatorWithRespectTo(const Expr& expr, const std::string& op);

  bool IsComplexOperator(const Expr& expr);

  // Returns true the given expression is
  // - a call expression AND ONE of the following holds:
  //   - a binary operator
  //   - a ternary conditional operator
  bool IsBinaryOrTernaryOperator(const Expr& expr);

  template <typename... Ts>
  void Print(Ts&&... args) {
    absl::StrAppend(&output_, std::forward<Ts>(args)...);
  }
};

absl::Status Unparser::Visit(const Expr& expr) {
  auto macro = source_info_.macro_calls().find(expr.id());
  if (macro != source_info_.macro_calls().end()) {
    return Visit(macro->second);
  }
  switch (expr.expr_kind_case()) {
    case Expr::kConstExpr:
      return VisitConst(expr.const_expr());
    case Expr::kIdentExpr:
      return VisitIdent(expr.ident_expr());
    case Expr::kSelectExpr:
      return VisitSelect(expr.select_expr());
    case Expr::kCallExpr:
      return VisitCall(expr.call_expr());
    case Expr::kListExpr:
      return VisitCreateList(expr.list_expr());
    case Expr::kStructExpr:
      return VisitCreateStruct(expr.struct_expr());
    case Expr::kComprehensionExpr:
      return VisitComprehension(expr.comprehension_expr());
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unsupported Expr kind: ", expr.expr_kind_case()));
  }
}

absl::Status Unparser::VisitConst(const Constant& expr) {
  switch (expr.constant_kind_case()) {
    case Constant::kStringValue:
      Print(
          cel::internal::FormatDoubleQuotedStringLiteral(expr.string_value()));
      break;
    case Constant::kInt64Value:
      Print(expr.int64_value());
      break;
    case Constant::kUint64Value:
      Print(expr.uint64_value(), "u");
      break;
    case Constant::kBoolValue:
      Print(expr.bool_value() ? "true" : "false");
      break;
    case Constant::kDoubleValue:
      Print(expr.double_value());
      break;
    case Constant::kNullValue:
      Print("null");
      break;
    case Constant::kBytesValue:
      Print(cel::internal::FormatDoubleQuotedBytesLiteral(expr.bytes_value()));
      break;
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "Unsupported Constant kind: ", expr.constant_kind_case()));
  }
  return absl::OkStatus();
}

absl::Status Unparser::VisitIdent(const Expr::Ident& expr) {
  Print(expr.name());
  return absl::OkStatus();
}

absl::Status Unparser::VisitSelect(const Expr::Select& expr) {
  if (expr.test_only()) {
    Print(CelOperator::HAS, kLeftParen);
  }
  const auto& operand = expr.operand();
  bool nested = !expr.test_only() && IsBinaryOrTernaryOperator(operand);
  CEL_RETURN_IF_ERROR(VisitMaybeNested(operand, nested));
  Print(kDot, FormatField(expr.field()));
  if (expr.test_only()) {
    Print(kRightParen);
  }
  return absl::OkStatus();
}

absl::Status Unparser::VisitOptSelect(const Expr::Call& expr) {
  if (expr.args_size() != 2 || !expr.args()[1].has_const_expr() ||
      !expr.args()[1].const_expr().has_string_value()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unexpected select: ", expr.ShortDebugString()));
  }
  const auto& operand = expr.args()[0];
  bool nested = IsBinaryOrTernaryOperator(operand);
  CEL_RETURN_IF_ERROR(VisitMaybeNested(operand, nested));
  Print(kDot, kQuestionMark,
        FormatField(expr.args()[1].const_expr().string_value()));
  return absl::OkStatus();
}

absl::Status Unparser::VisitCall(const Expr::Call& expr) {
  const auto& fun = expr.function();
  absl::optional<std::string> op = LookupUnaryOperator(fun);
  if (op.has_value()) {
    return VisitUnary(expr, *op);
  }

  op = LookupBinaryOperator(fun);
  if (op.has_value()) {
    return VisitBinary(expr, *op);
  }

  if (fun == CelOperator::INDEX) {
    return VisitIndex(expr);
  }

  if (fun == CelOperator::OPT_INDEX) {
    return VisitOptIndex(expr);
  }

  if (fun == CelOperator::OPT_SELECT) {
    return VisitOptSelect(expr);
  }

  if (fun == CelOperator::CONDITIONAL) {
    return VisitTernary(expr);
  }

  if (expr.has_target()) {
    bool nested = IsBinaryOrTernaryOperator(expr.target());
    CEL_RETURN_IF_ERROR(VisitMaybeNested(expr.target(), nested));
    Print(kDot);
  }
  Print(fun, kLeftParen);
  for (int i = 0; i < expr.args_size(); i++) {
    if (i > 0) {
      Print(kComma, kSpace);
    }
    CEL_RETURN_IF_ERROR(Visit(expr.args(i)));
  }
  Print(kRightParen);
  return absl::OkStatus();
}

absl::Status Unparser::VisitCreateList(const Expr::CreateList& expr) {
  Print(kLeftBracket);
  for (int i = 0; i < expr.elements_size(); i++) {
    if (i > 0) {
      Print(kComma, kSpace);
    }
    if (std::find(expr.optional_indices().begin(),
                  expr.optional_indices().end(),
                  static_cast<int32_t>(i)) != expr.optional_indices().end()) {
      Print(kQuestionMark);
    }
    CEL_RETURN_IF_ERROR(Visit(expr.elements(i)));
  }
  Print(kRightBracket);
  return absl::OkStatus();
}

absl::Status Unparser::VisitCreateStruct(const Expr::CreateStruct& expr) {
  if (!expr.message_name().empty()) {
    Print(expr.message_name());
  }
  Print(kLeftBrace);
  for (int i = 0; i < expr.entries_size(); i++) {
    if (i > 0) {
      Print(kComma, kSpace);
    }

    const auto& e = expr.entries(i);
    if (e.optional_entry()) {
      Print(kQuestionMark);
    }
    switch (e.key_kind_case()) {
      case Expr::CreateStruct::Entry::kFieldKey:
        Print(FormatField(e.field_key()));
        break;
      case Expr::CreateStruct::Entry::kMapKey:
        CEL_RETURN_IF_ERROR(Visit(e.map_key()));
        break;
      default:
        return absl::InvalidArgumentError(
            absl::StrCat("Unexpected struct: ", expr.ShortDebugString()));
    }
    Print(kColon, kSpace);
    CEL_RETURN_IF_ERROR(Visit(e.value()));
  }
  Print(kRightBrace);
  return absl::OkStatus();
}

absl::Status Unparser::VisitComprehension(const Expr::Comprehension& expr) {
  bool nested = IsComplexOperator(expr.iter_range());
  CEL_RETURN_IF_ERROR(VisitMaybeNested(expr.iter_range(), nested));
  Print(kDot);

  if (expr.loop_step().call_expr().function() == CelOperator::LOGICAL_AND) {
    return VisitAllMacro(expr);
  }

  if (expr.loop_step().call_expr().function() == CelOperator::LOGICAL_OR) {
    return VisitExistsMacro(expr);
  }

  if (expr.result().expr_kind_case() == Expr::kCallExpr) {
    return VisitExistsOneMacro(expr);
  }

  return VisitMapMacro(expr);
}

absl::Status Unparser::VisitAllMacro(const Expr::Comprehension& expr) {
  if (expr.loop_step().call_expr().args_size() != 2) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unexpected all macro: ", expr.ShortDebugString()));
  }

  Print(CelOperator::ALL, kLeftParen, expr.iter_var(), kComma, kSpace);
  CEL_RETURN_IF_ERROR(Visit(expr.loop_step().call_expr().args(1)));
  Print(kRightParen);
  return absl::OkStatus();
}

absl::Status Unparser::VisitExistsMacro(const Expr::Comprehension& expr) {
  if (expr.loop_step().call_expr().args_size() != 2) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unexpected exists macro: ", expr.ShortDebugString()));
  }

  Print(CelOperator::EXISTS, kLeftParen, expr.iter_var(), kComma, kSpace);
  CEL_RETURN_IF_ERROR(Visit(expr.loop_step().call_expr().args(1)));
  Print(kRightParen);
  return absl::OkStatus();
}

absl::Status Unparser::VisitExistsOneMacro(const Expr::Comprehension& expr) {
  if (expr.loop_step().call_expr().args_size() != 3) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unexpected exists one macro: ", expr.ShortDebugString()));
  }

  Print(CelOperator::EXISTS_ONE, kLeftParen, expr.iter_var(), kComma, kSpace);
  CEL_RETURN_IF_ERROR(Visit(expr.loop_step().call_expr().args(0)));
  Print(kRightParen);
  return absl::OkStatus();
}

absl::Status Unparser::VisitMapMacro(const Expr::Comprehension& expr) {
  Print(CelOperator::MAP, kLeftParen, expr.iter_var(), kComma, kSpace);
  Expr step = expr.loop_step();
  if (step.call_expr().function() == CelOperator::CONDITIONAL) {
    if (step.call_expr().args_size() != 3) {
      return absl::InvalidArgumentError(
          absl::StrCat("Unexpected exists map macro filter step: ",
                       expr.ShortDebugString()));
    }

    CEL_RETURN_IF_ERROR(Visit(step.call_expr().args(0)));
    Print(kComma, kSpace);

    auto temp = step.call_expr().args(1);
    step = temp;
  }

  if (step.call_expr().args_size() != 2 ||
      step.call_expr().args(1).list_expr().elements_size() != 1) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unexpected exists map macro: ", expr.ShortDebugString()));
  }

  CEL_RETURN_IF_ERROR(Visit(step.call_expr().args(1).list_expr().elements(0)));
  Print(kRightParen);
  return absl::OkStatus();
}

absl::Status Unparser::VisitUnary(const Expr::Call& expr,
                                  const std::string& op) {
  if (expr.args_size() != 1) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unexpected unary: ", expr.ShortDebugString()));
  }
  Print(op);
  bool nested = IsComplexOperator(expr.args(0));
  return VisitMaybeNested(expr.args(0), nested);
}

absl::Status Unparser::VisitBinary(const Expr::Call& expr,
                                   const std::string& op) {
  if (expr.args_size() != 2) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unexpected binary: ", expr.ShortDebugString()));
  }

  const auto& lhs = expr.args(0);
  const auto& rhs = expr.args(1);
  const auto& fun = expr.function();

  // add parens if the current operator is lower precedence than the lhs expr
  // operator.
  bool lhs_paren = IsComplexOperatorWithRespectTo(lhs, fun);
  // add parens if the current operator is lower precedence than the rhs expr
  // operator, or the same precedence and the operator is left recursive.
  bool rhs_paren = IsComplexOperatorWithRespectTo(rhs, fun);
  if (!rhs_paren && IsOperatorLeftRecursive(fun)) {
    rhs_paren = IsOperatorSamePrecedence(fun, rhs);
  }

  CEL_RETURN_IF_ERROR(VisitMaybeNested(lhs, lhs_paren));
  Print(kSpace, op, kSpace);
  return VisitMaybeNested(rhs, rhs_paren);
}

absl::Status Unparser::VisitMaybeNested(const Expr& expr, bool nested) {
  if (nested) {
    Print(kLeftParen);
  }
  CEL_RETURN_IF_ERROR(Visit(expr));
  if (nested) {
    Print(kRightParen);
  }
  return absl::OkStatus();
}

absl::Status Unparser::VisitIndex(const Expr::Call& expr) {
  if (expr.args_size() != 2) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unexpected index call: ", expr.ShortDebugString()));
  }
  bool nested = IsBinaryOrTernaryOperator(expr.args(0));
  CEL_RETURN_IF_ERROR(VisitMaybeNested(expr.args(0), nested));
  Print(kLeftBracket);
  CEL_RETURN_IF_ERROR(Visit(expr.args(1)));
  Print(kRightBracket);
  return absl::OkStatus();
}

absl::Status Unparser::VisitOptIndex(const Expr::Call& expr) {
  if (expr.args_size() != 2) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unexpected index call: ", expr.ShortDebugString()));
  }
  bool nested = IsBinaryOrTernaryOperator(expr.args(0));
  CEL_RETURN_IF_ERROR(VisitMaybeNested(expr.args(0), nested));
  Print(kLeftBracket);
  Print(kQuestionMark);
  CEL_RETURN_IF_ERROR(Visit(expr.args(1)));
  Print(kRightBracket);
  return absl::OkStatus();
}

absl::Status Unparser::VisitTernary(const Expr::Call& expr) {
  if (expr.args_size() != 3) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unexpected ternary: ", expr.ShortDebugString()));
  }

  bool nested =
      IsOperatorSamePrecedence(CelOperator::CONDITIONAL, expr.args(0)) ||
      IsComplexOperator(expr.args(0));
  CEL_RETURN_IF_ERROR(VisitMaybeNested(expr.args(0), nested));

  Print(kSpace, kQuestionMark, kSpace);

  nested = IsOperatorSamePrecedence(CelOperator::CONDITIONAL, expr.args(1)) ||
           IsComplexOperator(expr.args(1));
  CEL_RETURN_IF_ERROR(VisitMaybeNested(expr.args(1), nested));

  Print(kSpace, kColon, kSpace);

  nested = IsOperatorSamePrecedence(CelOperator::CONDITIONAL, expr.args(2)) ||
           IsComplexOperator(expr.args(2));
  return VisitMaybeNested(expr.args(2), nested);
}

bool Unparser::IsComplexOperatorWithRespectTo(const Expr& expr,
                                              const std::string& op) {
  // If the arg is not a call with more than one arg, return false.
  if (!expr.has_call_expr() || expr.call_expr().args_size() < 2) {
    return false;
  }
  // Otherwise, return whether the given op has lower precedence than expr
  return IsOperatorLowerPrecedence(op, expr);
}

bool Unparser::IsComplexOperator(const Expr& expr) {
  // If the arg is a call with more than one arg, return true
  return expr.has_call_expr() && expr.call_expr().args_size() >= 2;
}

// Returns true the given expression is
// - a call expression AND ONE of the following holds:
//   - a binary operator
//   - a ternary conditional operator
bool Unparser::IsBinaryOrTernaryOperator(const Expr& expr) {
  if (!IsComplexOperator(expr)) {
    return false;
  }
  return LookupBinaryOperator(expr.call_expr().function()).has_value() ||
         IsOperatorSamePrecedence(CelOperator::CONDITIONAL, expr);
}

}  // namespace

absl::StatusOr<std::string> Unparse(const Expr& expr,
                                    const SourceInfo* source_info) {
  const SourceInfo& info = source_info == nullptr ? SourceInfo() : *source_info;
  return Unparser::Unparse(expr, info);
}

absl::StatusOr<std::string> Unparse(const ParsedExpr& parsed_expr) {
  return Unparse(parsed_expr.expr(), &parsed_expr.source_info());
}

absl::StatusOr<std::string> Unparse(const CheckedExpr& checked_expr) {
  return Unparse(checked_expr.expr(), &checked_expr.source_info());
}

}  // namespace google::api::expr
