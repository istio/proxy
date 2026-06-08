// Copyright 2025 Google LLC.
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

#include "testing/testrunner/coverage_index.h"

#include <cstdint>
#include <string>
#include <vector>

#include "cel/expr/syntax.pb.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "common/ast.h"
#include "common/value.h"
#include "eval/compiler/cel_expression_builder_flat_impl.h"
#include "eval/compiler/instrumentation.h"
#include "eval/public/cel_expression.h"
#include "internal/casts.h"
#include "runtime/internal/runtime_impl.h"
#include "runtime/runtime.h"
#include "tools/cel_unparser.h"
#include "tools/navigable_ast.h"

namespace cel::test {
namespace {

using ::cel::expr::CheckedExpr;
using ::cel::expr::Type;
using ::google::api::expr::runtime::CelExpressionBuilder;
using ::google::api::expr::runtime::Instrumentation;
using ::google::api::expr::runtime::InstrumentationFactory;

std::string EscapeSpecialCharacters(absl::string_view expr_text) {
  return absl::StrReplaceAll(expr_text, {{"\\\"", "\""},
                                         {"\"", "\\\""},
                                         {"\n", "\\n"},
                                         {"||", " \\| \\| "},
                                         {"<", "\\<"},
                                         {">", "\\>"},
                                         {"{", "\\{"},
                                         {"}", "\\}"}});
}

std::string KindToString(const NavigableProtoAstNode& node) {
  if (node.parent_relation() != ChildKind::kUnspecified &&
      node.parent()->expr()->has_comprehension_expr()) {
    const cel::expr::Expr::Comprehension& comp =
        node.parent()->expr()->comprehension_expr();
    if (node.expr()->id() == comp.iter_range().id()) return "IterRange";
    if (node.expr()->id() == comp.accu_init().id()) return "AccuInit";
    if (node.expr()->id() == comp.loop_condition().id()) return "LoopCondition";
    if (node.expr()->id() == comp.loop_step().id()) return "LoopStep";
    if (node.expr()->id() == comp.result().id()) return "Result";
  }

  return absl::StrCat(NodeKindName(node.node_kind()), " Node");
}

const Type* absl_nullable FindCheckerType(const CheckedExpr& expr,
                                          int64_t expr_id) {
  if (auto it = expr.type_map().find(expr_id); it != expr.type_map().end()) {
    return &it->second;
  }
  return nullptr;
}

bool InferredBooleanNode(const CheckedExpr& checked_expr,
                         const NavigableProtoAstNode& node) {
  int64_t node_id = node.expr()->id();
  const auto* checker_type = FindCheckerType(checked_expr, node_id);
  if (checker_type != nullptr) {
    return checker_type->has_primitive() &&
           checker_type->primitive() == Type::BOOL;
  }

  return false;
}

void TraverseAndCalculateCoverage(
    const CheckedExpr& checked_expr, const NavigableProtoAstNode& node,
    const absl::flat_hash_map<int64_t, CoverageIndex::NodeCoverageStats>&
        stats_map,
    bool log_unencountered, std::string preceeding_tabs,
    CoverageIndex::CoverageReport& report, std::string& dot_graph) {
  int64_t node_id = node.expr()->id();

  const CoverageIndex::NodeCoverageStats& stats = stats_map.at(node_id);
  report.nodes++;

  absl::StatusOr<std::string> unparsed =
      google::api::expr::Unparse(*node.expr());
  std::string expr_text = unparsed.ok() ? *unparsed : "unparse_failed";

  bool is_interesting_bool_node =
      stats.is_boolean_node && !node.expr()->has_const_expr() &&
      (!node.expr()->has_call_expr() ||
       node.expr()->call_expr().function() != "cel.@block");

  absl::string_view node_coverage_style = kUncoveredNodeStyle;
  if (stats.covered) {
    if (is_interesting_bool_node) {
      if (stats.has_true_branch && stats.has_false_branch) {
        node_coverage_style = kCompletelyCoveredNodeStyle;
      } else {
        node_coverage_style = kPartiallyCoveredNodeStyle;
      }
    } else {
      node_coverage_style = kCompletelyCoveredNodeStyle;
    }
  }
  std::string escaped_expr_text = EscapeSpecialCharacters(expr_text);
  dot_graph += absl::StrFormat(
      "%d [shape=record, %s, label=\"{<1> exprID: %d | <2> %s} | <3> %s\"];\n",
      node_id, node_coverage_style, node_id, KindToString(node),
      escaped_expr_text);

  bool node_covered = stats.covered;
  if (node_covered) {
    report.covered_nodes++;
  } else if (log_unencountered) {
    if (is_interesting_bool_node) {
      report.unencountered_nodes.push_back(
          absl::StrCat("Expression ID ", node_id, " ('", expr_text, "')"));
    }
    log_unencountered = false;
  }

  if (is_interesting_bool_node) {
    report.branches += 2;
    if (stats.has_true_branch) {
      report.covered_boolean_outcomes++;
    } else if (log_unencountered) {
      report.unencountered_branches.push_back(
          absl::StrCat("\n", preceeding_tabs, "Expression ID ", node_id, " ('",
                       expr_text, "'): Never evaluated to 'true'"));
      preceeding_tabs += "\t\t";
    }
    if (stats.has_false_branch) {
      report.covered_boolean_outcomes++;
    } else if (log_unencountered) {
      report.unencountered_branches.push_back(
          absl::StrCat("\n", preceeding_tabs, "Expression ID ", node_id, " ('",
                       expr_text, "'): Never evaluated to 'false'"));
      preceeding_tabs += "\t\t";
    }
  }

  for (const auto* child : node.children()) {
    dot_graph += absl::StrFormat("%d -> %d;\n", node_id, child->expr()->id());
    TraverseAndCalculateCoverage(checked_expr, *child, stats_map,
                                 log_unencountered, preceeding_tabs, report,
                                 dot_graph);
  }
}

}  // namespace

void CoverageIndex::RecordCoverage(int64_t node_id, const cel::Value& value) {
  NodeCoverageStats& stats = node_coverage_stats_[node_id];
  stats.covered = true;
  if (node_coverage_stats_[node_id].is_boolean_node && value.IsBool()) {
    if (value.AsBool()->NativeValue()) {
      stats.has_true_branch = true;
    } else {
      stats.has_false_branch = true;
    }
  }
}

void CoverageIndex::Init(const cel::expr::CheckedExpr& checked_expr) {
  checked_expr_ = checked_expr;
  navigable_ast_ = NavigableProtoAst::Build(checked_expr_.expr());
  for (const auto& node : navigable_ast_.Root().DescendantsPreorder()) {
    NodeCoverageStats stats;
    stats.is_boolean_node = InferredBooleanNode(checked_expr_, node);
    node_coverage_stats_[node.expr()->id()] = stats;
  }
}

CoverageIndex::CoverageReport CoverageIndex::GetCoverageReport() const {
  CoverageReport report;
  if (node_coverage_stats_.empty()) {
    return report;
  }

  std::string dot_graph = std::string(kDigraphHeader);
  TraverseAndCalculateCoverage(checked_expr_, navigable_ast_.Root(),
                               node_coverage_stats_, true, "", report,
                               dot_graph);
  dot_graph += "}\n";
  report.dot_graph = dot_graph;
  report.cel_expression =
      google::api::expr::Unparse(checked_expr_).value_or("");
  return report;
}

InstrumentationFactory InstrumentationFactoryForCoverage(
    CoverageIndex& coverage_index) {
  return [&](const cel::Ast& ast) -> Instrumentation {
    return [&](int64_t node_id, const cel::Value& value) -> absl::Status {
      coverage_index.RecordCoverage(node_id, value);
      return absl::OkStatus();
    };
  };
}

absl::Status EnableCoverageInRuntime(cel::Runtime& runtime,
                                     CoverageIndex& coverage_index) {
  auto& runtime_impl =
      cel::internal::down_cast<runtime_internal::RuntimeImpl&>(runtime);
  runtime_impl.expr_builder().AddProgramOptimizer(
      google::api::expr::runtime::CreateInstrumentationExtension(
          InstrumentationFactoryForCoverage(coverage_index)));
  return absl::OkStatus();
}

absl::Status EnableCoverageInCelExpressionBuilder(
    CelExpressionBuilder& cel_expression_builder,
    CoverageIndex& coverage_index) {
  auto& cel_expression_builder_impl = cel::internal::down_cast<
      google::api::expr::runtime::CelExpressionBuilderFlatImpl&>(
      cel_expression_builder);
  cel_expression_builder_impl.flat_expr_builder().AddProgramOptimizer(
      google::api::expr::runtime::CreateInstrumentationExtension(
          InstrumentationFactoryForCoverage(coverage_index)));
  return absl::OkStatus();
}

}  // namespace cel::test
