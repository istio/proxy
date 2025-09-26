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

#include "eval/compiler/instrumentation.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "cel/expr/syntax.pb.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "common/ast/ast_impl.h"
#include "common/value.h"
#include "eval/compiler/constant_folding.h"
#include "eval/compiler/flat_expr_builder.h"
#include "eval/compiler/regex_precompilation_optimization.h"
#include "eval/eval/evaluator_core.h"
#include "extensions/protobuf/ast_converters.h"
#include "internal/testing.h"
#include "parser/parser.h"
#include "runtime/activation.h"
#include "runtime/function_registry.h"
#include "runtime/internal/runtime_env.h"
#include "runtime/internal/runtime_env_testing.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_functions.h"
#include "runtime/type_registry.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {
namespace {

using ::cel::IntValue;
using ::cel::Value;
using ::cel::runtime_internal::NewTestingRuntimeEnv;
using ::cel::runtime_internal::RuntimeEnv;
using ::cel::expr::ParsedExpr;
using ::google::api::expr::parser::Parse;
using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

class InstrumentationTest : public ::testing::Test {
 public:
  InstrumentationTest()
      : env_(NewTestingRuntimeEnv()),
        function_registry_(env_->function_registry),
        type_registry_(env_->type_registry) {}
  void SetUp() override {
    ASSERT_OK(cel::RegisterStandardFunctions(function_registry_, options_));
  }

 protected:
  absl_nonnull std::shared_ptr<RuntimeEnv> env_;
  cel::RuntimeOptions options_;
  cel::FunctionRegistry& function_registry_;
  cel::TypeRegistry& type_registry_;
  google::protobuf::Arena arena_;
};

MATCHER_P(IsIntValue, expected, "") {
  const Value& got = arg;

  return got.Is<IntValue>() && got.GetInt().NativeValue() == expected;
}

TEST_F(InstrumentationTest, Basic) {
  FlatExprBuilder builder(env_, options_);

  std::vector<int64_t> expr_ids;
  Instrumentation expr_id_recorder =
      [&expr_ids](int64_t expr_id, const cel::Value&) -> absl::Status {
    expr_ids.push_back(expr_id);
    return absl::OkStatus();
  };

  builder.AddProgramOptimizer(CreateInstrumentationExtension(
      [=](const cel::ast_internal::AstImpl&) -> Instrumentation {
        return expr_id_recorder;
      }));

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, Parse("1 + 2 + 3"));
  ASSERT_OK_AND_ASSIGN(auto ast,
                       cel::extensions::CreateAstFromParsedExpr(expr));
  ASSERT_OK_AND_ASSIGN(auto plan,
                       builder.CreateExpressionImpl(std::move(ast),
                                                    /*issues=*/nullptr));

  auto state = plan.MakeEvaluatorState(env_->descriptor_pool.get(),
                                       env_->MutableMessageFactory(), &arena_);
  cel::Activation activation;

  ASSERT_OK_AND_ASSIGN(
      auto value,
      plan.EvaluateWithCallback(activation, EvaluationListener(), state));

  // AST for the test expression:
  //              + <4>
  //             /     \
  //          +<2>     3<5>
  //         /    \
  //      1<1>   2<3>
  EXPECT_THAT(expr_ids, ElementsAre(1, 3, 2, 5, 4));
}

TEST_F(InstrumentationTest, BasicWithConstFolding) {
  FlatExprBuilder builder(env_, options_);

  absl::flat_hash_map<int64_t, cel::Value> expr_id_to_value;
  Instrumentation expr_id_recorder = [&expr_id_to_value](
                                         int64_t expr_id,
                                         const cel::Value& v) -> absl::Status {
    expr_id_to_value[expr_id] = v;
    return absl::OkStatus();
  };
  builder.AddProgramOptimizer(
      cel::runtime_internal::CreateConstantFoldingOptimizer());
  builder.AddProgramOptimizer(CreateInstrumentationExtension(
      [=](const cel::ast_internal::AstImpl&) -> Instrumentation {
        return expr_id_recorder;
      }));

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, Parse("1 + 2 + 3"));
  ASSERT_OK_AND_ASSIGN(auto ast,
                       cel::extensions::CreateAstFromParsedExpr(expr));
  ASSERT_OK_AND_ASSIGN(auto plan,
                       builder.CreateExpressionImpl(std::move(ast),
                                                    /*issues=*/nullptr));

  EXPECT_THAT(
      expr_id_to_value,
      UnorderedElementsAre(Pair(1, IsIntValue(1)), Pair(3, IsIntValue(2)),
                           Pair(2, IsIntValue(3)), Pair(5, IsIntValue(3))));
  expr_id_to_value.clear();

  auto state = plan.MakeEvaluatorState(env_->descriptor_pool.get(),
                                       env_->MutableMessageFactory(), &arena_);
  cel::Activation activation;

  ASSERT_OK_AND_ASSIGN(
      auto value,
      plan.EvaluateWithCallback(activation, EvaluationListener(), state));

  // AST for the test expression:
  //              + <4>
  //             /     \
  //          +<2>     3<5>
  //         /    \
  //      1<1>   2<3>
  EXPECT_THAT(expr_id_to_value, UnorderedElementsAre(Pair(4, IsIntValue(6))));
}

TEST_F(InstrumentationTest, AndShortCircuit) {
  FlatExprBuilder builder(env_, options_);

  std::vector<int64_t> expr_ids;
  Instrumentation expr_id_recorder =
      [&expr_ids](int64_t expr_id, const cel::Value&) -> absl::Status {
    expr_ids.push_back(expr_id);
    return absl::OkStatus();
  };

  builder.AddProgramOptimizer(CreateInstrumentationExtension(
      [=](const cel::ast_internal::AstImpl&) -> Instrumentation {
        return expr_id_recorder;
      }));

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, Parse("a && b"));
  ASSERT_OK_AND_ASSIGN(auto ast,
                       cel::extensions::CreateAstFromParsedExpr(expr));
  ASSERT_OK_AND_ASSIGN(auto plan,
                       builder.CreateExpressionImpl(std::move(ast),
                                                    /*issues=*/nullptr));

  auto state = plan.MakeEvaluatorState(env_->descriptor_pool.get(),
                                       env_->MutableMessageFactory(), &arena_);
  cel::Activation activation;

  activation.InsertOrAssignValue("a", cel::BoolValue(true));
  activation.InsertOrAssignValue("b", cel::BoolValue(false));

  ASSERT_OK_AND_ASSIGN(
      auto value,
      plan.EvaluateWithCallback(activation, EvaluationListener(), state));

  EXPECT_THAT(expr_ids, ElementsAre(1, 2, 3));

  activation.InsertOrAssignValue("a", cel::BoolValue(false));

  ASSERT_OK_AND_ASSIGN(value, plan.EvaluateWithCallback(
                                  activation, EvaluationListener(), state));

  EXPECT_THAT(expr_ids, ElementsAre(1, 2, 3, 1, 3));
}

TEST_F(InstrumentationTest, OrShortCircuit) {
  FlatExprBuilder builder(env_, options_);

  std::vector<int64_t> expr_ids;
  Instrumentation expr_id_recorder =
      [&expr_ids](int64_t expr_id, const cel::Value&) -> absl::Status {
    expr_ids.push_back(expr_id);
    return absl::OkStatus();
  };

  builder.AddProgramOptimizer(CreateInstrumentationExtension(
      [=](const cel::ast_internal::AstImpl&) -> Instrumentation {
        return expr_id_recorder;
      }));

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, Parse("a || b"));
  ASSERT_OK_AND_ASSIGN(auto ast,
                       cel::extensions::CreateAstFromParsedExpr(expr));
  ASSERT_OK_AND_ASSIGN(auto plan,
                       builder.CreateExpressionImpl(std::move(ast),
                                                    /*issues=*/nullptr));

  auto state = plan.MakeEvaluatorState(env_->descriptor_pool.get(),
                                       env_->MutableMessageFactory(), &arena_);
  cel::Activation activation;

  activation.InsertOrAssignValue("a", cel::BoolValue(false));
  activation.InsertOrAssignValue("b", cel::BoolValue(true));

  ASSERT_OK_AND_ASSIGN(
      auto value,
      plan.EvaluateWithCallback(activation, EvaluationListener(), state));

  EXPECT_THAT(expr_ids, ElementsAre(1, 2, 3));
  expr_ids.clear();
  activation.InsertOrAssignValue("a", cel::BoolValue(true));

  ASSERT_OK_AND_ASSIGN(value, plan.EvaluateWithCallback(
                                  activation, EvaluationListener(), state));

  EXPECT_THAT(expr_ids, ElementsAre(1, 3));
}

TEST_F(InstrumentationTest, Ternary) {
  FlatExprBuilder builder(env_, options_);

  std::vector<int64_t> expr_ids;
  Instrumentation expr_id_recorder =
      [&expr_ids](int64_t expr_id, const cel::Value&) -> absl::Status {
    expr_ids.push_back(expr_id);
    return absl::OkStatus();
  };

  builder.AddProgramOptimizer(CreateInstrumentationExtension(
      [=](const cel::ast_internal::AstImpl&) -> Instrumentation {
        return expr_id_recorder;
      }));

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, Parse("(c)? a : b"));
  ASSERT_OK_AND_ASSIGN(auto ast,
                       cel::extensions::CreateAstFromParsedExpr(expr));
  ASSERT_OK_AND_ASSIGN(auto plan,
                       builder.CreateExpressionImpl(std::move(ast),
                                                    /*issues=*/nullptr));

  auto state = plan.MakeEvaluatorState(env_->descriptor_pool.get(),
                                       env_->MutableMessageFactory(), &arena_);
  cel::Activation activation;

  activation.InsertOrAssignValue("c", cel::BoolValue(true));
  activation.InsertOrAssignValue("a", cel::IntValue(1));
  activation.InsertOrAssignValue("b", cel::IntValue(2));

  ASSERT_OK_AND_ASSIGN(
      auto value,
      plan.EvaluateWithCallback(activation, EvaluationListener(), state));

  // AST
  //       ?:() <2>
  //      /   |    \
  // c <1>  a <3>   b <4>
  EXPECT_THAT(expr_ids, ElementsAre(1, 3, 2));
  expr_ids.clear();

  activation.InsertOrAssignValue("c", cel::BoolValue(false));

  ASSERT_OK_AND_ASSIGN(value, plan.EvaluateWithCallback(
                                  activation, EvaluationListener(), state));

  EXPECT_THAT(expr_ids, ElementsAre(1, 4, 2));
  expr_ids.clear();
}

TEST_F(InstrumentationTest, OptimizedStepsNotEvaluated) {
  FlatExprBuilder builder(env_, options_);

  builder.AddProgramOptimizer(CreateRegexPrecompilationExtension(0));

  std::vector<int64_t> expr_ids;
  Instrumentation expr_id_recorder =
      [&expr_ids](int64_t expr_id, const cel::Value&) -> absl::Status {
    expr_ids.push_back(expr_id);
    return absl::OkStatus();
  };

  builder.AddProgramOptimizer(CreateInstrumentationExtension(
      [=](const cel::ast_internal::AstImpl&) -> Instrumentation {
        return expr_id_recorder;
      }));

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr,
                       Parse("r'test_string'.matches(r'[a-z_]+')"));
  ASSERT_OK_AND_ASSIGN(auto ast,
                       cel::extensions::CreateAstFromParsedExpr(expr));
  ASSERT_OK_AND_ASSIGN(auto plan,
                       builder.CreateExpressionImpl(std::move(ast),
                                                    /*issues=*/nullptr));

  auto state = plan.MakeEvaluatorState(env_->descriptor_pool.get(),
                                       env_->MutableMessageFactory(), &arena_);
  cel::Activation activation;

  ASSERT_OK_AND_ASSIGN(
      auto value,
      plan.EvaluateWithCallback(activation, EvaluationListener(), state));

  EXPECT_THAT(expr_ids, ElementsAre(1, 2));
  EXPECT_TRUE(value.Is<cel::BoolValue>() && value.GetBool().NativeValue());
}

TEST_F(InstrumentationTest, NoopSkipped) {
  FlatExprBuilder builder(env_, options_);

  builder.AddProgramOptimizer(CreateInstrumentationExtension(
      [=](const cel::ast_internal::AstImpl&) -> Instrumentation {
        return Instrumentation();
      }));

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, Parse("(c)? a : b"));
  ASSERT_OK_AND_ASSIGN(auto ast,
                       cel::extensions::CreateAstFromParsedExpr(expr));
  ASSERT_OK_AND_ASSIGN(auto plan,
                       builder.CreateExpressionImpl(std::move(ast),
                                                    /*issues=*/nullptr));

  auto state = plan.MakeEvaluatorState(env_->descriptor_pool.get(),
                                       env_->MutableMessageFactory(), &arena_);
  cel::Activation activation;

  activation.InsertOrAssignValue("c", cel::BoolValue(true));
  activation.InsertOrAssignValue("a", cel::IntValue(1));
  activation.InsertOrAssignValue("b", cel::IntValue(2));

  ASSERT_OK_AND_ASSIGN(
      auto value,
      plan.EvaluateWithCallback(activation, EvaluationListener(), state));

  // AST
  //       ?:() <2>
  //      /   |    \
  // c <1>  a <3>   b <4>
  EXPECT_THAT(value, IsIntValue(1));
}

}  // namespace
}  // namespace google::api::expr::runtime
