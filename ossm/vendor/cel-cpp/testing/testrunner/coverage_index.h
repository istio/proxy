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

#ifndef THIRD_PARTY_CEL_CPP_TESTING_TESTRUNNER_COVERAGE_INDEX_H_
#define THIRD_PARTY_CEL_CPP_TESTING_TESTRUNNER_COVERAGE_INDEX_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "common/value.h"
#include "eval/public/cel_expression.h"
#include "runtime/runtime.h"
#include "tools/navigable_ast.h"

namespace cel::test {
inline constexpr absl::string_view kDigraphHeader = "digraph {\n";
inline constexpr absl::string_view kUncoveredNodeStyle =
    R"(color="indianred2", style=filled)";
inline constexpr absl::string_view kPartiallyCoveredNodeStyle =
    R"(color="lightyellow", style=filled)";
inline constexpr absl::string_view kCompletelyCoveredNodeStyle =
    R"(color="lightgreen", style=filled)";

// `CoverageIndex` is a utility for tracking expression coverage based on the
// Abstract Syntax Tree (AST) of a `cel::expr::CheckedExpr`.
//
// To use `CoverageIndex`, it must first be initialized with a
// `cel::expr::CheckedExpr` using the `Init` method. This allows the
// index to build up a representation of all the nodes and potential boolean
// branches within the expression.
//
// The `CoverageIndex` is then integrated with the CEL evaluation process.
// This is done by enabling coverage either in a `cel::Runtime` or a
// `google::api::expr::runtime::CelExpressionBuilder` using the provided helper
// functions (`EnableCoverageInRuntime` or
// `EnableCoverageInCelExpressionBuilder`). When integrated, the CEL evaluation
// engine will call `RecordCoverage` for each visited expression node, allowing
// `CoverageIndex` to track which parts of the expression were executed and,
// for boolean-producing nodes, which branches were taken (true/false).
//
// After evaluation, a `CoverageReport` can be generated, summarizing the
// executed nodes and branches, and highlighting any unencountered parts of
// the expression.
class CoverageIndex {
 public:
  struct NodeCoverageStats {
    bool is_boolean_node = false;
    bool covered = false;
    bool has_true_branch = false;
    bool has_false_branch = false;
  };

  struct CoverageReport {
    std::string cel_expression;
    int64_t nodes = 0;
    int64_t covered_nodes = 0;
    int64_t branches = 0;
    int64_t covered_boolean_outcomes = 0;
    std::vector<std::string> unencountered_nodes;
    std::vector<std::string> unencountered_branches;
    std::string dot_graph;
  };

  // Initializes the coverage index with the given checked expression.
  //
  // The coverage index will be initialized with an entry for each node in the
  // AST.
  void Init(const cel::expr::CheckedExpr& checked_expr);

  // Records coverage for the given node.
  //
  // The coverage index will be updated with the coverage information for the
  // given node.
  void RecordCoverage(int64_t node_id, const cel::Value& value);

  // Returns a coverage report for the given checked expression.
  CoverageReport GetCoverageReport() const;

 private:
  absl::flat_hash_map<int64_t, NodeCoverageStats> node_coverage_stats_;
  NavigableProtoAst navigable_ast_;
  cel::expr::CheckedExpr checked_expr_;
};

// Enables coverage tracking within the provided `cel::Runtime`.
// Note: This function ties the `runtime` instance to a single expression.
// Do not reuse this `runtime` instance with multiple expressions when coverage
// is enabled, as the `coverage_index` will accumulate results across different
// expressions, leading to incorrect coverage reports.
absl::Status EnableCoverageInCelExpressionBuilder(
    google::api::expr::runtime::CelExpressionBuilder& cel_expression_builder,
    CoverageIndex& coverage_index);

// Enables coverage tracking within the provided `CelExpressionBuilder`.
// Note: This function ties the `cel_expression_builder` instance to a single
// expression. Do not reuse this `cel_expression_builder` instance with
// multiple expressions when coverage is enabled, as the `coverage_index` will
// accumulate results across different expressions, leading to incorrect
// coverage reports.
absl::Status EnableCoverageInRuntime(cel::Runtime& runtime,
                                     CoverageIndex& coverage_index);

}  // namespace cel::test

#endif  // THIRD_PARTY_CEL_CPP_TESTING_TESTRUNNER_COVERAGE_INDEX_H_
