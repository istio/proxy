// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "eval/public/ast_traverse.h"

#include "eval/public/ast_visitor.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

namespace {

using google::api::expr::v1alpha1::Constant;
using google::api::expr::v1alpha1::Expr;
using google::api::expr::v1alpha1::SourceInfo;
using testing::_;
using Ident = google::api::expr::v1alpha1::Expr::Ident;
using Select = google::api::expr::v1alpha1::Expr::Select;
using Call = google::api::expr::v1alpha1::Expr::Call;
using CreateList = google::api::expr::v1alpha1::Expr::CreateList;
using CreateStruct = google::api::expr::v1alpha1::Expr::CreateStruct;
using Comprehension = google::api::expr::v1alpha1::Expr::Comprehension;

class MockAstVisitor : public AstVisitor {
 public:
  // Expr handler.
  MOCK_METHOD(void, PreVisitExpr,
              (const Expr* expr, const SourcePosition* position), (override));

  // Expr handler.
  MOCK_METHOD(void, PostVisitExpr,
              (const Expr* expr, const SourcePosition* position), (override));

  // Constant node handler.
  MOCK_METHOD(void, PreVisitConst,
              (const Constant* const_expr, const Expr* expr,
               const SourcePosition* position),
              (override));

  // Constant node handler.
  MOCK_METHOD(void, PostVisitConst,
              (const Constant* const_expr, const Expr* expr,
               const SourcePosition* position),
              (override));

  // Ident node handler.
  MOCK_METHOD(void, PreVisitIdent,
              (const Ident* ident_expr, const Expr* expr,
               const SourcePosition* position),
              (override));

  // Ident node handler.
  MOCK_METHOD(void, PostVisitIdent,
              (const Ident* ident_expr, const Expr* expr,
               const SourcePosition* position),
              (override));

  // Select node handler group
  MOCK_METHOD(void, PreVisitSelect,
              (const Select* select_expr, const Expr* expr,
               const SourcePosition* position),
              (override));

  MOCK_METHOD(void, PostVisitSelect,
              (const Select* select_expr, const Expr* expr,
               const SourcePosition* position),
              (override));

  // Call node handler group
  MOCK_METHOD(void, PreVisitCall,
              (const Call* call_expr, const Expr* expr,
               const SourcePosition* position),
              (override));
  MOCK_METHOD(void, PostVisitCall,
              (const Call* call_expr, const Expr* expr,
               const SourcePosition* position),
              (override));

  // Comprehension node handler group
  MOCK_METHOD(void, PreVisitComprehension,
              (const Comprehension* comprehension_expr, const Expr* expr,
               const SourcePosition* position),
              (override));
  MOCK_METHOD(void, PostVisitComprehension,
              (const Comprehension* comprehension_expr, const Expr* expr,
               const SourcePosition* position),
              (override));

  // Comprehension node handler group
  MOCK_METHOD(void, PreVisitComprehensionSubexpression,
              (const Expr* expr, const Comprehension* comprehension_expr,
               ComprehensionArg comprehension_arg,
               const SourcePosition* position),
              (override));
  MOCK_METHOD(void, PostVisitComprehensionSubexpression,
              (const Expr* expr, const Comprehension* comprehension_expr,
               ComprehensionArg comprehension_arg,
               const SourcePosition* position),
              (override));

  // We provide finer granularity for Call and Comprehension node callbacks
  // to allow special handling for short-circuiting.
  MOCK_METHOD(void, PostVisitTarget,
              (const Expr* expr, const SourcePosition* position), (override));
  MOCK_METHOD(void, PostVisitArg,
              (int arg_num, const Expr* expr, const SourcePosition* position),
              (override));

  // CreateList node handler group
  MOCK_METHOD(void, PreVisitCreateList,
              (const CreateList* list_expr, const Expr* expr,
               const SourcePosition* position),
              (override));

  // CreateList node handler group
  MOCK_METHOD(void, PostVisitCreateList,
              (const CreateList* list_expr, const Expr* expr,
               const SourcePosition* position),
              (override));

  // CreateStruct node handler group
  MOCK_METHOD(void, PreVisitCreateStruct,
              (const CreateStruct* struct_expr, const Expr* expr,
               const SourcePosition* position),
              (override));

  // CreateStruct node handler group
  MOCK_METHOD(void, PostVisitCreateStruct,
              (const CreateStruct* struct_expr, const Expr* expr,
               const SourcePosition* position),
              (override));
};

TEST(AstCrawlerTest, CheckCrawlConstant) {
  SourceInfo source_info;
  MockAstVisitor handler;

  Expr expr;
  auto const_expr = expr.mutable_const_expr();

  EXPECT_CALL(handler, PreVisitConst(const_expr, &expr, _)).Times(1);
  EXPECT_CALL(handler, PostVisitConst(const_expr, &expr, _)).Times(1);

  AstTraverse(&expr, &source_info, &handler);
}

TEST(AstCrawlerTest, CheckCrawlIdent) {
  SourceInfo source_info;
  MockAstVisitor handler;

  Expr expr;
  auto ident_expr = expr.mutable_ident_expr();

  EXPECT_CALL(handler, PreVisitIdent(ident_expr, &expr, _)).Times(1);
  EXPECT_CALL(handler, PostVisitIdent(ident_expr, &expr, _)).Times(1);

  AstTraverse(&expr, &source_info, &handler);
}

// Test handling of Select node when operand is not set.
TEST(AstCrawlerTest, CheckCrawlSelectNotCrashingPostVisitAbsentOperand) {
  SourceInfo source_info;
  MockAstVisitor handler;

  Expr expr;
  auto select_expr = expr.mutable_select_expr();

  // Lowest level entry will be called first
  EXPECT_CALL(handler, PostVisitSelect(select_expr, &expr, _)).Times(1);

  AstTraverse(&expr, &source_info, &handler);
}

// Test handling of Select node
TEST(AstCrawlerTest, CheckCrawlSelect) {
  SourceInfo source_info;
  MockAstVisitor handler;

  Expr expr;
  auto select_expr = expr.mutable_select_expr();
  auto operand = select_expr->mutable_operand();
  auto ident_expr = operand->mutable_ident_expr();

  testing::InSequence seq;

  // Lowest level entry will be called first
  EXPECT_CALL(handler, PostVisitIdent(ident_expr, operand, _)).Times(1);
  EXPECT_CALL(handler, PostVisitSelect(select_expr, &expr, _)).Times(1);

  AstTraverse(&expr, &source_info, &handler);
}

// Test handling of Call node without receiver
TEST(AstCrawlerTest, CheckCrawlCallNoReceiver) {
  SourceInfo source_info;
  MockAstVisitor handler;

  // <call>(<const>, <ident>)
  Expr expr;
  auto* call_expr = expr.mutable_call_expr();
  Expr* arg0 = call_expr->add_args();
  auto* const_expr = arg0->mutable_const_expr();
  Expr* arg1 = call_expr->add_args();
  auto* ident_expr = arg1->mutable_ident_expr();

  testing::InSequence seq;

  // Lowest level entry will be called first
  EXPECT_CALL(handler, PreVisitCall(call_expr, &expr, _)).Times(1);
  EXPECT_CALL(handler, PostVisitTarget(_, _)).Times(0);

  // Arg0
  EXPECT_CALL(handler, PostVisitConst(const_expr, arg0, _)).Times(1);
  EXPECT_CALL(handler, PostVisitExpr(arg0, _)).Times(1);
  EXPECT_CALL(handler, PostVisitArg(0, &expr, _)).Times(1);

  // Arg1
  EXPECT_CALL(handler, PostVisitIdent(ident_expr, arg1, _)).Times(1);
  EXPECT_CALL(handler, PostVisitExpr(arg1, _)).Times(1);
  EXPECT_CALL(handler, PostVisitArg(1, &expr, _)).Times(1);

  // Back to call
  EXPECT_CALL(handler, PostVisitCall(call_expr, &expr, _)).Times(1);
  EXPECT_CALL(handler, PostVisitExpr(&expr, _)).Times(1);

  AstTraverse(&expr, &source_info, &handler);
}

// Test handling of Call node with receiver
TEST(AstCrawlerTest, CheckCrawlCallReceiver) {
  SourceInfo source_info;
  MockAstVisitor handler;

  // <ident>.<call>(<const>, <ident>)
  Expr expr;
  auto* call_expr = expr.mutable_call_expr();
  Expr* target = call_expr->mutable_target();
  auto* target_ident = target->mutable_ident_expr();
  Expr* arg0 = call_expr->add_args();
  auto* const_expr = arg0->mutable_const_expr();
  Expr* arg1 = call_expr->add_args();
  auto* ident_expr = arg1->mutable_ident_expr();

  testing::InSequence seq;

  // Lowest level entry will be called first
  EXPECT_CALL(handler, PreVisitCall(call_expr, &expr, _)).Times(1);

  // Target
  EXPECT_CALL(handler, PostVisitIdent(target_ident, target, _)).Times(1);
  EXPECT_CALL(handler, PostVisitExpr(target, _)).Times(1);
  EXPECT_CALL(handler, PostVisitTarget(&expr, _)).Times(1);

  // Arg0
  EXPECT_CALL(handler, PostVisitConst(const_expr, arg0, _)).Times(1);
  EXPECT_CALL(handler, PostVisitExpr(arg0, _)).Times(1);
  EXPECT_CALL(handler, PostVisitArg(0, &expr, _)).Times(1);

  // Arg1
  EXPECT_CALL(handler, PostVisitIdent(ident_expr, arg1, _)).Times(1);
  EXPECT_CALL(handler, PostVisitExpr(arg1, _)).Times(1);
  EXPECT_CALL(handler, PostVisitArg(1, &expr, _)).Times(1);

  // Back to call
  EXPECT_CALL(handler, PostVisitCall(call_expr, &expr, _)).Times(1);
  EXPECT_CALL(handler, PostVisitExpr(&expr, _)).Times(1);

  AstTraverse(&expr, &source_info, &handler);
}

// Test handling of Comprehension node
TEST(AstCrawlerTest, CheckCrawlComprehension) {
  SourceInfo source_info;
  MockAstVisitor handler;

  Expr expr;
  auto c = expr.mutable_comprehension_expr();
  auto iter_range = c->mutable_iter_range();
  auto iter_range_expr = iter_range->mutable_const_expr();
  auto accu_init = c->mutable_accu_init();
  auto accu_init_expr = accu_init->mutable_ident_expr();
  auto loop_condition = c->mutable_loop_condition();
  auto loop_condition_expr = loop_condition->mutable_const_expr();
  auto loop_step = c->mutable_loop_step();
  auto loop_step_expr = loop_step->mutable_ident_expr();
  auto result = c->mutable_result();
  auto result_expr = result->mutable_const_expr();

  testing::InSequence seq;

  // Lowest level entry will be called first
  EXPECT_CALL(handler, PreVisitComprehension(c, &expr, _)).Times(1);

  EXPECT_CALL(handler,
              PreVisitComprehensionSubexpression(iter_range, c, ITER_RANGE, _))
      .Times(1);
  EXPECT_CALL(handler, PostVisitConst(iter_range_expr, iter_range, _)).Times(1);
  EXPECT_CALL(handler,
              PostVisitComprehensionSubexpression(iter_range, c, ITER_RANGE, _))
      .Times(1);

  // ACCU_INIT
  EXPECT_CALL(handler,
              PreVisitComprehensionSubexpression(accu_init, c, ACCU_INIT, _))
      .Times(1);
  EXPECT_CALL(handler, PostVisitIdent(accu_init_expr, accu_init, _)).Times(1);
  EXPECT_CALL(handler,
              PostVisitComprehensionSubexpression(accu_init, c, ACCU_INIT, _))
      .Times(1);

  // LOOP CONDITION
  EXPECT_CALL(handler, PreVisitComprehensionSubexpression(loop_condition, c,
                                                          LOOP_CONDITION, _))
      .Times(1);
  EXPECT_CALL(handler, PostVisitConst(loop_condition_expr, loop_condition, _))
      .Times(1);
  EXPECT_CALL(handler, PostVisitComprehensionSubexpression(loop_condition, c,
                                                           LOOP_CONDITION, _))
      .Times(1);

  // LOOP STEP
  EXPECT_CALL(handler,
              PreVisitComprehensionSubexpression(loop_step, c, LOOP_STEP, _))
      .Times(1);
  EXPECT_CALL(handler, PostVisitIdent(loop_step_expr, loop_step, _)).Times(1);
  EXPECT_CALL(handler,
              PostVisitComprehensionSubexpression(loop_step, c, LOOP_STEP, _))
      .Times(1);

  // RESULT
  EXPECT_CALL(handler, PreVisitComprehensionSubexpression(result, c, RESULT, _))
      .Times(1);

  EXPECT_CALL(handler, PostVisitConst(result_expr, result, _)).Times(1);

  EXPECT_CALL(handler,
              PostVisitComprehensionSubexpression(result, c, RESULT, _))
      .Times(1);

  EXPECT_CALL(handler, PostVisitComprehension(c, &expr, _)).Times(1);

  TraversalOptions opts;
  opts.use_comprehension_callbacks = true;
  AstTraverse(&expr, &source_info, &handler, opts);
}

// Test handling of Comprehension node
TEST(AstCrawlerTest, CheckCrawlComprehensionLegacyCallbacks) {
  SourceInfo source_info;
  MockAstVisitor handler;

  Expr expr;
  auto c = expr.mutable_comprehension_expr();
  auto iter_range = c->mutable_iter_range();
  auto iter_range_expr = iter_range->mutable_const_expr();
  auto accu_init = c->mutable_accu_init();
  auto accu_init_expr = accu_init->mutable_ident_expr();
  auto loop_condition = c->mutable_loop_condition();
  auto loop_condition_expr = loop_condition->mutable_const_expr();
  auto loop_step = c->mutable_loop_step();
  auto loop_step_expr = loop_step->mutable_ident_expr();
  auto result = c->mutable_result();
  auto result_expr = result->mutable_const_expr();

  testing::InSequence seq;

  // Lowest level entry will be called first
  EXPECT_CALL(handler, PreVisitComprehension(c, &expr, _)).Times(1);

  EXPECT_CALL(handler, PostVisitConst(iter_range_expr, iter_range, _)).Times(1);
  EXPECT_CALL(handler, PostVisitArg(ITER_RANGE, &expr, _)).Times(1);

  // ACCU_INIT
  EXPECT_CALL(handler, PostVisitIdent(accu_init_expr, accu_init, _)).Times(1);
  EXPECT_CALL(handler, PostVisitArg(ACCU_INIT, &expr, _)).Times(1);

  // LOOP CONDITION
  EXPECT_CALL(handler, PostVisitConst(loop_condition_expr, loop_condition, _))
      .Times(1);
  EXPECT_CALL(handler, PostVisitArg(LOOP_CONDITION, &expr, _)).Times(1);

  // LOOP STEP
  EXPECT_CALL(handler, PostVisitIdent(loop_step_expr, loop_step, _)).Times(1);
  EXPECT_CALL(handler, PostVisitArg(LOOP_STEP, &expr, _)).Times(1);

  // RESULT
  EXPECT_CALL(handler, PostVisitConst(result_expr, result, _)).Times(1);
  EXPECT_CALL(handler, PostVisitArg(RESULT, &expr, _)).Times(1);

  EXPECT_CALL(handler, PostVisitComprehension(c, &expr, _)).Times(1);

  AstTraverse(&expr, &source_info, &handler);
}

// Test handling of CreateList node.
TEST(AstCrawlerTest, CheckCreateList) {
  SourceInfo source_info;
  MockAstVisitor handler;

  Expr expr;
  auto list_expr = expr.mutable_list_expr();
  auto arg0 = list_expr->add_elements();
  auto const_expr = arg0->mutable_const_expr();
  auto arg1 = list_expr->add_elements();
  auto ident_expr = arg1->mutable_ident_expr();

  testing::InSequence seq;

  EXPECT_CALL(handler, PreVisitCreateList(list_expr, &expr, _)).Times(1);
  EXPECT_CALL(handler, PostVisitConst(const_expr, arg0, _)).Times(1);
  EXPECT_CALL(handler, PostVisitIdent(ident_expr, arg1, _)).Times(1);
  EXPECT_CALL(handler, PostVisitCreateList(list_expr, &expr, _)).Times(1);

  AstTraverse(&expr, &source_info, &handler);
}

// Test handling of CreateStruct node.
TEST(AstCrawlerTest, CheckCreateStruct) {
  SourceInfo source_info;
  MockAstVisitor handler;

  Expr expr;
  auto struct_expr = expr.mutable_struct_expr();
  auto entry0 = struct_expr->add_entries();

  auto key = entry0->mutable_map_key()->mutable_const_expr();
  auto value = entry0->mutable_value()->mutable_ident_expr();

  testing::InSequence seq;

  EXPECT_CALL(handler, PreVisitCreateStruct(struct_expr, &expr, _)).Times(1);
  EXPECT_CALL(handler, PostVisitConst(key, &entry0->map_key(), _)).Times(1);
  EXPECT_CALL(handler, PostVisitIdent(value, &entry0->value(), _)).Times(1);
  EXPECT_CALL(handler, PostVisitCreateStruct(struct_expr, &expr, _)).Times(1);

  AstTraverse(&expr, &source_info, &handler);
}

// Test generic Expr handlers.
TEST(AstCrawlerTest, CheckExprHandlers) {
  SourceInfo source_info;
  MockAstVisitor handler;

  Expr expr;
  auto struct_expr = expr.mutable_struct_expr();
  auto entry0 = struct_expr->add_entries();

  entry0->mutable_map_key()->mutable_const_expr();
  entry0->mutable_value()->mutable_ident_expr();

  EXPECT_CALL(handler, PreVisitExpr(_, _)).Times(3);
  EXPECT_CALL(handler, PostVisitExpr(_, _)).Times(3);

  AstTraverse(&expr, &source_info, &handler);
}

}  // namespace

}  // namespace google::api::expr::runtime
