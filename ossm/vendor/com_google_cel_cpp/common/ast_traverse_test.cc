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

#include "common/ast_traverse.h"

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "common/ast_visitor.h"
#include "common/constant.h"
#include "common/expr.h"
#include "internal/testing.h"

namespace cel::ast_internal {

namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::testing::_;
using ::testing::Ref;

class MockAstVisitor : public AstVisitor {
 public:
  // Expr handler.
  MOCK_METHOD(void, PreVisitExpr, (const Expr& expr), (override));

  // Expr handler.
  MOCK_METHOD(void, PostVisitExpr, (const Expr& expr), (override));

  MOCK_METHOD(void, PostVisitConst,
              (const Expr& expr, const Constant& const_expr), (override));

  // Ident node handler.
  MOCK_METHOD(void, PostVisitIdent,
              (const Expr& expr, const IdentExpr& ident_expr), (override));

  // Select node handler group
  MOCK_METHOD(void, PreVisitSelect,
              (const Expr& expr, const SelectExpr& select_expr), (override));

  MOCK_METHOD(void, PostVisitSelect,
              (const Expr& expr, const SelectExpr& select_expr), (override));

  // Call node handler group
  MOCK_METHOD(void, PreVisitCall, (const Expr& expr, const CallExpr& call_expr),
              (override));
  MOCK_METHOD(void, PostVisitCall,
              (const Expr& expr, const CallExpr& call_expr), (override));

  // Comprehension node handler group
  MOCK_METHOD(void, PreVisitComprehension,
              (const Expr& expr, const ComprehensionExpr& comprehension_expr),
              (override));
  MOCK_METHOD(void, PostVisitComprehension,
              (const Expr& expr, const ComprehensionExpr& comprehension_expr),
              (override));

  // Comprehension node handler group
  MOCK_METHOD(void, PreVisitComprehensionSubexpression,
              (const Expr& expr, const ComprehensionExpr& comprehension_expr,
               ComprehensionArg comprehension_arg),
              (override));
  MOCK_METHOD(void, PostVisitComprehensionSubexpression,
              (const Expr& expr, const ComprehensionExpr& comprehension_expr,
               ComprehensionArg comprehension_arg),
              (override));

  // We provide finer granularity for Call and Comprehension node callbacks
  // to allow special handling for short-circuiting.
  MOCK_METHOD(void, PostVisitTarget, (const Expr& expr), (override));
  MOCK_METHOD(void, PostVisitArg, (const Expr& expr, int arg_num), (override));

  // List node handler group
  MOCK_METHOD(void, PostVisitList,
              (const Expr& expr, const ListExpr& list_expr), (override));

  // Struct node handler group
  MOCK_METHOD(void, PostVisitStruct,
              (const Expr& expr, const StructExpr& struct_expr), (override));

  // Map node handler group
  MOCK_METHOD(void, PostVisitMap, (const Expr& expr, const MapExpr& map_expr),
              (override));
};

TEST(AstCrawlerTest, CheckCrawlConstant) {
  MockAstVisitor handler;

  Expr expr;
  auto& const_expr = expr.mutable_const_expr();

  EXPECT_CALL(handler, PostVisitConst(Ref(expr), Ref(const_expr))).Times(1);

  AstTraverse(expr, handler);
}

TEST(AstCrawlerTest, CheckCrawlIdent) {
  MockAstVisitor handler;

  Expr expr;
  auto& ident_expr = expr.mutable_ident_expr();

  EXPECT_CALL(handler, PostVisitIdent(Ref(expr), Ref(ident_expr))).Times(1);

  AstTraverse(expr, handler);
}

// Test handling of Select node when operand is not set.
TEST(AstCrawlerTest, CheckCrawlSelectNotCrashingPostVisitAbsentOperand) {
  MockAstVisitor handler;

  Expr expr;
  auto& select_expr = expr.mutable_select_expr();

  // Lowest level entry will be called first
  EXPECT_CALL(handler, PostVisitSelect(Ref(expr), Ref(select_expr))).Times(1);

  AstTraverse(expr, handler);
}

// Test handling of Select node
TEST(AstCrawlerTest, CheckCrawlSelect) {
  MockAstVisitor handler;

  Expr expr;
  auto& select_expr = expr.mutable_select_expr();
  auto& operand = select_expr.mutable_operand();
  auto& ident_expr = operand.mutable_ident_expr();

  testing::InSequence seq;

  // Lowest level entry will be called first
  EXPECT_CALL(handler, PostVisitIdent(Ref(operand), Ref(ident_expr))).Times(1);
  EXPECT_CALL(handler, PostVisitSelect(Ref(expr), Ref(select_expr))).Times(1);

  AstTraverse(expr, handler);
}

// Test handling of Call node without receiver
TEST(AstCrawlerTest, CheckCrawlCallNoReceiver) {
  MockAstVisitor handler;

  // <call>(<const>, <ident>)
  Expr expr;
  auto& call_expr = expr.mutable_call_expr();
  call_expr.mutable_args().reserve(2);
  auto& arg0 = call_expr.mutable_args().emplace_back();
  auto& const_expr = arg0.mutable_const_expr();
  auto& arg1 = call_expr.mutable_args().emplace_back();
  auto& ident_expr = arg1.mutable_ident_expr();

  testing::InSequence seq;

  // Lowest level entry will be called first
  EXPECT_CALL(handler, PreVisitCall(Ref(expr), Ref(call_expr))).Times(1);
  EXPECT_CALL(handler, PostVisitTarget(_)).Times(0);

  // Arg0
  EXPECT_CALL(handler, PostVisitConst(Ref(arg0), Ref(const_expr))).Times(1);
  EXPECT_CALL(handler, PostVisitExpr(Ref(arg0))).Times(1);
  EXPECT_CALL(handler, PostVisitArg(Ref(expr), 0)).Times(1);

  // Arg1
  EXPECT_CALL(handler, PostVisitIdent(Ref(arg1), Ref(ident_expr))).Times(1);
  EXPECT_CALL(handler, PostVisitExpr(Ref(arg1))).Times(1);
  EXPECT_CALL(handler, PostVisitArg(Ref(expr), 1)).Times(1);

  // Back to call
  EXPECT_CALL(handler, PostVisitCall(Ref(expr), Ref(call_expr))).Times(1);
  EXPECT_CALL(handler, PostVisitExpr(Ref(expr))).Times(1);

  AstTraverse(expr, handler);
}

// Test handling of Call node with receiver
TEST(AstCrawlerTest, CheckCrawlCallReceiver) {
  MockAstVisitor handler;

  // <ident>.<call>(<const>, <ident>)
  Expr expr;
  auto& call_expr = expr.mutable_call_expr();
  auto& target = call_expr.mutable_target();
  auto& target_ident = target.mutable_ident_expr();
  call_expr.mutable_args().reserve(2);
  auto& arg0 = call_expr.mutable_args().emplace_back();
  auto& const_expr = arg0.mutable_const_expr();
  auto& arg1 = call_expr.mutable_args().emplace_back();
  auto& ident_expr = arg1.mutable_ident_expr();

  testing::InSequence seq;

  // Lowest level entry will be called first
  EXPECT_CALL(handler, PreVisitCall(Ref(expr), Ref(call_expr))).Times(1);

  // Target
  EXPECT_CALL(handler, PostVisitIdent(Ref(target), Ref(target_ident))).Times(1);
  EXPECT_CALL(handler, PostVisitExpr(Ref(target))).Times(1);
  EXPECT_CALL(handler, PostVisitTarget(Ref(expr))).Times(1);

  // Arg0
  EXPECT_CALL(handler, PostVisitConst(Ref(arg0), Ref(const_expr))).Times(1);
  EXPECT_CALL(handler, PostVisitExpr(Ref(arg0))).Times(1);
  EXPECT_CALL(handler, PostVisitArg(Ref(expr), 0)).Times(1);

  // Arg1
  EXPECT_CALL(handler, PostVisitIdent(Ref(arg1), Ref(ident_expr))).Times(1);
  EXPECT_CALL(handler, PostVisitExpr(Ref(arg1))).Times(1);
  EXPECT_CALL(handler, PostVisitArg(Ref(expr), 1)).Times(1);

  // Back to call
  EXPECT_CALL(handler, PostVisitCall(Ref(expr), Ref(call_expr))).Times(1);
  EXPECT_CALL(handler, PostVisitExpr(Ref(expr))).Times(1);

  AstTraverse(expr, handler);
}

// Test handling of Comprehension node
TEST(AstCrawlerTest, CheckCrawlComprehension) {
  MockAstVisitor handler;

  Expr expr;
  auto& c = expr.mutable_comprehension_expr();
  auto& iter_range = c.mutable_iter_range();
  auto& iter_range_expr = iter_range.mutable_const_expr();
  auto& accu_init = c.mutable_accu_init();
  auto& accu_init_expr = accu_init.mutable_ident_expr();
  auto& loop_condition = c.mutable_loop_condition();
  auto& loop_condition_expr = loop_condition.mutable_const_expr();
  auto& loop_step = c.mutable_loop_step();
  auto& loop_step_expr = loop_step.mutable_ident_expr();
  auto& result = c.mutable_result();
  auto& result_expr = result.mutable_const_expr();

  testing::InSequence seq;

  // Lowest level entry will be called first
  EXPECT_CALL(handler, PreVisitComprehension(Ref(expr), Ref(c))).Times(1);

  EXPECT_CALL(handler,
              PreVisitComprehensionSubexpression(Ref(expr), Ref(c), ITER_RANGE))
      .Times(1);
  EXPECT_CALL(handler, PostVisitConst(Ref(iter_range), Ref(iter_range_expr)))
      .Times(1);
  EXPECT_CALL(handler, PostVisitComprehensionSubexpression(Ref(expr), Ref(c),
                                                           ITER_RANGE))
      .Times(1);

  // ACCU_INIT
  EXPECT_CALL(handler,
              PreVisitComprehensionSubexpression(Ref(expr), Ref(c), ACCU_INIT))
      .Times(1);
  EXPECT_CALL(handler, PostVisitIdent(Ref(accu_init), Ref(accu_init_expr)))
      .Times(1);
  EXPECT_CALL(handler,
              PostVisitComprehensionSubexpression(Ref(expr), Ref(c), ACCU_INIT))
      .Times(1);

  // LOOP CONDITION
  EXPECT_CALL(handler, PreVisitComprehensionSubexpression(Ref(expr), Ref(c),
                                                          LOOP_CONDITION))
      .Times(1);
  EXPECT_CALL(handler,
              PostVisitConst(Ref(loop_condition), Ref(loop_condition_expr)))
      .Times(1);
  EXPECT_CALL(handler, PostVisitComprehensionSubexpression(Ref(expr), Ref(c),
                                                           LOOP_CONDITION))
      .Times(1);

  // LOOP STEP
  EXPECT_CALL(handler,
              PreVisitComprehensionSubexpression(Ref(expr), Ref(c), LOOP_STEP))
      .Times(1);
  EXPECT_CALL(handler, PostVisitIdent(Ref(loop_step), Ref(loop_step_expr)))
      .Times(1);
  EXPECT_CALL(handler,
              PostVisitComprehensionSubexpression(Ref(expr), Ref(c), LOOP_STEP))
      .Times(1);

  // RESULT
  EXPECT_CALL(handler,
              PreVisitComprehensionSubexpression(Ref(expr), Ref(c), RESULT))
      .Times(1);

  EXPECT_CALL(handler, PostVisitConst(Ref(result), Ref(result_expr))).Times(1);

  EXPECT_CALL(handler,
              PostVisitComprehensionSubexpression(Ref(expr), Ref(c), RESULT))
      .Times(1);

  EXPECT_CALL(handler, PostVisitComprehension(Ref(expr), Ref(c))).Times(1);

  TraversalOptions opts;
  opts.use_comprehension_callbacks = true;
  AstTraverse(expr, handler, opts);
}

// Test handling of Comprehension node
TEST(AstCrawlerTest, CheckCrawlComprehensionLegacyCallbacks) {
  MockAstVisitor handler;

  Expr expr;
  auto& c = expr.mutable_comprehension_expr();
  auto& iter_range = c.mutable_iter_range();
  auto& iter_range_expr = iter_range.mutable_const_expr();
  auto& accu_init = c.mutable_accu_init();
  auto& accu_init_expr = accu_init.mutable_ident_expr();
  auto& loop_condition = c.mutable_loop_condition();
  auto& loop_condition_expr = loop_condition.mutable_const_expr();
  auto& loop_step = c.mutable_loop_step();
  auto& loop_step_expr = loop_step.mutable_ident_expr();
  auto& result = c.mutable_result();
  auto& result_expr = result.mutable_const_expr();

  testing::InSequence seq;

  // Lowest level entry will be called first
  EXPECT_CALL(handler, PreVisitComprehension(Ref(expr), Ref(c))).Times(1);

  EXPECT_CALL(handler, PostVisitConst(Ref(iter_range), Ref(iter_range_expr)))
      .Times(1);
  EXPECT_CALL(handler, PostVisitArg(Ref(expr), ITER_RANGE)).Times(1);

  // ACCU_INIT
  EXPECT_CALL(handler, PostVisitIdent(Ref(accu_init), Ref(accu_init_expr)))
      .Times(1);
  EXPECT_CALL(handler, PostVisitArg(Ref(expr), ACCU_INIT)).Times(1);

  // LOOP CONDITION
  EXPECT_CALL(handler,
              PostVisitConst(Ref(loop_condition), Ref(loop_condition_expr)))
      .Times(1);
  EXPECT_CALL(handler, PostVisitArg(Ref(expr), LOOP_CONDITION)).Times(1);

  // LOOP STEP
  EXPECT_CALL(handler, PostVisitIdent(Ref(loop_step), Ref(loop_step_expr)))
      .Times(1);
  EXPECT_CALL(handler, PostVisitArg(Ref(expr), LOOP_STEP)).Times(1);

  // RESULT
  EXPECT_CALL(handler, PostVisitConst(Ref(result), Ref(result_expr))).Times(1);
  EXPECT_CALL(handler, PostVisitArg(Ref(expr), RESULT)).Times(1);

  EXPECT_CALL(handler, PostVisitComprehension(Ref(expr), Ref(c))).Times(1);

  AstTraverse(expr, handler);
}

// Test handling of List node.
TEST(AstCrawlerTest, CheckList) {
  MockAstVisitor handler;

  Expr expr;
  auto& list_expr = expr.mutable_list_expr();
  list_expr.mutable_elements().reserve(2);
  auto& arg0 = list_expr.mutable_elements().emplace_back().mutable_expr();
  auto& const_expr = arg0.mutable_const_expr();
  auto& arg1 = list_expr.mutable_elements().emplace_back().mutable_expr();
  auto& ident_expr = arg1.mutable_ident_expr();

  testing::InSequence seq;

  EXPECT_CALL(handler, PostVisitConst(Ref(arg0), Ref(const_expr))).Times(1);
  EXPECT_CALL(handler, PostVisitIdent(Ref(arg1), Ref(ident_expr))).Times(1);
  EXPECT_CALL(handler, PostVisitList(Ref(expr), Ref(list_expr))).Times(1);

  AstTraverse(expr, handler);
}

// Test handling of Struct node.
TEST(AstCrawlerTest, CheckStruct) {
  MockAstVisitor handler;

  Expr expr;
  auto& struct_expr = expr.mutable_struct_expr();
  auto& entry0 = struct_expr.mutable_fields().emplace_back();

  auto& value = entry0.mutable_value().mutable_ident_expr();

  testing::InSequence seq;

  EXPECT_CALL(handler, PostVisitIdent(Ref(entry0.value()), Ref(value)))
      .Times(1);
  EXPECT_CALL(handler, PostVisitStruct(Ref(expr), Ref(struct_expr))).Times(1);

  AstTraverse(expr, handler);
}

// Test handling of Map node.
TEST(AstCrawlerTest, CheckMap) {
  MockAstVisitor handler;

  Expr expr;
  auto& map_expr = expr.mutable_map_expr();
  auto& entry0 = map_expr.mutable_entries().emplace_back();

  auto& key = entry0.mutable_key().mutable_const_expr();
  auto& value = entry0.mutable_value().mutable_ident_expr();

  testing::InSequence seq;

  EXPECT_CALL(handler, PostVisitConst(Ref(entry0.key()), Ref(key))).Times(1);
  EXPECT_CALL(handler, PostVisitIdent(Ref(entry0.value()), Ref(value)))
      .Times(1);
  EXPECT_CALL(handler, PostVisitMap(Ref(expr), Ref(map_expr))).Times(1);

  AstTraverse(expr, handler);
}

// Test generic Expr handlers.
TEST(AstCrawlerTest, CheckExprHandlers) {
  MockAstVisitor handler;

  Expr expr;
  auto& map_expr = expr.mutable_map_expr();
  auto& entry0 = map_expr.mutable_entries().emplace_back();

  entry0.mutable_key().mutable_const_expr();
  entry0.mutable_value().mutable_ident_expr();

  EXPECT_CALL(handler, PreVisitExpr(_)).Times(3);
  EXPECT_CALL(handler, PostVisitExpr(_)).Times(3);

  AstTraverse(expr, handler);
}

TEST(AstTraverseManager, Interrupt) {
  MockAstVisitor handler;

  Expr expr;
  auto& select_expr = expr.mutable_select_expr();
  auto& operand = select_expr.mutable_operand();
  auto& ident_expr = operand.mutable_ident_expr();

  testing::InSequence seq;

  AstTraverseManager manager;

  EXPECT_CALL(handler, PostVisitIdent(Ref(operand), Ref(ident_expr)))
      .Times(1)
      .WillOnce([&manager](const Expr& expr, const IdentExpr& ident_expr) {
        manager.RequestHalt();
      });
  EXPECT_CALL(handler, PostVisitSelect(Ref(expr), Ref(select_expr))).Times(0);

  EXPECT_THAT(manager.AstTraverse(expr, handler), IsOk());
}

TEST(AstTraverseManager, NoInterrupt) {
  MockAstVisitor handler;

  Expr expr;
  auto& select_expr = expr.mutable_select_expr();
  auto& operand = select_expr.mutable_operand();
  auto& ident_expr = operand.mutable_ident_expr();

  testing::InSequence seq;

  AstTraverseManager manager;

  EXPECT_CALL(handler, PostVisitIdent(Ref(operand), Ref(ident_expr))).Times(1);
  EXPECT_CALL(handler, PostVisitSelect(Ref(expr), Ref(select_expr))).Times(1);

  EXPECT_THAT(manager.AstTraverse(expr, handler), IsOk());
}

TEST(AstCrawlerTest, ReentantTraversalUnsupported) {
  MockAstVisitor handler;

  Expr expr;
  auto& select_expr = expr.mutable_select_expr();
  auto& operand = select_expr.mutable_operand();
  auto& ident_expr = operand.mutable_ident_expr();

  AstTraverseManager manager;

  testing::InSequence seq;

  EXPECT_CALL(handler, PostVisitIdent(Ref(operand), Ref(ident_expr)))
      .Times(1)
      .WillOnce(
          [&manager, &handler](const Expr& expr, const IdentExpr& ident_expr) {
            EXPECT_THAT(manager.AstTraverse(expr, handler),
                        StatusIs(absl::StatusCode::kFailedPrecondition));
          });

  EXPECT_CALL(handler, PostVisitSelect(Ref(expr), Ref(select_expr))).Times(1);

  EXPECT_THAT(manager.AstTraverse(expr, handler), IsOk());
}

}  // namespace

}  // namespace cel::ast_internal
