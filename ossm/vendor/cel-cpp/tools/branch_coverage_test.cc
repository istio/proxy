// Copyright 2024 Google LLC
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

#include "tools/branch_coverage.h"

#include <cstdint>
#include <string>

#include "absl/base/no_destructor.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/strings/substitute.h"
#include "base/builtins.h"
#include "common/value.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_value.h"
#include "internal/proto_file_util.h"
#include "internal/testing.h"
#include "tools/navigable_ast.h"
#include "google/protobuf/arena.h"

namespace cel {
namespace {

using ::cel::internal::test::ReadTextProtoFromFile;
using ::cel::expr::CheckedExpr;
using ::google::api::expr::runtime::Activation;
using ::google::api::expr::runtime::CelValue;
using ::google::api::expr::runtime::CreateCelExpressionBuilder;
using ::google::api::expr::runtime::RegisterBuiltinFunctions;

// int1 < int2 &&
// (43 > 42) &&
// !(bool1 || bool2) &&
// 4 / int_divisor >= 1 &&
// (ternary_c ? ternary_t : ternary_f)
constexpr char kCoverageExamplePath[] =
    "tools/testdata/coverage_example.textproto";

const CheckedExpr& TestExpression() {
  static absl::NoDestructor<CheckedExpr> expression([]() {
    CheckedExpr value;
    ABSL_CHECK_OK(ReadTextProtoFromFile(kCoverageExamplePath, value));
    return value;
  }());
  return *expression;
}

std::string FormatNodeStats(const BranchCoverage::NodeCoverageStats& stats) {
  return absl::Substitute(
      "is_bool: $0; evaluated: $1; bool_true: $2; bool_false: $3; error: $4",
      stats.is_boolean, stats.evaluation_count, stats.boolean_true_count,
      stats.boolean_false_count, stats.error_count);
}

google::api::expr::runtime::CelEvaluationListener EvaluationListenerForCoverage(
    BranchCoverage* coverage) {
  return [coverage](int64_t id, const CelValue& value, google::protobuf::Arena* arena) {
    coverage->RecordLegacyValue(id, value);
    return absl::OkStatus();
  };
}

MATCHER_P(MatchesNodeStats, expected, "") {
  const BranchCoverage::NodeCoverageStats& actual = arg;

  *result_listener << "\n";
  *result_listener << "Expected: " << FormatNodeStats(expected);
  *result_listener << "\n";
  *result_listener << "Got: " << FormatNodeStats(actual);

  return actual.is_boolean == expected.is_boolean &&
         actual.evaluation_count == expected.evaluation_count &&
         actual.boolean_true_count == expected.boolean_true_count &&
         actual.boolean_false_count == expected.boolean_false_count &&
         actual.error_count == expected.error_count;
}

MATCHER(NodeStatsIsBool, "") {
  const BranchCoverage::NodeCoverageStats& actual = arg;

  *result_listener << "\n";
  *result_listener << "Expected: " << FormatNodeStats({true, 0, 0, 0, 0});
  *result_listener << "\n";
  *result_listener << "Got: " << FormatNodeStats(actual);

  return actual.is_boolean == true;
}

TEST(BranchCoverage, DefaultsForUntrackedId) {
  auto coverage = CreateBranchCoverage(TestExpression());

  using Stats = BranchCoverage::NodeCoverageStats;

  EXPECT_THAT(coverage->StatsForNode(99),
              MatchesNodeStats(Stats{/*is_boolean=*/false,
                                     /*evaluation_count=*/0,
                                     /*boolean_true_count=*/0,
                                     /*boolean_false_count=*/0,
                                     /*error_count=*/0}));
}

TEST(BranchCoverage, Record) {
  auto coverage = CreateBranchCoverage(TestExpression());

  int64_t root_id = coverage->expr().expr().id();

  coverage->Record(root_id, cel::BoolValue(false));

  using Stats = BranchCoverage::NodeCoverageStats;

  EXPECT_THAT(coverage->StatsForNode(root_id),
              MatchesNodeStats(Stats{/*is_boolean=*/true,
                                     /*evaluation_count=*/1,
                                     /*boolean_true_count=*/0,
                                     /*boolean_false_count=*/1,
                                     /*error_count=*/0}));
}

TEST(BranchCoverage, RecordUnexpectedId) {
  auto coverage = CreateBranchCoverage(TestExpression());

  int64_t unexpected_id = 99;

  coverage->Record(unexpected_id, cel::BoolValue(false));

  using Stats = BranchCoverage::NodeCoverageStats;

  EXPECT_THAT(coverage->StatsForNode(unexpected_id),
              MatchesNodeStats(Stats{/*is_boolean=*/true,
                                     /*evaluation_count=*/1,
                                     /*boolean_true_count=*/0,
                                     /*boolean_false_count=*/1,
                                     /*error_count=*/0}));
}

TEST(BranchCoverage, IncrementsCounters) {
  auto coverage = CreateBranchCoverage(TestExpression());

  EXPECT_TRUE(static_cast<bool>(coverage->ast()));

  auto builder = CreateCelExpressionBuilder();
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  // int1 < int2 &&
  // (43 > 42) &&
  // !(bool1 || bool2) &&
  // 4 / int_divisor >= 1 &&
  // (ternary_c ? ternary_t : ternary_f)
  ASSERT_OK_AND_ASSIGN(auto program,
                       builder->CreateExpression(&TestExpression()));

  google::protobuf::Arena arena;
  Activation activation;
  activation.InsertValue("bool1", CelValue::CreateBool(false));
  activation.InsertValue("bool2", CelValue::CreateBool(false));

  activation.InsertValue("int1", CelValue::CreateInt64(42));
  activation.InsertValue("int2", CelValue::CreateInt64(43));

  activation.InsertValue("int_divisor", CelValue::CreateInt64(4));

  activation.InsertValue("ternary_c", CelValue::CreateBool(true));
  activation.InsertValue("ternary_t", CelValue::CreateBool(true));
  activation.InsertValue("ternary_f", CelValue::CreateBool(false));

  ASSERT_OK_AND_ASSIGN(
      auto result,
      program->Trace(activation, &arena,
                     EvaluationListenerForCoverage(coverage.get())));

  EXPECT_TRUE(result.IsBool() && result.BoolOrDie() == true);

  using Stats = BranchCoverage::NodeCoverageStats;
  const NavigableProtoAst& ast = coverage->ast();
  auto root_node_stats = coverage->StatsForNode(ast.Root().expr()->id());

  EXPECT_THAT(root_node_stats, MatchesNodeStats(Stats{/*is_boolean=*/true,
                                                      /*evaluation_count=*/1,
                                                      /*boolean_true_count=*/1,
                                                      /*boolean_false_count=*/0,
                                                      /*error_count=*/0}));

  const NavigableProtoAstNode* ternary;
  for (const auto& node : ast.Root().DescendantsPreorder()) {
    if (node.node_kind() == NodeKind::kCall &&
        node.expr()->call_expr().function() == cel::builtin::kTernary) {
      ternary = &node;
      break;
    }
  }

  ASSERT_NE(ternary, nullptr);
  auto ternary_node_stats = coverage->StatsForNode(ternary->expr()->id());
  // Ternary gets optimized to conditional jumps, so it isn't instrumented
  // directly in stack machine impl.
  EXPECT_THAT(ternary_node_stats, NodeStatsIsBool());

  const auto* false_node = ternary->children().at(2);
  auto false_node_stats = coverage->StatsForNode(false_node->expr()->id());
  EXPECT_THAT(false_node_stats,
              MatchesNodeStats(Stats{/*is_boolean=*/true,
                                     /*evaluation_count=*/0,
                                     /*boolean_true_count=*/0,
                                     /*boolean_false_count=*/0,
                                     /*error_count=*/0}));

  const NavigableProtoAstNode* not_arg_expr;
  for (const auto& node : ast.Root().DescendantsPreorder()) {
    if (node.node_kind() == NodeKind::kCall &&
        node.expr()->call_expr().function() == cel::builtin::kNot) {
      not_arg_expr = node.children().at(0);
      break;
    }
  }

  ASSERT_NE(not_arg_expr, nullptr);
  auto not_expr_node_stats = coverage->StatsForNode(not_arg_expr->expr()->id());
  EXPECT_THAT(not_expr_node_stats,
              MatchesNodeStats(Stats{/*is_boolean=*/true,
                                     /*evaluation_count=*/1,
                                     /*boolean_true_count=*/0,
                                     /*boolean_false_count=*/1,
                                     /*error_count=*/0}));

  const NavigableProtoAstNode* div_expr;
  for (const auto& node : ast.Root().DescendantsPreorder()) {
    if (node.node_kind() == NodeKind::kCall &&
        node.expr()->call_expr().function() == cel::builtin::kDivide) {
      div_expr = &node;
      break;
    }
  }

  ASSERT_NE(div_expr, nullptr);
  auto div_expr_stats = coverage->StatsForNode(div_expr->expr()->id());
  EXPECT_THAT(div_expr_stats, MatchesNodeStats(Stats{/*is_boolean=*/false,
                                                     /*evaluation_count=*/1,
                                                     /*boolean_true_count=*/0,
                                                     /*boolean_false_count=*/0,
                                                     /*error_count=*/0}));
}

TEST(BranchCoverage, AccumulatesAcrossRuns) {
  auto coverage = CreateBranchCoverage(TestExpression());

  EXPECT_TRUE(static_cast<bool>(coverage->ast()));

  auto builder = CreateCelExpressionBuilder();
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  // int1 < int2 &&
  // (43 > 42) &&
  // !(bool1 || bool2) &&
  // 4 / int_divisor >= 1 &&
  // (ternary_c ? ternary_t : ternary_f)
  ASSERT_OK_AND_ASSIGN(auto program,
                       builder->CreateExpression(&TestExpression()));

  google::protobuf::Arena arena;
  Activation activation;
  activation.InsertValue("bool1", CelValue::CreateBool(false));
  activation.InsertValue("bool2", CelValue::CreateBool(false));

  activation.InsertValue("int1", CelValue::CreateInt64(42));
  activation.InsertValue("int2", CelValue::CreateInt64(43));

  activation.InsertValue("int_divisor", CelValue::CreateInt64(4));

  activation.InsertValue("ternary_c", CelValue::CreateBool(true));
  activation.InsertValue("ternary_t", CelValue::CreateBool(true));
  activation.InsertValue("ternary_f", CelValue::CreateBool(false));

  ASSERT_OK_AND_ASSIGN(
      auto result,
      program->Trace(activation, &arena,
                     EvaluationListenerForCoverage(coverage.get())));

  EXPECT_TRUE(result.IsBool() && result.BoolOrDie() == true);

  activation.RemoveValueEntry("ternary_c");
  activation.RemoveValueEntry("ternary_f");

  activation.InsertValue("ternary_c", CelValue::CreateBool(false));
  activation.InsertValue("ternary_f", CelValue::CreateBool(false));

  ASSERT_OK_AND_ASSIGN(
      result, program->Trace(activation, &arena,
                             EvaluationListenerForCoverage(coverage.get())));

  EXPECT_TRUE(result.IsBool() && result.BoolOrDie() == false)
      << result.DebugString();

  using Stats = BranchCoverage::NodeCoverageStats;
  const NavigableProtoAst& ast = coverage->ast();
  auto root_node_stats = coverage->StatsForNode(ast.Root().expr()->id());

  EXPECT_THAT(root_node_stats, MatchesNodeStats(Stats{/*is_boolean=*/true,
                                                      /*evaluation_count=*/2,
                                                      /*boolean_true_count=*/1,
                                                      /*boolean_false_count=*/1,
                                                      /*error_count=*/0}));

  const NavigableProtoAstNode* ternary;
  for (const auto& node : ast.Root().DescendantsPreorder()) {
    if (node.node_kind() == NodeKind::kCall &&
        node.expr()->call_expr().function() == cel::builtin::kTernary) {
      ternary = &node;
      break;
    }
  }

  ASSERT_NE(ternary, nullptr);
  auto ternary_node_stats = coverage->StatsForNode(ternary->expr()->id());

  // Ternary gets optimized into conditional jumps for stack machine plan.
  EXPECT_THAT(ternary_node_stats, NodeStatsIsBool());

  const auto* false_node = ternary->children().at(2);
  auto false_node_stats = coverage->StatsForNode(false_node->expr()->id());
  EXPECT_THAT(false_node_stats,
              MatchesNodeStats(Stats{/*is_boolean=*/true,
                                     /*evaluation_count=*/1,
                                     /*boolean_true_count=*/0,
                                     /*boolean_false_count=*/1,
                                     /*error_count=*/0}));
}

TEST(BranchCoverage, CountsErrors) {
  auto coverage = CreateBranchCoverage(TestExpression());

  EXPECT_TRUE(static_cast<bool>(coverage->ast()));

  auto builder = CreateCelExpressionBuilder();
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  // int1 < int2 &&
  // (43 > 42) &&
  // !(bool1 || bool2) &&
  // 4 / int_divisor >= 1 &&
  // (ternary_c ? ternary_t : ternary_f)
  ASSERT_OK_AND_ASSIGN(auto program,
                       builder->CreateExpression(&TestExpression()));

  google::protobuf::Arena arena;
  Activation activation;
  activation.InsertValue("bool1", CelValue::CreateBool(false));
  activation.InsertValue("bool2", CelValue::CreateBool(false));

  activation.InsertValue("int1", CelValue::CreateInt64(42));
  activation.InsertValue("int2", CelValue::CreateInt64(43));

  activation.InsertValue("int_divisor", CelValue::CreateInt64(0));

  activation.InsertValue("ternary_c", CelValue::CreateBool(true));
  activation.InsertValue("ternary_t", CelValue::CreateBool(false));
  activation.InsertValue("ternary_f", CelValue::CreateBool(false));

  ASSERT_OK_AND_ASSIGN(
      auto result,
      program->Trace(activation, &arena,
                     EvaluationListenerForCoverage(coverage.get())));

  EXPECT_TRUE(result.IsBool() && result.BoolOrDie() == false);

  using Stats = BranchCoverage::NodeCoverageStats;
  const NavigableProtoAst& ast = coverage->ast();
  auto root_node_stats = coverage->StatsForNode(ast.Root().expr()->id());

  EXPECT_THAT(root_node_stats, MatchesNodeStats(Stats{/*is_boolean=*/true,
                                                      /*evaluation_count=*/1,
                                                      /*boolean_true_count=*/0,
                                                      /*boolean_false_count=*/1,
                                                      /*error_count=*/0}));

  const NavigableProtoAstNode* ternary;
  for (const auto& node : ast.Root().DescendantsPreorder()) {
    if (node.node_kind() == NodeKind::kCall &&
        node.expr()->call_expr().function() == cel::builtin::kTernary) {
      ternary = &node;
      break;
    }
  }

  const NavigableProtoAstNode* div_expr;
  for (const auto& node : ast.Root().DescendantsPreorder()) {
    if (node.node_kind() == NodeKind::kCall &&
        node.expr()->call_expr().function() == cel::builtin::kDivide) {
      div_expr = &node;
      break;
    }
  }

  ASSERT_NE(div_expr, nullptr);
  auto div_expr_stats = coverage->StatsForNode(div_expr->expr()->id());
  EXPECT_THAT(div_expr_stats, MatchesNodeStats(Stats{/*is_boolean=*/false,
                                                     /*evaluation_count=*/1,
                                                     /*boolean_true_count=*/0,
                                                     /*boolean_false_count=*/0,
                                                     /*error_count=*/1}));
}

}  // namespace
}  // namespace cel
