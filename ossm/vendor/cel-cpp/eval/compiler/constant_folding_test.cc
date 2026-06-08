// Copyright 2019 Google LLC
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

#include "eval/compiler/constant_folding.h"

#include <memory>
#include <utility>
#include <vector>

#include "cel/expr/syntax.pb.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/ast.h"
#include "common/expr.h"
#include "common/value.h"
#include "eval/compiler/flat_expr_builder_extensions.h"
#include "eval/compiler/resolver.h"
#include "eval/eval/const_value_step.h"
#include "eval/eval/create_list_step.h"
#include "eval/eval/create_map_step.h"
#include "eval/eval/evaluator_core.h"
#include "extensions/protobuf/ast_converters.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "parser/parser.h"
#include "runtime/function_registry.h"
#include "runtime/internal/issue_collector.h"
#include "runtime/internal/runtime_env.h"
#include "runtime/internal/runtime_env_testing.h"
#include "runtime/runtime_issue.h"
#include "runtime/runtime_options.h"
#include "runtime/type_registry.h"
#include "google/protobuf/arena.h"

namespace cel::runtime_internal {

namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::cel::Expr;
using ::cel::RuntimeIssue;
using ::cel::runtime_internal::IssueCollector;
using ::cel::runtime_internal::NewTestingRuntimeEnv;
using ::cel::expr::ParsedExpr;
using ::google::api::expr::parser::Parse;
using ::google::api::expr::runtime::CreateConstValueStep;
using ::google::api::expr::runtime::CreateCreateListStep;
using ::google::api::expr::runtime::CreateCreateStructStepForMap;
using ::google::api::expr::runtime::ExecutionPath;
using ::google::api::expr::runtime::PlannerContext;
using ::google::api::expr::runtime::ProgramBuilder;
using ::google::api::expr::runtime::ProgramOptimizer;
using ::google::api::expr::runtime::ProgramOptimizerFactory;
using ::google::api::expr::runtime::Resolver;
using ::testing::SizeIs;

class UpdatedConstantFoldingTest : public testing::Test {
 public:
  UpdatedConstantFoldingTest()
      : env_(NewTestingRuntimeEnv()),
        function_registry_(env_->function_registry),
        type_registry_(env_->type_registry),
        issue_collector_(RuntimeIssue::Severity::kError),
        resolver_("", function_registry_, type_registry_,
                  type_registry_.GetComposedTypeProvider()) {}

 protected:
  absl_nonnull std::shared_ptr<RuntimeEnv> env_;
  google::protobuf::Arena arena_;
  cel::FunctionRegistry& function_registry_;
  cel::TypeRegistry& type_registry_;
  cel::RuntimeOptions options_;
  IssueCollector issue_collector_;
  Resolver resolver_;
};

absl::StatusOr<std::unique_ptr<cel::Ast>> ParseFromCel(
    absl::string_view expression) {
  CEL_ASSIGN_OR_RETURN(ParsedExpr expr, Parse(expression));
  return cel::extensions::CreateAstFromParsedExpr(expr);
}

// While CEL doesn't provide execution order guarantees per se, short circuiting
// operators are treated specially to evaluate to user expectations.
//
// These behaviors aren't easily observable since the flat expression doesn't
// expose any details about the program after building, so a lot of setup is
// needed to simulate what the expression builder does.
TEST_F(UpdatedConstantFoldingTest, SkipsTernary) {
  // Arrange
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Ast> ast,
                       ParseFromCel("true ? true : false"));

  const Expr& call = ast->root_expr();
  const Expr& condition = call.call_expr().args()[0];
  const Expr& true_branch = call.call_expr().args()[1];
  const Expr& false_branch = call.call_expr().args()[2];

  ProgramBuilder program_builder;
  program_builder.EnterSubexpression(&call);
  // condition
  program_builder.EnterSubexpression(&condition);
  ASSERT_OK_AND_ASSIGN(auto step,
                       CreateConstValueStep(cel::BoolValue(true), -1));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&condition);

  // true
  program_builder.EnterSubexpression(&true_branch);
  ASSERT_OK_AND_ASSIGN(step, CreateConstValueStep(cel::BoolValue(true), -1));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&true_branch);

  // false
  program_builder.EnterSubexpression(&false_branch);
  ASSERT_OK_AND_ASSIGN(step, CreateConstValueStep(cel::BoolValue(true), -1));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&false_branch);

  // ternary.
  ASSERT_OK_AND_ASSIGN(step, CreateConstValueStep(cel::NullValue(), -1));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&call);

  std::shared_ptr<google::protobuf::Arena> arena;
  PlannerContext context(env_, resolver_, options_,
                         type_registry_.GetComposedTypeProvider(),
                         issue_collector_, program_builder, arena);

  ProgramOptimizerFactory constant_folder_factory =
      CreateConstantFoldingOptimizer();

  // Act
  // Issue the visitation calls.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ProgramOptimizer> constant_folder,
                       constant_folder_factory(context, *ast));
  ASSERT_OK(constant_folder->OnPreVisit(context, call));
  ASSERT_OK(constant_folder->OnPreVisit(context, condition));
  ASSERT_OK(constant_folder->OnPostVisit(context, condition));
  ASSERT_OK(constant_folder->OnPreVisit(context, true_branch));
  ASSERT_OK(constant_folder->OnPostVisit(context, true_branch));
  ASSERT_OK(constant_folder->OnPreVisit(context, false_branch));
  ASSERT_OK(constant_folder->OnPostVisit(context, false_branch));
  ASSERT_OK(constant_folder->OnPostVisit(context, call));

  // Assert
  // No changes attempted.
  auto path = std::move(program_builder).FlattenMain();
  EXPECT_THAT(path, SizeIs(4));
}

TEST_F(UpdatedConstantFoldingTest, SkipsOr) {
  // Arrange
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Ast> ast,
                       ParseFromCel("false || true"));

  const Expr& call = ast->root_expr();
  const Expr& left_condition = call.call_expr().args()[0];
  const Expr& right_condition = call.call_expr().args()[1];

  ProgramBuilder program_builder;

  program_builder.EnterSubexpression(&call);

  // left
  program_builder.EnterSubexpression(&left_condition);
  ASSERT_OK_AND_ASSIGN(auto step,
                       CreateConstValueStep(cel::BoolValue(false), -1));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&left_condition);

  // right
  program_builder.EnterSubexpression(&right_condition);
  ASSERT_OK_AND_ASSIGN(step, CreateConstValueStep(cel::BoolValue(true), -1));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&right_condition);

  // op
  // Just a placeholder.
  ASSERT_OK_AND_ASSIGN(step, CreateConstValueStep(cel::NullValue(), -1));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&call);

  std::shared_ptr<google::protobuf::Arena> arena;
  PlannerContext context(env_, resolver_, options_,
                         type_registry_.GetComposedTypeProvider(),
                         issue_collector_, program_builder, arena);

  ProgramOptimizerFactory constant_folder_factory =
      CreateConstantFoldingOptimizer();

  // Act
  // Issue the visitation calls.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ProgramOptimizer> constant_folder,
                       constant_folder_factory(context, *ast));
  ASSERT_OK(constant_folder->OnPreVisit(context, call));
  ASSERT_OK(constant_folder->OnPreVisit(context, left_condition));
  ASSERT_OK(constant_folder->OnPostVisit(context, left_condition));
  ASSERT_OK(constant_folder->OnPreVisit(context, right_condition));
  ASSERT_OK(constant_folder->OnPostVisit(context, right_condition));
  ASSERT_OK(constant_folder->OnPostVisit(context, call));

  // Assert
  // No changes attempted.
  auto path = std::move(program_builder).FlattenMain();
  EXPECT_THAT(path, SizeIs(3));
}

TEST_F(UpdatedConstantFoldingTest, SkipsAnd) {
  // Arrange
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Ast> ast,
                       ParseFromCel("true && false"));

  const Expr& call = ast->root_expr();
  const Expr& left_condition = call.call_expr().args()[0];
  const Expr& right_condition = call.call_expr().args()[1];

  ProgramBuilder program_builder;
  program_builder.EnterSubexpression(&call);

  // left
  program_builder.EnterSubexpression(&left_condition);
  ASSERT_OK_AND_ASSIGN(auto step,
                       CreateConstValueStep(cel::BoolValue(true), -1));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&left_condition);

  // right
  program_builder.EnterSubexpression(&right_condition);
  ASSERT_OK_AND_ASSIGN(step, CreateConstValueStep(cel::BoolValue(false), -1));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&right_condition);

  // op
  // Just a placeholder.
  ASSERT_OK_AND_ASSIGN(step, CreateConstValueStep(cel::NullValue(), -1));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&call);

  std::shared_ptr<google::protobuf::Arena> arena;
  PlannerContext context(env_, resolver_, options_,
                         type_registry_.GetComposedTypeProvider(),
                         issue_collector_, program_builder, arena);

  ProgramOptimizerFactory constant_folder_factory =
      CreateConstantFoldingOptimizer();

  // Act
  // Issue the visitation calls.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ProgramOptimizer> constant_folder,
                       constant_folder_factory(context, *ast));
  ASSERT_OK(constant_folder->OnPreVisit(context, call));
  ASSERT_OK(constant_folder->OnPreVisit(context, left_condition));
  ASSERT_OK(constant_folder->OnPostVisit(context, left_condition));
  ASSERT_OK(constant_folder->OnPreVisit(context, right_condition));
  ASSERT_OK(constant_folder->OnPostVisit(context, right_condition));
  ASSERT_OK(constant_folder->OnPostVisit(context, call));

  // Assert
  // No changes attempted.
  ExecutionPath path = std::move(program_builder).FlattenMain();
  EXPECT_THAT(path, SizeIs(3));
}

TEST_F(UpdatedConstantFoldingTest, CreatesList) {
  // Arrange
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Ast> ast, ParseFromCel("[1, 2]"));

  const Expr& create_list = ast->root_expr();
  const Expr& elem_one = create_list.list_expr().elements()[0].expr();
  const Expr& elem_two = create_list.list_expr().elements()[1].expr();

  ProgramBuilder program_builder;
  // Simulate the visitor order.
  program_builder.EnterSubexpression(&create_list);

  // elem one
  program_builder.EnterSubexpression(&elem_one);
  ASSERT_OK_AND_ASSIGN(auto step, CreateConstValueStep(cel::IntValue(1L), 1));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&elem_one);

  // elem two
  program_builder.EnterSubexpression(&elem_two);
  ASSERT_OK_AND_ASSIGN(step, CreateConstValueStep(cel::IntValue(2L), 2));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&elem_two);

  // createlist
  ASSERT_OK_AND_ASSIGN(step, CreateCreateListStep(create_list.list_expr(), 3));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&create_list);

  std::shared_ptr<google::protobuf::Arena> arena;
  PlannerContext context(env_, resolver_, options_,
                         type_registry_.GetComposedTypeProvider(),
                         issue_collector_, program_builder, arena);

  ProgramOptimizerFactory constant_folder_factory =
      CreateConstantFoldingOptimizer();

  // Act
  // Issue the visitation calls.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ProgramOptimizer> constant_folder,
                       constant_folder_factory(context, *ast));
  ASSERT_OK(constant_folder->OnPreVisit(context, create_list));
  ASSERT_OK(constant_folder->OnPreVisit(context, elem_one));
  ASSERT_OK(constant_folder->OnPostVisit(context, elem_one));
  ASSERT_OK(constant_folder->OnPreVisit(context, elem_two));
  ASSERT_OK(constant_folder->OnPostVisit(context, elem_two));
  ASSERT_OK(constant_folder->OnPostVisit(context, create_list));

  // Assert
  // Single constant value for the two element list.
  ExecutionPath path = std::move(program_builder).FlattenMain();
  EXPECT_THAT(path, SizeIs(1));
}

TEST_F(UpdatedConstantFoldingTest, CreatesLargeList) {
  // Arrange
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Ast> ast,
                       ParseFromCel("[1, 2, 3, 4, 5]"));

  const Expr& create_list = ast->root_expr();
  const Expr& elem0 = create_list.list_expr().elements()[0].expr();
  const Expr& elem1 = create_list.list_expr().elements()[1].expr();
  const Expr& elem2 = create_list.list_expr().elements()[2].expr();
  const Expr& elem3 = create_list.list_expr().elements()[3].expr();
  const Expr& elem4 = create_list.list_expr().elements()[4].expr();

  ProgramBuilder program_builder;
  // Simulate the visitor order.
  ASSERT_TRUE(program_builder.EnterSubexpression(&create_list) != nullptr);

  // 0
  ASSERT_TRUE(program_builder.EnterSubexpression(&elem0) != nullptr);
  ASSERT_OK_AND_ASSIGN(auto step, CreateConstValueStep(cel::IntValue(1L), 1));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&elem0);

  // 1
  ASSERT_TRUE(program_builder.EnterSubexpression(&elem1));
  ASSERT_OK_AND_ASSIGN(step, CreateConstValueStep(cel::IntValue(2L), 2));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&elem1);

  // 2
  ASSERT_TRUE(program_builder.EnterSubexpression(&elem2) != nullptr);
  ASSERT_OK_AND_ASSIGN(step, CreateConstValueStep(cel::IntValue(3L), 3));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&elem2);

  // 3
  ASSERT_TRUE(program_builder.EnterSubexpression(&elem3) != nullptr);
  ASSERT_OK_AND_ASSIGN(step, CreateConstValueStep(cel::IntValue(4L), 4));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&elem3);

  // 4
  ASSERT_TRUE(program_builder.EnterSubexpression(&elem4) != nullptr);
  ASSERT_OK_AND_ASSIGN(step, CreateConstValueStep(cel::IntValue(5L), 5));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&elem4);

  // createlist
  ASSERT_OK_AND_ASSIGN(step, CreateCreateListStep(create_list.list_expr(), 6));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&create_list);

  std::shared_ptr<google::protobuf::Arena> arena;
  PlannerContext context(env_, resolver_, options_,
                         type_registry_.GetComposedTypeProvider(),
                         issue_collector_, program_builder, arena);

  ProgramOptimizerFactory constant_folder_factory =
      CreateConstantFoldingOptimizer();

  // Act
  // Issue the visitation calls.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ProgramOptimizer> constant_folder,
                       constant_folder_factory(context, *ast));
  ASSERT_THAT(constant_folder->OnPreVisit(context, create_list), IsOk());
  ASSERT_THAT(constant_folder->OnPreVisit(context, elem0), IsOk());
  ASSERT_THAT(constant_folder->OnPostVisit(context, elem0), IsOk());
  ASSERT_THAT(constant_folder->OnPreVisit(context, elem1), IsOk());
  ASSERT_THAT(constant_folder->OnPostVisit(context, elem1), IsOk());
  ASSERT_THAT(constant_folder->OnPreVisit(context, elem2), IsOk());
  ASSERT_THAT(constant_folder->OnPostVisit(context, elem2), IsOk());
  ASSERT_THAT(constant_folder->OnPreVisit(context, elem3), IsOk());
  ASSERT_THAT(constant_folder->OnPostVisit(context, elem3), IsOk());
  ASSERT_THAT(constant_folder->OnPreVisit(context, elem4), IsOk());
  ASSERT_THAT(constant_folder->OnPostVisit(context, elem4), IsOk());
  ASSERT_THAT(constant_folder->OnPostVisit(context, create_list), IsOk());

  // Assert
  // Single constant value for the two element list.
  ExecutionPath path = std::move(program_builder).FlattenMain();
  EXPECT_THAT(path, SizeIs(1));
}

TEST_F(UpdatedConstantFoldingTest, CreatesMap) {
  // Arrange
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Ast> ast, ParseFromCel("{1: 2}"));

  const Expr& create_map = ast->root_expr();
  const Expr& key = create_map.map_expr().entries()[0].key();
  const Expr& value = create_map.map_expr().entries()[0].value();

  ProgramBuilder program_builder;
  program_builder.EnterSubexpression(&create_map);

  // key
  program_builder.EnterSubexpression(&key);
  ASSERT_OK_AND_ASSIGN(auto step, CreateConstValueStep(cel::IntValue(1L), 1));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&key);

  // value
  program_builder.EnterSubexpression(&value);
  ASSERT_OK_AND_ASSIGN(step, CreateConstValueStep(cel::IntValue(2L), 2));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&value);

  // create map
  ASSERT_OK_AND_ASSIGN(
      step, CreateCreateStructStepForMap(create_map.map_expr().entries().size(),
                                         {}, 3));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&create_map);

  std::shared_ptr<google::protobuf::Arena> arena;
  PlannerContext context(env_, resolver_, options_,
                         type_registry_.GetComposedTypeProvider(),
                         issue_collector_, program_builder, arena);

  ProgramOptimizerFactory constant_folder_factory =
      CreateConstantFoldingOptimizer();

  // Act
  // Issue the visitation calls.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ProgramOptimizer> constant_folder,
                       constant_folder_factory(context, *ast));
  ASSERT_OK(constant_folder->OnPreVisit(context, create_map));
  ASSERT_OK(constant_folder->OnPreVisit(context, key));
  ASSERT_OK(constant_folder->OnPostVisit(context, key));
  ASSERT_OK(constant_folder->OnPreVisit(context, value));
  ASSERT_OK(constant_folder->OnPostVisit(context, value));
  ASSERT_OK(constant_folder->OnPostVisit(context, create_map));

  // Assert
  // Single constant value for the map.
  ExecutionPath path = std::move(program_builder).FlattenMain();
  EXPECT_THAT(path, SizeIs(1));
}

TEST_F(UpdatedConstantFoldingTest, CreatesInvalidMap) {
  // Arrange
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Ast> ast, ParseFromCel("{1.0: 2}"));

  const Expr& create_map = ast->root_expr();
  const Expr& key = create_map.map_expr().entries()[0].key();
  const Expr& value = create_map.map_expr().entries()[0].value();

  ProgramBuilder program_builder;
  program_builder.EnterSubexpression(&create_map);

  // key
  program_builder.EnterSubexpression(&key);
  ASSERT_OK_AND_ASSIGN(auto step,
                       CreateConstValueStep(cel::DoubleValue(1.0), 1));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&key);

  // value
  program_builder.EnterSubexpression(&value);
  ASSERT_OK_AND_ASSIGN(step, CreateConstValueStep(cel::IntValue(2L), 2));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&value);

  // create map
  ASSERT_OK_AND_ASSIGN(
      step, CreateCreateStructStepForMap(create_map.map_expr().entries().size(),
                                         {}, 3));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&create_map);

  std::shared_ptr<google::protobuf::Arena> arena;
  PlannerContext context(env_, resolver_, options_,
                         type_registry_.GetComposedTypeProvider(),
                         issue_collector_, program_builder, arena);

  ProgramOptimizerFactory constant_folder_factory =
      CreateConstantFoldingOptimizer();

  // Act
  // Issue the visitation calls.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ProgramOptimizer> constant_folder,
                       constant_folder_factory(context, *ast));
  ASSERT_OK(constant_folder->OnPreVisit(context, create_map));
  ASSERT_OK(constant_folder->OnPreVisit(context, key));
  ASSERT_OK(constant_folder->OnPostVisit(context, key));
  ASSERT_OK(constant_folder->OnPreVisit(context, value));
  ASSERT_OK(constant_folder->OnPostVisit(context, value));
  ASSERT_OK(constant_folder->OnPostVisit(context, create_map));

  ExecutionPath path = std::move(program_builder).FlattenMain();
  EXPECT_THAT(path, SizeIs(1));
}

TEST_F(UpdatedConstantFoldingTest, ErrorsOnUnexpectedOrder) {
  // Arrange
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Ast> ast,
                       ParseFromCel("true && false"));

  const Expr& call = ast->root_expr();
  const Expr& left_condition = call.call_expr().args()[0];
  const Expr& right_condition = call.call_expr().args()[1];

  ProgramBuilder program_builder;

  program_builder.EnterSubexpression(&call);
  // left
  program_builder.EnterSubexpression(&left_condition);
  ASSERT_OK_AND_ASSIGN(auto step,
                       CreateConstValueStep(cel::BoolValue(true), -1));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&left_condition);

  // right
  program_builder.EnterSubexpression(&right_condition);
  ASSERT_OK_AND_ASSIGN(step, CreateConstValueStep(cel::BoolValue(false), -1));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&right_condition);

  // op
  // Just a placeholder.
  ASSERT_OK_AND_ASSIGN(step, CreateConstValueStep(cel::NullValue(), -1));
  program_builder.AddStep(std::move(step));
  program_builder.ExitSubexpression(&call);

  std::shared_ptr<google::protobuf::Arena> arena;
  PlannerContext context(env_, resolver_, options_,
                         type_registry_.GetComposedTypeProvider(),
                         issue_collector_, program_builder, arena);

  ProgramOptimizerFactory constant_folder_factory =
      CreateConstantFoldingOptimizer();

  // Act / Assert
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ProgramOptimizer> constant_folder,
                       constant_folder_factory(context, *ast));
  EXPECT_THAT(constant_folder->OnPostVisit(context, left_condition),
              StatusIs(absl::StatusCode::kInternal));
}

}  // namespace

}  // namespace cel::runtime_internal
