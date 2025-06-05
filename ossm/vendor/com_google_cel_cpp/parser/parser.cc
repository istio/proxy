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

#include "parser/parser.h"

#include <algorithm>
#include <any>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/overload.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "antlr4-runtime.h"
#include "common/ast.h"
#include "common/constant.h"
#include "common/expr_factory.h"
#include "common/operators.h"
#include "common/source.h"
#include "extensions/protobuf/internal/ast.h"
#include "internal/lexis.h"
#include "internal/status_macros.h"
#include "internal/strings.h"
#include "internal/utf8.h"
#include "parser/internal/CelBaseVisitor.h"
#include "parser/internal/CelLexer.h"
#include "parser/internal/CelParser.h"
#include "parser/macro.h"
#include "parser/macro_expr_factory.h"
#include "parser/macro_registry.h"
#include "parser/options.h"
#include "parser/source_factory.h"

namespace google::api::expr::parser {
namespace {
class ParserVisitor;
}
}  // namespace google::api::expr::parser

namespace cel {

namespace {

std::any ExprPtrToAny(std::unique_ptr<Expr>&& expr) {
  return std::make_any<Expr*>(expr.release());
}

std::any ExprToAny(Expr&& expr) {
  return ExprPtrToAny(std::make_unique<Expr>(std::move(expr)));
}

std::unique_ptr<Expr> ExprPtrFromAny(std::any&& any) {
  return absl::WrapUnique(std::any_cast<Expr*>(std::move(any)));
}

Expr ExprFromAny(std::any&& any) {
  auto expr = ExprPtrFromAny(std::move(any));
  return std::move(*expr);
}

struct ParserError {
  std::string message;
  SourceRange range;
};

std::string DisplayParserError(const cel::Source& source,
                               const ParserError& error) {
  auto location =
      source.GetLocation(error.range.begin).value_or(SourceLocation{});
  return absl::StrCat(absl::StrFormat("ERROR: %s:%zu:%zu: %s",
                                      source.description(), location.line,
                                      // add one to the 0-based column
                                      location.column + 1, error.message),
                      source.DisplayErrorLocation(location));
}

int32_t PositiveOrMax(int32_t value) {
  return value >= 0 ? value : std::numeric_limits<int32_t>::max();
}

SourceRange SourceRangeFromToken(const antlr4::Token* token) {
  SourceRange range;
  if (token != nullptr) {
    if (auto start = token->getStartIndex(); start != INVALID_INDEX) {
      range.begin = static_cast<int32_t>(start);
    }
    if (auto end = token->getStopIndex(); end != INVALID_INDEX) {
      range.end = static_cast<int32_t>(end + 1);
    }
  }
  return range;
}

SourceRange SourceRangeFromParserRuleContext(
    const antlr4::ParserRuleContext* context) {
  SourceRange range;
  if (context != nullptr) {
    if (auto start = context->getStart() != nullptr
                         ? context->getStart()->getStartIndex()
                         : INVALID_INDEX;
        start != INVALID_INDEX) {
      range.begin = static_cast<int32_t>(start);
    }
    if (auto end = context->getStop() != nullptr
                       ? context->getStop()->getStopIndex()
                       : INVALID_INDEX;
        end != INVALID_INDEX) {
      range.end = static_cast<int32_t>(end + 1);
    }
  }
  return range;
}

}  // namespace

class ParserMacroExprFactory final : public MacroExprFactory {
 public:
  explicit ParserMacroExprFactory(const cel::Source& source)
      : MacroExprFactory(), source_(source) {}

  void BeginMacro(SourceRange macro_position) {
    macro_position_ = macro_position;
  }

  void EndMacro() { macro_position_ = SourceRange{}; }

  Expr ReportError(absl::string_view message) override {
    return ReportError(macro_position_, message);
  }

  Expr ReportError(int64_t expr_id, absl::string_view message) {
    return ReportError(GetSourceRange(expr_id), message);
  }

  Expr ReportError(SourceRange range, absl::string_view message) {
    ++error_count_;
    if (errors_.size() <= 100) {
      errors_.push_back(ParserError{std::string(message), range});
    }
    return NewUnspecified(NextId(range));
  }

  Expr ReportErrorAt(const Expr& expr, absl::string_view message) override {
    return ReportError(GetSourceRange(expr.id()), message);
  }

  SourceRange GetSourceRange(int64_t id) const {
    if (auto it = positions_.find(id); it != positions_.end()) {
      return it->second;
    }
    return SourceRange{};
  }

  int64_t NextId(const SourceRange& range) {
    auto id = expr_id_++;
    if (range.begin != -1 || range.end != -1) {
      positions_.insert(std::pair{id, range});
    }
    return id;
  }

  bool HasErrors() const { return error_count_ != 0; }

  std::string ErrorMessage() {
    // Errors are collected as they are encountered, not by their location
    // within the source. To have a more stable error message as implementation
    // details change, we sort the collected errors by their source location
    // first.
    std::stable_sort(
        errors_.begin(), errors_.end(),
        [](const ParserError& lhs, const ParserError& rhs) -> bool {
          auto lhs_begin = PositiveOrMax(lhs.range.begin);
          auto lhs_end = PositiveOrMax(lhs.range.end);
          auto rhs_begin = PositiveOrMax(rhs.range.begin);
          auto rhs_end = PositiveOrMax(rhs.range.end);
          return lhs_begin < rhs_begin ||
                 (lhs_begin == rhs_begin && lhs_end < rhs_end);
        });
    // Build the summary error message using the sorted errors.
    bool errors_truncated = error_count_ > 100;
    std::vector<std::string> messages;
    messages.reserve(
        errors_.size() +
        errors_truncated);  // Reserve space for the transform and an
                            // additional element when truncation occurs.
    std::transform(errors_.begin(), errors_.end(), std::back_inserter(messages),
                   [this](const ParserError& error) {
                     return cel::DisplayParserError(source_, error);
                   });
    if (errors_truncated) {
      messages.emplace_back(
          absl::StrCat(error_count_ - 100, " more errors were truncated."));
    }
    return absl::StrJoin(messages, "\n");
  }

  void AddMacroCall(int64_t macro_id, absl::string_view function,
                    absl::optional<Expr> target, std::vector<Expr> arguments) {
    macro_calls_.insert(
        {macro_id, target.has_value()
                       ? NewMemberCall(0, function, std::move(*target),
                                       std::move(arguments))
                       : NewCall(0, function, std::move(arguments))});
  }

  Expr BuildMacroCallArg(const Expr& expr) {
    if (auto it = macro_calls_.find(expr.id()); it != macro_calls_.end()) {
      return NewUnspecified(expr.id());
    }
    return absl::visit(
        absl::Overload(
            [this, &expr](const UnspecifiedExpr&) -> Expr {
              return NewUnspecified(expr.id());
            },
            [this, &expr](const Constant& const_expr) -> Expr {
              return NewConst(expr.id(), const_expr);
            },
            [this, &expr](const IdentExpr& ident_expr) -> Expr {
              return NewIdent(expr.id(), ident_expr.name());
            },
            [this, &expr](const SelectExpr& select_expr) -> Expr {
              return select_expr.test_only()
                         ? NewPresenceTest(
                               expr.id(),
                               BuildMacroCallArg(select_expr.operand()),
                               select_expr.field())
                         : NewSelect(expr.id(),
                                     BuildMacroCallArg(select_expr.operand()),
                                     select_expr.field());
            },
            [this, &expr](const CallExpr& call_expr) -> Expr {
              std::vector<Expr> macro_arguments;
              macro_arguments.reserve(call_expr.args().size());
              for (const auto& argument : call_expr.args()) {
                macro_arguments.push_back(BuildMacroCallArg(argument));
              }
              absl::optional<Expr> macro_target;
              if (call_expr.has_target()) {
                macro_target = BuildMacroCallArg(call_expr.target());
              }
              return macro_target.has_value()
                         ? NewMemberCall(expr.id(), call_expr.function(),
                                         std::move(*macro_target),
                                         std::move(macro_arguments))
                         : NewCall(expr.id(), call_expr.function(),
                                   std::move(macro_arguments));
            },
            [this, &expr](const ListExpr& list_expr) -> Expr {
              std::vector<ListExprElement> macro_elements;
              macro_elements.reserve(list_expr.elements().size());
              for (const auto& element : list_expr.elements()) {
                auto& cloned_element = macro_elements.emplace_back();
                if (element.has_expr()) {
                  cloned_element.set_expr(BuildMacroCallArg(element.expr()));
                }
                cloned_element.set_optional(element.optional());
              }
              return NewList(expr.id(), std::move(macro_elements));
            },
            [this, &expr](const StructExpr& struct_expr) -> Expr {
              std::vector<StructExprField> macro_fields;
              macro_fields.reserve(struct_expr.fields().size());
              for (const auto& field : struct_expr.fields()) {
                auto& macro_field = macro_fields.emplace_back();
                macro_field.set_id(field.id());
                macro_field.set_name(field.name());
                macro_field.set_value(BuildMacroCallArg(field.value()));
                macro_field.set_optional(field.optional());
              }
              return NewStruct(expr.id(), struct_expr.name(),
                               std::move(macro_fields));
            },
            [this, &expr](const MapExpr& map_expr) -> Expr {
              std::vector<MapExprEntry> macro_entries;
              macro_entries.reserve(map_expr.entries().size());
              for (const auto& entry : map_expr.entries()) {
                auto& macro_entry = macro_entries.emplace_back();
                macro_entry.set_id(entry.id());
                macro_entry.set_key(BuildMacroCallArg(entry.key()));
                macro_entry.set_value(BuildMacroCallArg(entry.value()));
                macro_entry.set_optional(entry.optional());
              }
              return NewMap(expr.id(), std::move(macro_entries));
            },
            [this, &expr](const ComprehensionExpr& comprehension_expr) -> Expr {
              return NewComprehension(
                  expr.id(), comprehension_expr.iter_var(),
                  BuildMacroCallArg(comprehension_expr.iter_range()),
                  comprehension_expr.accu_var(),
                  BuildMacroCallArg(comprehension_expr.accu_init()),
                  BuildMacroCallArg(comprehension_expr.loop_condition()),
                  BuildMacroCallArg(comprehension_expr.loop_step()),
                  BuildMacroCallArg(comprehension_expr.result()));
            }),
        expr.kind());
  }

  using ExprFactory::NewBoolConst;
  using ExprFactory::NewBytesConst;
  using ExprFactory::NewCall;
  using ExprFactory::NewComprehension;
  using ExprFactory::NewConst;
  using ExprFactory::NewDoubleConst;
  using ExprFactory::NewIdent;
  using ExprFactory::NewIntConst;
  using ExprFactory::NewList;
  using ExprFactory::NewListElement;
  using ExprFactory::NewMap;
  using ExprFactory::NewMapEntry;
  using ExprFactory::NewMemberCall;
  using ExprFactory::NewNullConst;
  using ExprFactory::NewPresenceTest;
  using ExprFactory::NewSelect;
  using ExprFactory::NewStringConst;
  using ExprFactory::NewStruct;
  using ExprFactory::NewStructField;
  using ExprFactory::NewUintConst;
  using ExprFactory::NewUnspecified;

  const absl::btree_map<int64_t, SourceRange>& positions() const {
    return positions_;
  }

  const absl::flat_hash_map<int64_t, Expr>& macro_calls() const {
    return macro_calls_;
  }

  void EraseId(ExprId id) {
    positions_.erase(id);
    if (expr_id_ == id + 1) {
      --expr_id_;
    }
  }

 protected:
  int64_t NextId() override { return NextId(macro_position_); }

  int64_t CopyId(int64_t id) override {
    if (id == 0) {
      return 0;
    }
    return NextId(GetSourceRange(id));
  }

 private:
  int64_t expr_id_ = 1;
  absl::btree_map<int64_t, SourceRange> positions_;
  absl::flat_hash_map<int64_t, Expr> macro_calls_;
  std::vector<ParserError> errors_;
  size_t error_count_ = 0;
  const Source& source_;
  SourceRange macro_position_;
};

}  // namespace cel

namespace google::api::expr::parser {

namespace {

using ::antlr4::CharStream;
using ::antlr4::CommonTokenStream;
using ::antlr4::DefaultErrorStrategy;
using ::antlr4::ParseCancellationException;
using ::antlr4::Parser;
using ::antlr4::ParserRuleContext;
using ::antlr4::Token;
using ::antlr4::misc::IntervalSet;
using ::antlr4::tree::ErrorNode;
using ::antlr4::tree::ParseTreeListener;
using ::antlr4::tree::TerminalNode;
using ::cel::Expr;
using ::cel::ExprFromAny;
using ::cel::ExprKind;
using ::cel::ExprToAny;
using ::cel::IdentExpr;
using ::cel::ListExprElement;
using ::cel::MapExprEntry;
using ::cel::SelectExpr;
using ::cel::SourceRangeFromParserRuleContext;
using ::cel::SourceRangeFromToken;
using ::cel::StructExprField;
using ::cel_parser_internal::CelBaseVisitor;
using ::cel_parser_internal::CelLexer;
using ::cel_parser_internal::CelParser;
using common::CelOperator;
using common::ReverseLookupOperator;
using ::google::api::expr::v1alpha1::ParsedExpr;

class CodePointStream final : public CharStream {
 public:
  CodePointStream(cel::SourceContentView buffer, absl::string_view source_name)
      : buffer_(buffer),
        source_name_(source_name),
        size_(buffer_.size()),
        index_(0) {}

  void consume() override {
    if (ABSL_PREDICT_FALSE(index_ >= size_)) {
      ABSL_ASSERT(LA(1) == IntStream::EOF);
      throw antlr4::IllegalStateException("cannot consume EOF");
    }
    index_++;
  }

  size_t LA(ssize_t i) override {
    if (ABSL_PREDICT_FALSE(i == 0)) {
      return 0;
    }
    auto p = static_cast<ssize_t>(index_);
    if (i < 0) {
      i++;
      if (p + i - 1 < 0) {
        return IntStream::EOF;
      }
    }
    if (p + i - 1 >= static_cast<ssize_t>(size_)) {
      return IntStream::EOF;
    }
    return buffer_.at(static_cast<size_t>(p + i - 1));
  }

  ssize_t mark() override { return -1; }

  void release(ssize_t marker) override {}

  size_t index() override { return index_; }

  void seek(size_t index) override { index_ = std::min(index, size_); }

  size_t size() override { return size_; }

  std::string getSourceName() const override {
    return source_name_.empty() ? IntStream::UNKNOWN_SOURCE_NAME
                                : std::string(source_name_);
  }

  std::string getText(const antlr4::misc::Interval& interval) override {
    if (ABSL_PREDICT_FALSE(interval.a < 0 || interval.b < 0)) {
      return std::string();
    }
    size_t start = static_cast<size_t>(interval.a);
    if (ABSL_PREDICT_FALSE(start >= size_)) {
      return std::string();
    }
    size_t stop = static_cast<size_t>(interval.b);
    if (ABSL_PREDICT_FALSE(stop >= size_)) {
      stop = size_ - 1;
    }
    return buffer_.ToString(static_cast<cel::SourcePosition>(start),
                            static_cast<cel::SourcePosition>(stop) + 1);
  }

  std::string toString() const override { return buffer_.ToString(); }

 private:
  cel::SourceContentView const buffer_;
  const absl::string_view source_name_;
  const size_t size_;
  size_t index_;
};

// Scoped helper for incrementing the parse recursion count.
// Increments on creation, decrements on destruction (stack unwind).
class ScopedIncrement final {
 public:
  explicit ScopedIncrement(int& recursion_depth)
      : recursion_depth_(recursion_depth) {
    ++recursion_depth_;
  }

  ~ScopedIncrement() { --recursion_depth_; }

 private:
  int& recursion_depth_;
};

// balancer performs tree balancing on operators whose arguments are of equal
// precedence.
//
// The purpose of the balancer is to ensure a compact serialization format for
// the logical &&, || operators which have a tendency to create long DAGs which
// are skewed in one direction. Since the operators are commutative re-ordering
// the terms *must not* affect the evaluation result.
//
// Based on code from //third_party/cel/go/parser/helper.go
class ExpressionBalancer final {
 public:
  ExpressionBalancer(cel::ParserMacroExprFactory& factory, std::string function,
                     Expr expr);

  // addTerm adds an operation identifier and term to the set of terms to be
  // balanced.
  void AddTerm(int64_t op, Expr term);

  // balance creates a balanced tree from the sub-terms and returns the final
  // Expr value.
  Expr Balance();

 private:
  // balancedTree recursively balances the terms provided to a commutative
  // operator.
  Expr BalancedTree(int lo, int hi);

 private:
  cel::ParserMacroExprFactory& factory_;
  std::string function_;
  std::vector<Expr> terms_;
  std::vector<int64_t> ops_;
};

ExpressionBalancer::ExpressionBalancer(cel::ParserMacroExprFactory& factory,
                                       std::string function, Expr expr)
    : factory_(factory), function_(std::move(function)) {
  terms_.push_back(std::move(expr));
}

void ExpressionBalancer::AddTerm(int64_t op, Expr term) {
  terms_.push_back(std::move(term));
  ops_.push_back(op);
}

Expr ExpressionBalancer::Balance() {
  if (terms_.size() == 1) {
    return std::move(terms_[0]);
  }
  return BalancedTree(0, ops_.size() - 1);
}

Expr ExpressionBalancer::BalancedTree(int lo, int hi) {
  int mid = (lo + hi + 1) / 2;

  std::vector<Expr> arguments;
  arguments.reserve(2);

  if (mid == lo) {
    arguments.push_back(std::move(terms_[mid]));
  } else {
    arguments.push_back(BalancedTree(lo, mid - 1));
  }

  if (mid == hi) {
    arguments.push_back(std::move(terms_[mid + 1]));
  } else {
    arguments.push_back(BalancedTree(mid + 1, hi));
  }
  return factory_.NewCall(ops_[mid], function_, std::move(arguments));
}

class ParserVisitor final : public CelBaseVisitor,
                            public antlr4::BaseErrorListener {
 public:
  ParserVisitor(const cel::Source& source, int max_recursion_depth,
                const cel::MacroRegistry& macro_registry,
                bool add_macro_calls = false,
                bool enable_optional_syntax = false);
  ~ParserVisitor() override;

  std::any visit(antlr4::tree::ParseTree* tree) override;

  std::any visitStart(CelParser::StartContext* ctx) override;
  std::any visitExpr(CelParser::ExprContext* ctx) override;
  std::any visitConditionalOr(CelParser::ConditionalOrContext* ctx) override;
  std::any visitConditionalAnd(CelParser::ConditionalAndContext* ctx) override;
  std::any visitRelation(CelParser::RelationContext* ctx) override;
  std::any visitCalc(CelParser::CalcContext* ctx) override;
  std::any visitUnary(CelParser::UnaryContext* ctx);
  std::any visitLogicalNot(CelParser::LogicalNotContext* ctx) override;
  std::any visitNegate(CelParser::NegateContext* ctx) override;
  std::any visitSelect(CelParser::SelectContext* ctx) override;
  std::any visitMemberCall(CelParser::MemberCallContext* ctx) override;
  std::any visitIndex(CelParser::IndexContext* ctx) override;
  std::any visitCreateMessage(CelParser::CreateMessageContext* ctx) override;
  std::any visitFieldInitializerList(
      CelParser::FieldInitializerListContext* ctx) override;
  std::vector<StructExprField> visitFields(
      CelParser::FieldInitializerListContext* ctx);
  std::any visitIdentOrGlobalCall(
      CelParser::IdentOrGlobalCallContext* ctx) override;
  std::any visitNested(CelParser::NestedContext* ctx) override;
  std::any visitCreateList(CelParser::CreateListContext* ctx) override;
  std::vector<ListExprElement> visitList(CelParser::ListInitContext* ctx);
  std::vector<Expr> visitList(CelParser::ExprListContext* ctx);
  std::any visitCreateStruct(CelParser::CreateStructContext* ctx) override;
  std::any visitConstantLiteral(
      CelParser::ConstantLiteralContext* ctx) override;
  std::any visitPrimaryExpr(CelParser::PrimaryExprContext* ctx) override;
  std::any visitMemberExpr(CelParser::MemberExprContext* ctx) override;

  std::any visitMapInitializerList(
      CelParser::MapInitializerListContext* ctx) override;
  std::vector<MapExprEntry> visitEntries(
      CelParser::MapInitializerListContext* ctx);
  std::any visitInt(CelParser::IntContext* ctx) override;
  std::any visitUint(CelParser::UintContext* ctx) override;
  std::any visitDouble(CelParser::DoubleContext* ctx) override;
  std::any visitString(CelParser::StringContext* ctx) override;
  std::any visitBytes(CelParser::BytesContext* ctx) override;
  std::any visitBoolTrue(CelParser::BoolTrueContext* ctx) override;
  std::any visitBoolFalse(CelParser::BoolFalseContext* ctx) override;
  std::any visitNull(CelParser::NullContext* ctx) override;
  absl::Status GetSourceInfo(google::api::expr::v1alpha1::SourceInfo* source_info) const;
  EnrichedSourceInfo enriched_source_info() const;
  void syntaxError(antlr4::Recognizer* recognizer,
                   antlr4::Token* offending_symbol, size_t line, size_t col,
                   const std::string& msg, std::exception_ptr e) override;
  bool HasErrored() const;

  std::string ErrorMessage();

 private:
  template <typename... Args>
  Expr GlobalCallOrMacro(int64_t expr_id, absl::string_view function,
                         Args&&... args) {
    std::vector<Expr> arguments;
    arguments.reserve(sizeof...(Args));
    (arguments.push_back(std::forward<Args>(args)), ...);
    return GlobalCallOrMacroImpl(expr_id, function, std::move(arguments));
  }

  template <typename... Args>
  Expr ReceiverCallOrMacro(int64_t expr_id, absl::string_view function,
                           Expr target, Args&&... args) {
    std::vector<Expr> arguments;
    arguments.reserve(sizeof...(Args));
    (arguments.push_back(std::forward<Args>(args)), ...);
    return ReceiverCallOrMacroImpl(expr_id, function, std::move(target),
                                   std::move(arguments));
  }

  Expr GlobalCallOrMacroImpl(int64_t expr_id, absl::string_view function,
                             std::vector<Expr> args);
  Expr ReceiverCallOrMacroImpl(int64_t expr_id, absl::string_view function,
                               Expr target, std::vector<Expr> args);
  std::string ExtractQualifiedName(antlr4::ParserRuleContext* ctx,
                                   const Expr& e);
  // Attempt to unnest parse context.
  //
  // Walk the parse tree to the first complex term to reduce recursive depth in
  // the visit* calls.
  antlr4::tree::ParseTree* UnnestContext(antlr4::tree::ParseTree* tree);

 private:
  const cel::Source& source_;
  cel::ParserMacroExprFactory factory_;
  const cel::MacroRegistry& macro_registry_;
  int recursion_depth_;
  const int max_recursion_depth_;
  const bool add_macro_calls_;
  const bool enable_optional_syntax_;
};

ParserVisitor::ParserVisitor(const cel::Source& source,
                             const int max_recursion_depth,
                             const cel::MacroRegistry& macro_registry,
                             const bool add_macro_calls,
                             bool enable_optional_syntax)
    : source_(source),
      factory_(source_),
      macro_registry_(macro_registry),
      recursion_depth_(0),
      max_recursion_depth_(max_recursion_depth),
      add_macro_calls_(add_macro_calls),
      enable_optional_syntax_(enable_optional_syntax) {}

ParserVisitor::~ParserVisitor() {}

template <typename T, typename = std::enable_if_t<
                          std::is_base_of<antlr4::tree::ParseTree, T>::value>>
T* tree_as(antlr4::tree::ParseTree* tree) {
  return dynamic_cast<T*>(tree);
}

std::any ParserVisitor::visit(antlr4::tree::ParseTree* tree) {
  ScopedIncrement inc(recursion_depth_);
  if (recursion_depth_ > max_recursion_depth_) {
    return ExprToAny(factory_.ReportError(
        absl::StrFormat("Exceeded max recursion depth of %d when parsing.",
                        max_recursion_depth_)));
  }
  tree = UnnestContext(tree);
  if (auto* ctx = tree_as<CelParser::StartContext>(tree)) {
    return visitStart(ctx);
  } else if (auto* ctx = tree_as<CelParser::ExprContext>(tree)) {
    return visitExpr(ctx);
  } else if (auto* ctx = tree_as<CelParser::ConditionalAndContext>(tree)) {
    return visitConditionalAnd(ctx);
  } else if (auto* ctx = tree_as<CelParser::ConditionalOrContext>(tree)) {
    return visitConditionalOr(ctx);
  } else if (auto* ctx = tree_as<CelParser::RelationContext>(tree)) {
    return visitRelation(ctx);
  } else if (auto* ctx = tree_as<CelParser::CalcContext>(tree)) {
    return visitCalc(ctx);
  } else if (auto* ctx = tree_as<CelParser::LogicalNotContext>(tree)) {
    return visitLogicalNot(ctx);
  } else if (auto* ctx = tree_as<CelParser::PrimaryExprContext>(tree)) {
    return visitPrimaryExpr(ctx);
  } else if (auto* ctx = tree_as<CelParser::MemberExprContext>(tree)) {
    return visitMemberExpr(ctx);
  } else if (auto* ctx = tree_as<CelParser::SelectContext>(tree)) {
    return visitSelect(ctx);
  } else if (auto* ctx = tree_as<CelParser::MemberCallContext>(tree)) {
    return visitMemberCall(ctx);
  } else if (auto* ctx = tree_as<CelParser::MapInitializerListContext>(tree)) {
    return visitMapInitializerList(ctx);
  } else if (auto* ctx = tree_as<CelParser::NegateContext>(tree)) {
    return visitNegate(ctx);
  } else if (auto* ctx = tree_as<CelParser::IndexContext>(tree)) {
    return visitIndex(ctx);
  } else if (auto* ctx = tree_as<CelParser::UnaryContext>(tree)) {
    return visitUnary(ctx);
  } else if (auto* ctx = tree_as<CelParser::CreateListContext>(tree)) {
    return visitCreateList(ctx);
  } else if (auto* ctx = tree_as<CelParser::CreateMessageContext>(tree)) {
    return visitCreateMessage(ctx);
  } else if (auto* ctx = tree_as<CelParser::CreateStructContext>(tree)) {
    return visitCreateStruct(ctx);
  }

  if (tree) {
    return ExprToAny(
        factory_.ReportError(SourceRangeFromParserRuleContext(
                                 tree_as<antlr4::ParserRuleContext>(tree)),
                             "unknown parsetree type"));
  }
  return ExprToAny(factory_.ReportError("<<nil>> parsetree"));
}

std::any ParserVisitor::visitPrimaryExpr(CelParser::PrimaryExprContext* pctx) {
  CelParser::PrimaryContext* primary = pctx->primary();
  if (auto* ctx = tree_as<CelParser::NestedContext>(primary)) {
    return visitNested(ctx);
  } else if (auto* ctx =
                 tree_as<CelParser::IdentOrGlobalCallContext>(primary)) {
    return visitIdentOrGlobalCall(ctx);
  } else if (auto* ctx = tree_as<CelParser::CreateListContext>(primary)) {
    return visitCreateList(ctx);
  } else if (auto* ctx = tree_as<CelParser::CreateStructContext>(primary)) {
    return visitCreateStruct(ctx);
  } else if (auto* ctx = tree_as<CelParser::CreateMessageContext>(primary)) {
    return visitCreateMessage(ctx);
  } else if (auto* ctx = tree_as<CelParser::ConstantLiteralContext>(primary)) {
    return visitConstantLiteral(ctx);
  }
  if (factory_.HasErrors()) {
    // ANTLR creates PrimaryContext rather than a derived class during certain
    // error conditions. This is odd, but we ignore it as we already have errors
    // that occurred.
    return ExprToAny(factory_.NewUnspecified(factory_.NextId({})));
  }
  return ExprToAny(factory_.ReportError(SourceRangeFromParserRuleContext(pctx),
                                        "invalid primary expression"));
}

std::any ParserVisitor::visitMemberExpr(CelParser::MemberExprContext* mctx) {
  CelParser::MemberContext* member = mctx->member();
  if (auto* ctx = tree_as<CelParser::PrimaryExprContext>(member)) {
    return visitPrimaryExpr(ctx);
  } else if (auto* ctx = tree_as<CelParser::SelectContext>(member)) {
    return visitSelect(ctx);
  } else if (auto* ctx = tree_as<CelParser::MemberCallContext>(member)) {
    return visitMemberCall(ctx);
  } else if (auto* ctx = tree_as<CelParser::IndexContext>(member)) {
    return visitIndex(ctx);
  }
  return ExprToAny(factory_.ReportError(SourceRangeFromParserRuleContext(mctx),
                                        "unsupported simple expression"));
}

std::any ParserVisitor::visitStart(CelParser::StartContext* ctx) {
  return visit(ctx->expr());
}

antlr4::tree::ParseTree* ParserVisitor::UnnestContext(
    antlr4::tree::ParseTree* tree) {
  antlr4::tree::ParseTree* last = nullptr;
  while (tree != last) {
    last = tree;

    if (auto* ctx = tree_as<CelParser::StartContext>(tree)) {
      tree = ctx->expr();
    }

    if (auto* ctx = tree_as<CelParser::ExprContext>(tree)) {
      if (ctx->op != nullptr) {
        return ctx;
      }
      tree = ctx->e;
    }

    if (auto* ctx = tree_as<CelParser::ConditionalOrContext>(tree)) {
      if (!ctx->ops.empty()) {
        return ctx;
      }
      tree = ctx->e;
    }

    if (auto* ctx = tree_as<CelParser::ConditionalAndContext>(tree)) {
      if (!ctx->ops.empty()) {
        return ctx;
      }
      tree = ctx->e;
    }

    if (auto* ctx = tree_as<CelParser::RelationContext>(tree)) {
      if (ctx->calc() == nullptr) {
        return ctx;
      }
      tree = ctx->calc();
    }

    if (auto* ctx = tree_as<CelParser::CalcContext>(tree)) {
      if (ctx->unary() == nullptr) {
        return ctx;
      }
      tree = ctx->unary();
    }

    if (auto* ctx = tree_as<CelParser::MemberExprContext>(tree)) {
      tree = ctx->member();
    }

    if (auto* ctx = tree_as<CelParser::PrimaryExprContext>(tree)) {
      if (auto* nested = tree_as<CelParser::NestedContext>(ctx->primary())) {
        tree = nested->e;
      } else {
        return ctx;
      }
    }
  }

  return tree;
}

std::any ParserVisitor::visitExpr(CelParser::ExprContext* ctx) {
  auto result = ExprFromAny(visit(ctx->e));
  if (!ctx->op) {
    return ExprToAny(std::move(result));
  }
  std::vector<Expr> arguments;
  arguments.reserve(3);
  arguments.push_back(std::move(result));
  int64_t op_id = factory_.NextId(SourceRangeFromToken(ctx->op));
  arguments.push_back(ExprFromAny(visit(ctx->e1)));
  arguments.push_back(ExprFromAny(visit(ctx->e2)));

  return ExprToAny(
      factory_.NewCall(op_id, CelOperator::CONDITIONAL, std::move(arguments)));
}

std::any ParserVisitor::visitConditionalOr(
    CelParser::ConditionalOrContext* ctx) {
  auto result = ExprFromAny(visit(ctx->e));
  if (ctx->ops.empty()) {
    return ExprToAny(std::move(result));
  }
  ExpressionBalancer b(factory_, CelOperator::LOGICAL_OR, std::move(result));
  for (size_t i = 0; i < ctx->ops.size(); ++i) {
    auto op = ctx->ops[i];
    if (i >= ctx->e1.size()) {
      return ExprToAny(
          factory_.ReportError(SourceRangeFromParserRuleContext(ctx),
                               "unexpected character, wanted '||'"));
    }
    auto next = ExprFromAny(visit(ctx->e1[i]));
    int64_t op_id = factory_.NextId(SourceRangeFromToken(op));
    b.AddTerm(op_id, std::move(next));
  }
  return ExprToAny(b.Balance());
}

std::any ParserVisitor::visitConditionalAnd(
    CelParser::ConditionalAndContext* ctx) {
  auto result = ExprFromAny(visit(ctx->e));
  if (ctx->ops.empty()) {
    return ExprToAny(std::move(result));
  }
  ExpressionBalancer b(factory_, CelOperator::LOGICAL_AND, std::move(result));
  for (size_t i = 0; i < ctx->ops.size(); ++i) {
    auto op = ctx->ops[i];
    if (i >= ctx->e1.size()) {
      return ExprToAny(
          factory_.ReportError(SourceRangeFromParserRuleContext(ctx),
                               "unexpected character, wanted '&&'"));
    }
    auto next = ExprFromAny(visit(ctx->e1[i]));
    int64_t op_id = factory_.NextId(SourceRangeFromToken(op));
    b.AddTerm(op_id, std::move(next));
  }
  return ExprToAny(b.Balance());
}

std::any ParserVisitor::visitRelation(CelParser::RelationContext* ctx) {
  if (ctx->calc()) {
    return visit(ctx->calc());
  }
  std::string op_text;
  if (ctx->op) {
    op_text = ctx->op->getText();
  }
  auto op = ReverseLookupOperator(op_text);
  if (op) {
    auto lhs = ExprFromAny(visit(ctx->relation(0)));
    int64_t op_id = factory_.NextId(SourceRangeFromToken(ctx->op));
    auto rhs = ExprFromAny(visit(ctx->relation(1)));
    return ExprToAny(
        GlobalCallOrMacro(op_id, *op, std::move(lhs), std::move(rhs)));
  }
  return ExprToAny(factory_.ReportError(SourceRangeFromParserRuleContext(ctx),
                                        "operator not found"));
}

std::any ParserVisitor::visitCalc(CelParser::CalcContext* ctx) {
  if (ctx->unary()) {
    return visit(ctx->unary());
  }
  std::string op_text;
  if (ctx->op) {
    op_text = ctx->op->getText();
  }
  auto op = ReverseLookupOperator(op_text);
  if (op) {
    auto lhs = ExprFromAny(visit(ctx->calc(0)));
    int64_t op_id = factory_.NextId(SourceRangeFromToken(ctx->op));
    auto rhs = ExprFromAny(visit(ctx->calc(1)));
    return ExprToAny(
        GlobalCallOrMacro(op_id, *op, std::move(lhs), std::move(rhs)));
  }
  return ExprToAny(factory_.ReportError(SourceRangeFromParserRuleContext(ctx),
                                        "operator not found"));
}

std::any ParserVisitor::visitUnary(CelParser::UnaryContext* ctx) {
  return ExprToAny(factory_.NewStringConst(
      factory_.NextId(SourceRangeFromParserRuleContext(ctx)), "<<error>>"));
}

std::any ParserVisitor::visitLogicalNot(CelParser::LogicalNotContext* ctx) {
  if (ctx->ops.size() % 2 == 0) {
    return visit(ctx->member());
  }
  int64_t op_id = factory_.NextId(SourceRangeFromToken(ctx->ops[0]));
  auto target = ExprFromAny(visit(ctx->member()));
  return ExprToAny(
      GlobalCallOrMacro(op_id, CelOperator::LOGICAL_NOT, std::move(target)));
}

std::any ParserVisitor::visitNegate(CelParser::NegateContext* ctx) {
  if (ctx->ops.size() % 2 == 0) {
    return visit(ctx->member());
  }
  int64_t op_id = factory_.NextId(SourceRangeFromToken(ctx->ops[0]));
  auto target = ExprFromAny(visit(ctx->member()));
  return ExprToAny(
      GlobalCallOrMacro(op_id, CelOperator::NEGATE, std::move(target)));
}

std::any ParserVisitor::visitSelect(CelParser::SelectContext* ctx) {
  auto operand = ExprFromAny(visit(ctx->member()));
  // Handle the error case where no valid identifier is specified.
  if (!ctx->id || !ctx->op) {
    return ExprToAny(factory_.NewUnspecified(
        factory_.NextId(SourceRangeFromParserRuleContext(ctx))));
  }
  auto id = ctx->id->getText();
  if (ctx->opt != nullptr) {
    if (!enable_optional_syntax_) {
      return ExprToAny(factory_.ReportError(
          SourceRangeFromParserRuleContext(ctx), "unsupported syntax '.?'"));
    }
    auto op_id = factory_.NextId(SourceRangeFromToken(ctx->op));
    std::vector<Expr> arguments;
    arguments.reserve(2);
    arguments.push_back(std::move(operand));
    arguments.push_back(factory_.NewStringConst(
        factory_.NextId(SourceRangeFromParserRuleContext(ctx)), std::move(id)));
    return ExprToAny(factory_.NewCall(op_id, "_?._", std::move(arguments)));
  }
  return ExprToAny(
      factory_.NewSelect(factory_.NextId(SourceRangeFromToken(ctx->op)),
                         std::move(operand), std::move(id)));
}

std::any ParserVisitor::visitMemberCall(CelParser::MemberCallContext* ctx) {
  auto operand = ExprFromAny(visit(ctx->member()));
  // Handle the error case where no valid identifier is specified.
  if (!ctx->id) {
    return ExprToAny(factory_.NewUnspecified(
        factory_.NextId(SourceRangeFromParserRuleContext(ctx))));
  }
  auto id = ctx->id->getText();
  int64_t op_id = factory_.NextId(SourceRangeFromToken(ctx->open));
  auto args = visitList(ctx->args);
  return ExprToAny(
      ReceiverCallOrMacroImpl(op_id, id, std::move(operand), std::move(args)));
}

std::any ParserVisitor::visitIndex(CelParser::IndexContext* ctx) {
  auto target = ExprFromAny(visit(ctx->member()));
  int64_t op_id = factory_.NextId(SourceRangeFromToken(ctx->op));
  auto index = ExprFromAny(visit(ctx->index));
  if (!enable_optional_syntax_ && ctx->opt != nullptr) {
    return ExprToAny(factory_.ReportError(SourceRangeFromParserRuleContext(ctx),
                                          "unsupported syntax '.?'"));
  }
  return ExprToAny(GlobalCallOrMacro(
      op_id, ctx->opt != nullptr ? "_[?_]" : CelOperator::INDEX,
      std::move(target), std::move(index)));
}

std::any ParserVisitor::visitCreateMessage(
    CelParser::CreateMessageContext* ctx) {
  std::vector<std::string> parts;
  parts.reserve(ctx->ids.size());
  for (const auto* id : ctx->ids) {
    parts.push_back(id->getText());
  }
  std::string name;
  if (ctx->leadingDot) {
    name.push_back('.');
    name.append(absl::StrJoin(parts, "."));
  } else {
    name = absl::StrJoin(parts, ".");
  }
  int64_t obj_id = factory_.NextId(SourceRangeFromToken(ctx->op));
  std::vector<StructExprField> fields;
  if (ctx->entries) {
    fields = visitFields(ctx->entries);
  }
  return ExprToAny(
      factory_.NewStruct(obj_id, std::move(name), std::move(fields)));
}

std::any ParserVisitor::visitFieldInitializerList(
    CelParser::FieldInitializerListContext* ctx) {
  return ExprToAny(factory_.ReportError(SourceRangeFromParserRuleContext(ctx),
                                        "<<unreachable>>"));
}

std::vector<StructExprField> ParserVisitor::visitFields(
    CelParser::FieldInitializerListContext* ctx) {
  std::vector<StructExprField> res;
  if (!ctx || ctx->fields.empty()) {
    return res;
  }

  res.reserve(ctx->fields.size());
  for (size_t i = 0; i < ctx->fields.size(); ++i) {
    if (i >= ctx->cols.size() || i >= ctx->values.size()) {
      // This is the result of a syntax error detected elsewhere.
      return res;
    }
    const auto* f = ctx->fields[i];
    if (f->id == nullptr) {
      ABSL_DCHECK(HasErrored());
      // This is the result of a syntax error detected elsewhere.
      return res;
    }
    int64_t init_id = factory_.NextId(SourceRangeFromToken(ctx->cols[i]));
    if (!enable_optional_syntax_ && f->opt) {
      factory_.ReportError(SourceRangeFromParserRuleContext(ctx),
                           "unsupported syntax '?'");
      continue;
    }
    auto value = ExprFromAny(visit(ctx->values[i]));
    res.push_back(factory_.NewStructField(init_id, f->id->getText(),
                                          std::move(value), f->opt != nullptr));
  }

  return res;
}

std::any ParserVisitor::visitIdentOrGlobalCall(
    CelParser::IdentOrGlobalCallContext* ctx) {
  std::string ident_name;
  if (ctx->leadingDot) {
    ident_name = ".";
  }
  if (!ctx->id) {
    return ExprToAny(factory_.NewUnspecified(
        factory_.NextId(SourceRangeFromParserRuleContext(ctx))));
  }
  if (cel::internal::LexisIsReserved(ctx->id->getText())) {
    return ExprToAny(factory_.ReportError(
        SourceRangeFromParserRuleContext(ctx),
        absl::StrFormat("reserved identifier: %s", ctx->id->getText())));
  }
  // check if ID is in reserved identifiers
  ident_name += ctx->id->getText();
  if (ctx->op) {
    int64_t op_id = factory_.NextId(SourceRangeFromToken(ctx->op));
    auto args = visitList(ctx->args);
    return ExprToAny(
        GlobalCallOrMacroImpl(op_id, std::move(ident_name), std::move(args)));
  }
  return ExprToAny(factory_.NewIdent(
      factory_.NextId(SourceRangeFromToken(ctx->id)), std::move(ident_name)));
}

std::any ParserVisitor::visitNested(CelParser::NestedContext* ctx) {
  return visit(ctx->e);
}

std::any ParserVisitor::visitCreateList(CelParser::CreateListContext* ctx) {
  int64_t list_id = factory_.NextId(SourceRangeFromToken(ctx->op));
  auto elems = visitList(ctx->elems);
  return ExprToAny(factory_.NewList(list_id, std::move(elems)));
}

std::vector<ListExprElement> ParserVisitor::visitList(
    CelParser::ListInitContext* ctx) {
  std::vector<ListExprElement> rv;
  if (!ctx) return rv;
  rv.reserve(ctx->elems.size());
  for (size_t i = 0; i < ctx->elems.size(); ++i) {
    auto* expr_ctx = ctx->elems[i];
    if (expr_ctx == nullptr) {
      return rv;
    }
    if (!enable_optional_syntax_ && expr_ctx->opt != nullptr) {
      factory_.ReportError(SourceRangeFromParserRuleContext(ctx),
                           "unsupported syntax '?'");
      rv.push_back(factory_.NewListElement(factory_.NewUnspecified(0), false));
      continue;
    }
    rv.push_back(factory_.NewListElement(ExprFromAny(visitExpr(expr_ctx->e)),
                                         expr_ctx->opt != nullptr));
  }
  return rv;
}

std::vector<Expr> ParserVisitor::visitList(CelParser::ExprListContext* ctx) {
  std::vector<Expr> rv;
  if (!ctx) return rv;
  std::transform(ctx->e.begin(), ctx->e.end(), std::back_inserter(rv),
                 [this](CelParser::ExprContext* expr_ctx) {
                   return ExprFromAny(visitExpr(expr_ctx));
                 });
  return rv;
}

std::any ParserVisitor::visitCreateStruct(CelParser::CreateStructContext* ctx) {
  int64_t struct_id = factory_.NextId(SourceRangeFromToken(ctx->op));
  std::vector<MapExprEntry> entries;
  if (ctx->entries) {
    entries = visitEntries(ctx->entries);
  }
  return ExprToAny(factory_.NewMap(struct_id, std::move(entries)));
}

std::any ParserVisitor::visitConstantLiteral(
    CelParser::ConstantLiteralContext* clctx) {
  CelParser::LiteralContext* literal = clctx->literal();
  if (auto* ctx = tree_as<CelParser::IntContext>(literal)) {
    return visitInt(ctx);
  } else if (auto* ctx = tree_as<CelParser::UintContext>(literal)) {
    return visitUint(ctx);
  } else if (auto* ctx = tree_as<CelParser::DoubleContext>(literal)) {
    return visitDouble(ctx);
  } else if (auto* ctx = tree_as<CelParser::StringContext>(literal)) {
    return visitString(ctx);
  } else if (auto* ctx = tree_as<CelParser::BytesContext>(literal)) {
    return visitBytes(ctx);
  } else if (auto* ctx = tree_as<CelParser::BoolFalseContext>(literal)) {
    return visitBoolFalse(ctx);
  } else if (auto* ctx = tree_as<CelParser::BoolTrueContext>(literal)) {
    return visitBoolTrue(ctx);
  } else if (auto* ctx = tree_as<CelParser::NullContext>(literal)) {
    return visitNull(ctx);
  }
  return ExprToAny(factory_.ReportError(SourceRangeFromParserRuleContext(clctx),
                                        "invalid constant literal expression"));
}

std::any ParserVisitor::visitMapInitializerList(
    CelParser::MapInitializerListContext* ctx) {
  return ExprToAny(factory_.ReportError(SourceRangeFromParserRuleContext(ctx),
                                        "<<unreachable>>"));
}

std::vector<MapExprEntry> ParserVisitor::visitEntries(
    CelParser::MapInitializerListContext* ctx) {
  std::vector<MapExprEntry> res;
  if (!ctx || ctx->keys.empty()) {
    return res;
  }

  res.reserve(ctx->cols.size());
  for (size_t i = 0; i < ctx->cols.size(); ++i) {
    auto id = factory_.NextId(SourceRangeFromToken(ctx->cols[i]));
    if (!enable_optional_syntax_ && ctx->keys[i]->opt) {
      factory_.ReportError(SourceRangeFromParserRuleContext(ctx),
                           "unsupported syntax '?'");
      res.push_back(factory_.NewMapEntry(0, factory_.NewUnspecified(0),
                                         factory_.NewUnspecified(0), false));
      continue;
    }
    auto key = ExprFromAny(visit(ctx->keys[i]->e));
    auto value = ExprFromAny(visit(ctx->values[i]));
    res.push_back(factory_.NewMapEntry(id, std::move(key), std::move(value),
                                       ctx->keys[i]->opt != nullptr));
  }
  return res;
}

std::any ParserVisitor::visitInt(CelParser::IntContext* ctx) {
  std::string value;
  if (ctx->sign) {
    value = ctx->sign->getText();
  }
  value += ctx->tok->getText();
  int64_t int_value;
  if (absl::StartsWith(ctx->tok->getText(), "0x")) {
    if (absl::SimpleHexAtoi(value, &int_value)) {
      return ExprToAny(factory_.NewIntConst(
          factory_.NextId(SourceRangeFromParserRuleContext(ctx)), int_value));
    } else {
      return ExprToAny(factory_.ReportError(
          SourceRangeFromParserRuleContext(ctx), "invalid hex int literal"));
    }
  }
  if (absl::SimpleAtoi(value, &int_value)) {
    return ExprToAny(factory_.NewIntConst(
        factory_.NextId(SourceRangeFromParserRuleContext(ctx)), int_value));
  } else {
    return ExprToAny(factory_.ReportError(SourceRangeFromParserRuleContext(ctx),
                                          "invalid int literal"));
  }
}

std::any ParserVisitor::visitUint(CelParser::UintContext* ctx) {
  std::string value = ctx->tok->getText();
  // trim the 'u' designator included in the uint literal.
  if (!value.empty()) {
    value.resize(value.size() - 1);
  }
  uint64_t uint_value;
  if (absl::StartsWith(ctx->tok->getText(), "0x")) {
    if (absl::SimpleHexAtoi(value, &uint_value)) {
      return ExprToAny(factory_.NewUintConst(
          factory_.NextId(SourceRangeFromParserRuleContext(ctx)), uint_value));
    } else {
      return ExprToAny(factory_.ReportError(
          SourceRangeFromParserRuleContext(ctx), "invalid hex uint literal"));
    }
  }
  if (absl::SimpleAtoi(value, &uint_value)) {
    return ExprToAny(factory_.NewUintConst(
        factory_.NextId(SourceRangeFromParserRuleContext(ctx)), uint_value));
  } else {
    return ExprToAny(factory_.ReportError(SourceRangeFromParserRuleContext(ctx),
                                          "invalid uint literal"));
  }
}

std::any ParserVisitor::visitDouble(CelParser::DoubleContext* ctx) {
  std::string value;
  if (ctx->sign) {
    value = ctx->sign->getText();
  }
  value += ctx->tok->getText();
  double double_value;
  if (absl::SimpleAtod(value, &double_value)) {
    return ExprToAny(factory_.NewDoubleConst(
        factory_.NextId(SourceRangeFromParserRuleContext(ctx)), double_value));
  } else {
    return ExprToAny(factory_.ReportError(SourceRangeFromParserRuleContext(ctx),
                                          "invalid double literal"));
  }
}

std::any ParserVisitor::visitString(CelParser::StringContext* ctx) {
  auto status_or_value = cel::internal::ParseStringLiteral(ctx->tok->getText());
  if (!status_or_value.ok()) {
    return ExprToAny(factory_.ReportError(SourceRangeFromParserRuleContext(ctx),
                                          status_or_value.status().message()));
  }
  return ExprToAny(factory_.NewStringConst(
      factory_.NextId(SourceRangeFromParserRuleContext(ctx)),
      std::move(status_or_value).value()));
}

std::any ParserVisitor::visitBytes(CelParser::BytesContext* ctx) {
  auto status_or_value = cel::internal::ParseBytesLiteral(ctx->tok->getText());
  if (!status_or_value.ok()) {
    return ExprToAny(factory_.ReportError(SourceRangeFromParserRuleContext(ctx),
                                          status_or_value.status().message()));
  }
  return ExprToAny(factory_.NewBytesConst(
      factory_.NextId(SourceRangeFromParserRuleContext(ctx)),
      std::move(status_or_value).value()));
}

std::any ParserVisitor::visitBoolTrue(CelParser::BoolTrueContext* ctx) {
  return ExprToAny(factory_.NewBoolConst(
      factory_.NextId(SourceRangeFromParserRuleContext(ctx)), true));
}

std::any ParserVisitor::visitBoolFalse(CelParser::BoolFalseContext* ctx) {
  return ExprToAny(factory_.NewBoolConst(
      factory_.NextId(SourceRangeFromParserRuleContext(ctx)), false));
}

std::any ParserVisitor::visitNull(CelParser::NullContext* ctx) {
  return ExprToAny(factory_.NewNullConst(
      factory_.NextId(SourceRangeFromParserRuleContext(ctx))));
}

absl::Status ParserVisitor::GetSourceInfo(
    google::api::expr::v1alpha1::SourceInfo* source_info) const {
  source_info->set_location(source_.description());
  for (const auto& positions : factory_.positions()) {
    source_info->mutable_positions()->insert(
        std::pair{positions.first, positions.second.begin});
  }
  source_info->mutable_line_offsets()->Reserve(source_.line_offsets().size());
  for (const auto& line_offset : source_.line_offsets()) {
    source_info->mutable_line_offsets()->Add(line_offset);
  }
  for (const auto& macro_call : factory_.macro_calls()) {
    google::api::expr::v1alpha1::Expr macro_call_proto;
    CEL_RETURN_IF_ERROR(cel::extensions::protobuf_internal::ExprToProto(
        macro_call.second, &macro_call_proto));
    source_info->mutable_macro_calls()->insert(
        std::pair{macro_call.first, std::move(macro_call_proto)});
  }
  return absl::OkStatus();
}

EnrichedSourceInfo ParserVisitor::enriched_source_info() const {
  std::map<int64_t, std::pair<int32_t, int32_t>> offsets;
  for (const auto& positions : factory_.positions()) {
    offsets.insert(
        std::pair{positions.first,
                  std::pair{positions.second.begin, positions.second.end - 1}});
  }
  return EnrichedSourceInfo(std::move(offsets));
}

void ParserVisitor::syntaxError(antlr4::Recognizer* recognizer,
                                antlr4::Token* offending_symbol, size_t line,
                                size_t col, const std::string& msg,
                                std::exception_ptr e) {
  cel::SourceRange range;
  if (auto position = source_.GetPosition(cel::SourceLocation{
          static_cast<int32_t>(line), static_cast<int32_t>(col)});
      position) {
    range.begin = *position;
  }
  factory_.ReportError(range, absl::StrCat("Syntax error: ", msg));
}

bool ParserVisitor::HasErrored() const { return factory_.HasErrors(); }

std::string ParserVisitor::ErrorMessage() { return factory_.ErrorMessage(); }

Expr ParserVisitor::GlobalCallOrMacroImpl(int64_t expr_id,
                                          absl::string_view function,
                                          std::vector<Expr> args) {
  if (auto macro = macro_registry_.FindMacro(function, args.size(), false);
      macro) {
    std::vector<Expr> macro_args;
    if (add_macro_calls_) {
      macro_args.reserve(args.size());
      for (const auto& arg : args) {
        macro_args.push_back(factory_.BuildMacroCallArg(arg));
      }
    }
    factory_.BeginMacro(factory_.GetSourceRange(expr_id));
    auto expr = macro->Expand(factory_, absl::nullopt, absl::MakeSpan(args));
    factory_.EndMacro();
    if (expr) {
      if (add_macro_calls_) {
        factory_.AddMacroCall(expr->id(), function, absl::nullopt,
                              std::move(macro_args));
      }
      // We did not end up using `expr_id`. Delete metadata.
      factory_.EraseId(expr_id);
      return std::move(*expr);
    }
  }

  return factory_.NewCall(expr_id, function, std::move(args));
}

Expr ParserVisitor::ReceiverCallOrMacroImpl(int64_t expr_id,
                                            absl::string_view function,
                                            Expr target,
                                            std::vector<Expr> args) {
  if (auto macro = macro_registry_.FindMacro(function, args.size(), true);
      macro) {
    Expr macro_target;
    std::vector<Expr> macro_args;
    if (add_macro_calls_) {
      macro_args.reserve(args.size());
      macro_target = factory_.BuildMacroCallArg(target);
      for (const auto& arg : args) {
        macro_args.push_back(factory_.BuildMacroCallArg(arg));
      }
    }
    factory_.BeginMacro(factory_.GetSourceRange(expr_id));
    auto expr = macro->Expand(factory_, std::ref(target), absl::MakeSpan(args));
    factory_.EndMacro();
    if (expr) {
      if (add_macro_calls_) {
        factory_.AddMacroCall(expr->id(), function, std::move(macro_target),
                              std::move(macro_args));
      }
      // We did not end up using `expr_id`. Delete metadata.
      factory_.EraseId(expr_id);
      return std::move(*expr);
    }
  }
  return factory_.NewMemberCall(expr_id, function, std::move(target),
                                std::move(args));
}

std::string ParserVisitor::ExtractQualifiedName(antlr4::ParserRuleContext* ctx,
                                                const Expr& e) {
  if (e == Expr{}) {
    return "";
  }

  if (const auto* ident_expr = absl::get_if<IdentExpr>(&e.kind()); ident_expr) {
    return ident_expr->name();
  }
  if (const auto* select_expr = absl::get_if<SelectExpr>(&e.kind());
      select_expr) {
    std::string prefix = ExtractQualifiedName(ctx, select_expr->operand());
    if (!prefix.empty()) {
      return absl::StrCat(prefix, ".", select_expr->field());
    }
  }
  factory_.ReportError(factory_.GetSourceRange(e.id()),
                       "expected a qualified name");
  return "";
}

// Replacements for absl::StrReplaceAll for escaping standard whitespace
// characters.
static constexpr auto kStandardReplacements =
    std::array<std::pair<absl::string_view, absl::string_view>, 3>{
        std::make_pair("\n", "\\n"),
        std::make_pair("\r", "\\r"),
        std::make_pair("\t", "\\t"),
    };

static constexpr absl::string_view kSingleQuote = "'";

// ExprRecursionListener extends the standard ANTLR CelParser to ensure that
// recursive entries into the 'expr' rule are limited to a configurable depth so
// as to prevent stack overflows.
class ExprRecursionListener final : public ParseTreeListener {
 public:
  explicit ExprRecursionListener(
      const int max_recursion_depth = kDefaultMaxRecursionDepth)
      : max_recursion_depth_(max_recursion_depth), recursion_depth_(0) {}
  ~ExprRecursionListener() override {}

  void visitTerminal(TerminalNode* node) override {};
  void visitErrorNode(ErrorNode* error) override {};
  void enterEveryRule(ParserRuleContext* ctx) override;
  void exitEveryRule(ParserRuleContext* ctx) override;

 private:
  const int max_recursion_depth_;
  int recursion_depth_;
};

void ExprRecursionListener::enterEveryRule(ParserRuleContext* ctx) {
  // Throw a ParseCancellationException since the parsing would otherwise
  // continue if this were treated as a syntax error and the problem would
  // continue to manifest.
  if (ctx->getRuleIndex() == CelParser::RuleExpr) {
    if (recursion_depth_ > max_recursion_depth_) {
      throw ParseCancellationException(
          absl::StrFormat("Expression recursion limit exceeded. limit: %d",
                          max_recursion_depth_));
    }
    recursion_depth_++;
  }
}

void ExprRecursionListener::exitEveryRule(ParserRuleContext* ctx) {
  if (ctx->getRuleIndex() == CelParser::RuleExpr) {
    recursion_depth_--;
  }
}

class RecoveryLimitErrorStrategy final : public DefaultErrorStrategy {
 public:
  explicit RecoveryLimitErrorStrategy(
      int recovery_limit = kDefaultErrorRecoveryLimit,
      int recovery_token_lookahead_limit =
          kDefaultErrorRecoveryTokenLookaheadLimit)
      : recovery_limit_(recovery_limit),
        recovery_attempts_(0),
        recovery_token_lookahead_limit_(recovery_token_lookahead_limit) {}

  void recover(Parser* recognizer, std::exception_ptr e) override {
    checkRecoveryLimit(recognizer);
    DefaultErrorStrategy::recover(recognizer, e);
  }

  Token* recoverInline(Parser* recognizer) override {
    checkRecoveryLimit(recognizer);
    return DefaultErrorStrategy::recoverInline(recognizer);
  }

  // Override the ANTLR implementation to introduce a token lookahead limit as
  // this prevents pathologically constructed, yet small (< 16kb) inputs from
  // consuming inordinate amounts of compute.
  //
  // This method is only called on error recovery paths.
  void consumeUntil(Parser* recognizer, const IntervalSet& set) override {
    size_t ttype = recognizer->getInputStream()->LA(1);
    int recovery_search_depth = 0;
    while (ttype != Token::EOF && !set.contains(ttype) &&
           recovery_search_depth++ < recovery_token_lookahead_limit_) {
      recognizer->consume();
      ttype = recognizer->getInputStream()->LA(1);
    }
    // Halt all parsing if the lookahead limit is reached during error recovery.
    if (recovery_search_depth == recovery_token_lookahead_limit_) {
      throw ParseCancellationException("Unable to find a recovery token");
    }
  }

 protected:
  std::string escapeWSAndQuote(const std::string& s) const override {
    std::string result;
    result.reserve(s.size() + 2);
    absl::StrAppend(&result, kSingleQuote, s, kSingleQuote);
    absl::StrReplaceAll(kStandardReplacements, &result);
    return result;
  }

 private:
  void checkRecoveryLimit(Parser* recognizer) {
    if (recovery_attempts_++ >= recovery_limit_) {
      std::string too_many_errors =
          absl::StrFormat("More than %d parse errors.", recovery_limit_);
      recognizer->notifyErrorListeners(too_many_errors);
      throw ParseCancellationException(too_many_errors);
    }
  }

  int recovery_limit_;
  int recovery_attempts_;
  int recovery_token_lookahead_limit_;
};

}  // namespace

absl::StatusOr<ParsedExpr> Parse(absl::string_view expression,
                                 absl::string_view description,
                                 const ParserOptions& options) {
  std::vector<Macro> macros = Macro::AllMacros();
  if (options.enable_optional_syntax) {
    macros.push_back(cel::OptMapMacro());
    macros.push_back(cel::OptFlatMapMacro());
  }
  return ParseWithMacros(expression, macros, description, options);
}

absl::StatusOr<ParsedExpr> ParseWithMacros(absl::string_view expression,
                                           const std::vector<Macro>& macros,
                                           absl::string_view description,
                                           const ParserOptions& options) {
  CEL_ASSIGN_OR_RETURN(auto verbose_parsed_expr,
                       EnrichedParse(expression, macros, description, options));
  return verbose_parsed_expr.parsed_expr();
}

absl::StatusOr<VerboseParsedExpr> EnrichedParse(
    absl::string_view expression, const std::vector<Macro>& macros,
    absl::string_view description, const ParserOptions& options) {
  CEL_ASSIGN_OR_RETURN(auto source,
                       cel::NewSource(expression, std::string(description)));
  cel::MacroRegistry macro_registry;
  CEL_RETURN_IF_ERROR(macro_registry.RegisterMacros(macros));
  return EnrichedParse(*source, macro_registry, options);
}

absl::StatusOr<VerboseParsedExpr> EnrichedParse(
    const cel::Source& source, const cel::MacroRegistry& registry,
    const ParserOptions& options) {
  try {
    CodePointStream input(source.content(), source.description());
    if (input.size() > options.expression_size_codepoint_limit) {
      return absl::InvalidArgumentError(absl::StrCat(
          "expression size exceeds codepoint limit.", " input size: ",
          input.size(), ", limit: ", options.expression_size_codepoint_limit));
    }
    CelLexer lexer(&input);
    CommonTokenStream tokens(&lexer);
    CelParser parser(&tokens);
    ExprRecursionListener listener(options.max_recursion_depth);
    ParserVisitor visitor(source, options.max_recursion_depth, registry,
                          options.add_macro_calls,
                          options.enable_optional_syntax);

    lexer.removeErrorListeners();
    parser.removeErrorListeners();
    lexer.addErrorListener(&visitor);
    parser.addErrorListener(&visitor);
    parser.addParseListener(&listener);

    // Limit the number of error recovery attempts to prevent bad expressions
    // from consuming lots of cpu / memory.
    parser.setErrorHandler(std::make_shared<RecoveryLimitErrorStrategy>(
        options.error_recovery_limit,
        options.error_recovery_token_lookahead_limit));

    Expr expr;
    try {
      expr = ExprFromAny(visitor.visit(parser.start()));
    } catch (const ParseCancellationException& e) {
      if (visitor.HasErrored()) {
        return absl::InvalidArgumentError(visitor.ErrorMessage());
      }
      return absl::CancelledError(e.what());
    }

    if (visitor.HasErrored()) {
      return absl::InvalidArgumentError(visitor.ErrorMessage());
    }

    // root is deleted as part of the parser context
    ParsedExpr parsed_expr;
    CEL_RETURN_IF_ERROR(cel::extensions::protobuf_internal::ExprToProto(
        expr, parsed_expr.mutable_expr()));
    CEL_RETURN_IF_ERROR(
        visitor.GetSourceInfo(parsed_expr.mutable_source_info()));
    auto enriched_source_info = visitor.enriched_source_info();
    return VerboseParsedExpr(std::move(parsed_expr),
                             std::move(enriched_source_info));
  } catch (const std::exception& e) {
    return absl::AbortedError(e.what());
  } catch (const char* what) {
    // ANTLRv4 has historically thrown C string literals.
    return absl::AbortedError(what);
  } catch (...) {
    // We guarantee to never throw and always return a status.
    return absl::UnknownError("An unknown exception occurred");
  }
}

absl::StatusOr<google::api::expr::v1alpha1::ParsedExpr> Parse(
    const cel::Source& source, const cel::MacroRegistry& registry,
    const ParserOptions& options) {
  CEL_ASSIGN_OR_RETURN(auto verbose_expr,
                       EnrichedParse(source, registry, options));
  return verbose_expr.parsed_expr();
}

}  // namespace google::api::expr::parser
