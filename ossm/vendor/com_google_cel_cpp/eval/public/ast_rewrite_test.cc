// Copyright 2021 Google LLC
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

#include "eval/public/ast_rewrite.h"

#include <string>
#include <vector>

#include "cel/expr/syntax.pb.h"
#include "eval/public/ast_visitor.h"
#include "eval/public/source_position.h"
#include "internal/testing.h"
#include "parser/parser.h"
#include "testutil/util.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::expr::Constant;
using ::cel::expr::Expr;
using ::cel::expr::ParsedExpr;
using ::cel::expr::SourceInfo;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::InSequence;

using Ident = cel::expr::Expr::Ident;
using Select = cel::expr::Expr::Select;
using Call = cel::expr::Expr::Call;
using CreateList = cel::expr::Expr::CreateList;
using CreateStruct = cel::expr::Expr::CreateStruct;
using Comprehension = cel::expr::Expr::Comprehension;

class MockAstRewriter : public AstRewriter {
 public:
  // Expr handler.
  MOCK_METHOD(void, PreVisitExpr,
              (const Expr* expr, const SourcePosition* position), (override));

  // Expr handler.
  MOCK_METHOD(void, PostVisitExpr,
              (const Expr* expr, const SourcePosition* position), (override));

  MOCK_METHOD(void, PostVisitConst,
              (const Constant* const_expr, const Expr* expr,
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
  MOCK_METHOD(void, PostVisitCreateList,
              (const CreateList* list_expr, const Expr* expr,
               const SourcePosition* position),
              (override));

  // CreateStruct node handler group
  MOCK_METHOD(void, PostVisitCreateStruct,
              (const CreateStruct* struct_expr, const Expr* expr,
               const SourcePosition* position),
              (override));

  MOCK_METHOD(bool, PreVisitRewrite,
              (Expr * expr, const SourcePosition* position), (override));

  MOCK_METHOD(bool, PostVisitRewrite,
              (Expr * expr, const SourcePosition* position), (override));

  MOCK_METHOD(void, TraversalStackUpdate, (absl::Span<const Expr*> path),
              (override));
};

TEST(AstCrawlerTest, CheckCrawlConstant) {
  SourceInfo source_info;
  MockAstRewriter handler;

  Expr expr;
  auto const_expr = expr.mutable_const_expr();

  EXPECT_CALL(handler, PostVisitConst(const_expr, &expr, _)).Times(1);

  AstRewrite(&expr, &source_info, &handler);
}

TEST(AstCrawlerTest, CheckCrawlIdent) {
  SourceInfo source_info;
  MockAstRewriter handler;

  Expr expr;
  auto ident_expr = expr.mutable_ident_expr();

  EXPECT_CALL(handler, PostVisitIdent(ident_expr, &expr, _)).Times(1);

  AstRewrite(&expr, &source_info, &handler);
}

// Test handling of Select node when operand is not set.
TEST(AstCrawlerTest, CheckCrawlSelectNotCrashingPostVisitAbsentOperand) {
  SourceInfo source_info;
  MockAstRewriter handler;

  Expr expr;
  auto select_expr = expr.mutable_select_expr();

  // Lowest level entry will be called first
  EXPECT_CALL(handler, PostVisitSelect(select_expr, &expr, _)).Times(1);

  AstRewrite(&expr, &source_info, &handler);
}

// Test handling of Select node
TEST(AstCrawlerTest, CheckCrawlSelect) {
  SourceInfo source_info;
  MockAstRewriter handler;

  Expr expr;
  auto select_expr = expr.mutable_select_expr();
  auto operand = select_expr->mutable_operand();
  auto ident_expr = operand->mutable_ident_expr();

  testing::InSequence seq;

  // Lowest level entry will be called first
  EXPECT_CALL(handler, PostVisitIdent(ident_expr, operand, _)).Times(1);
  EXPECT_CALL(handler, PostVisitSelect(select_expr, &expr, _)).Times(1);

  AstRewrite(&expr, &source_info, &handler);
}

// Test handling of Call node without receiver
TEST(AstCrawlerTest, CheckCrawlCallNoReceiver) {
  SourceInfo source_info;
  MockAstRewriter handler;

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

  AstRewrite(&expr, &source_info, &handler);
}

// Test handling of Call node with receiver
TEST(AstCrawlerTest, CheckCrawlCallReceiver) {
  SourceInfo source_info;
  MockAstRewriter handler;

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

  AstRewrite(&expr, &source_info, &handler);
}

// Test handling of Comprehension node
TEST(AstCrawlerTest, CheckCrawlComprehension) {
  SourceInfo source_info;
  MockAstRewriter handler;

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

  RewriteTraversalOptions opts;
  opts.use_comprehension_callbacks = true;
  AstRewrite(&expr, &source_info, &handler, opts);
}

// Test handling of Comprehension node
TEST(AstCrawlerTest, CheckCrawlComprehensionLegacyCallbacks) {
  SourceInfo source_info;
  MockAstRewriter handler;

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

  AstRewrite(&expr, &source_info, &handler);
}

// Test handling of CreateList node.
TEST(AstCrawlerTest, CheckCreateList) {
  SourceInfo source_info;
  MockAstRewriter handler;

  Expr expr;
  auto list_expr = expr.mutable_list_expr();
  auto arg0 = list_expr->add_elements();
  auto const_expr = arg0->mutable_const_expr();
  auto arg1 = list_expr->add_elements();
  auto ident_expr = arg1->mutable_ident_expr();

  testing::InSequence seq;

  EXPECT_CALL(handler, PostVisitConst(const_expr, arg0, _)).Times(1);
  EXPECT_CALL(handler, PostVisitIdent(ident_expr, arg1, _)).Times(1);
  EXPECT_CALL(handler, PostVisitCreateList(list_expr, &expr, _)).Times(1);

  AstRewrite(&expr, &source_info, &handler);
}

// Test handling of CreateStruct node.
TEST(AstCrawlerTest, CheckCreateStruct) {
  SourceInfo source_info;
  MockAstRewriter handler;

  Expr expr;
  auto struct_expr = expr.mutable_struct_expr();
  auto entry0 = struct_expr->add_entries();

  auto key = entry0->mutable_map_key()->mutable_const_expr();
  auto value = entry0->mutable_value()->mutable_ident_expr();

  testing::InSequence seq;

  EXPECT_CALL(handler, PostVisitConst(key, &entry0->map_key(), _)).Times(1);
  EXPECT_CALL(handler, PostVisitIdent(value, &entry0->value(), _)).Times(1);
  EXPECT_CALL(handler, PostVisitCreateStruct(struct_expr, &expr, _)).Times(1);

  AstRewrite(&expr, &source_info, &handler);
}

// Test generic Expr handlers.
TEST(AstCrawlerTest, CheckExprHandlers) {
  SourceInfo source_info;
  MockAstRewriter handler;

  Expr expr;
  auto struct_expr = expr.mutable_struct_expr();
  auto entry0 = struct_expr->add_entries();

  entry0->mutable_map_key()->mutable_const_expr();
  entry0->mutable_value()->mutable_ident_expr();

  EXPECT_CALL(handler, PreVisitExpr(_, _)).Times(3);
  EXPECT_CALL(handler, PostVisitExpr(_, _)).Times(3);

  AstRewrite(&expr, &source_info, &handler);
}

// Test generic Expr handlers.
TEST(AstCrawlerTest, CheckExprRewriteHandlers) {
  SourceInfo source_info;
  MockAstRewriter handler;

  Expr select_expr;
  select_expr.mutable_select_expr()->set_field("var");
  auto* inner_select_expr =
      select_expr.mutable_select_expr()->mutable_operand();
  inner_select_expr->mutable_select_expr()->set_field("mid");
  auto* ident = inner_select_expr->mutable_select_expr()->mutable_operand();
  ident->mutable_ident_expr()->set_name("top");

  {
    InSequence sequence;
    EXPECT_CALL(handler,
                TraversalStackUpdate(testing::ElementsAre(&select_expr)));
    EXPECT_CALL(handler, PreVisitRewrite(&select_expr, _));

    EXPECT_CALL(handler, TraversalStackUpdate(testing::ElementsAre(
                             &select_expr, inner_select_expr)));
    EXPECT_CALL(handler, PreVisitRewrite(inner_select_expr, _));

    EXPECT_CALL(handler, TraversalStackUpdate(testing::ElementsAre(
                             &select_expr, inner_select_expr, ident)));
    EXPECT_CALL(handler, PreVisitRewrite(ident, _));

    EXPECT_CALL(handler, PostVisitRewrite(ident, _));
    EXPECT_CALL(handler, TraversalStackUpdate(testing::ElementsAre(
                             &select_expr, inner_select_expr)));

    EXPECT_CALL(handler, PostVisitRewrite(inner_select_expr, _));
    EXPECT_CALL(handler,
                TraversalStackUpdate(testing::ElementsAre(&select_expr)));

    EXPECT_CALL(handler, PostVisitRewrite(&select_expr, _));
    EXPECT_CALL(handler, TraversalStackUpdate(testing::IsEmpty()));
  }

  EXPECT_FALSE(AstRewrite(&select_expr, &source_info, &handler));
}

// Simple rewrite that replaces a select path with a dot-qualified identifier.
class RewriterExample : public AstRewriterBase {
 public:
  RewriterExample() {}
  bool PostVisitRewrite(Expr* expr, const SourcePosition* info) override {
    if (target_.has_value() && expr->id() == *target_) {
      expr->mutable_ident_expr()->set_name("com.google.Identifier");
      return true;
    }
    return false;
  }

  void PostVisitIdent(const Ident* ident, const Expr* expr,
                      const SourcePosition* pos) override {
    if (path_.size() >= 3) {
      if (ident->name() == "com") {
        const Expr* p1 = path_.at(path_.size() - 2);
        const Expr* p2 = path_.at(path_.size() - 3);

        if (p1->has_select_expr() && p1->select_expr().field() == "google" &&
            p2->has_select_expr() &&
            p2->select_expr().field() == "Identifier") {
          target_ = p2->id();
        }
      }
    }
  }

  void TraversalStackUpdate(absl::Span<const Expr*> path) override {
    path_ = path;
  }

 private:
  absl::Span<const Expr*> path_;
  absl::optional<int64_t> target_;
};

TEST(AstRewrite, SelectRewriteExample) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed,
                       parser::Parse("com.google.Identifier"));
  RewriterExample example;
  ASSERT_TRUE(
      AstRewrite(parsed.mutable_expr(), &parsed.source_info(), &example));

  EXPECT_THAT(parsed.expr(), testutil::EqualsProto(R"pb(
                id: 3
                ident_expr { name: "com.google.Identifier" }
              )pb"));
}

// Rewrites x -> y -> z to demonstrate traversal when a node is rewritten on
// both passes.
class PreRewriterExample : public AstRewriterBase {
 public:
  PreRewriterExample() {}
  bool PreVisitRewrite(Expr* expr, const SourcePosition* info) override {
    if (expr->ident_expr().name() == "x") {
      expr->mutable_ident_expr()->set_name("y");
      return true;
    }
    return false;
  }

  bool PostVisitRewrite(Expr* expr, const SourcePosition* info) override {
    if (expr->ident_expr().name() == "y") {
      expr->mutable_ident_expr()->set_name("z");
      return true;
    }
    return false;
  }

  void PostVisitIdent(const Ident* ident, const Expr* expr,
                      const SourcePosition* pos) override {
    visited_idents_.push_back(ident->name());
  }

  const std::vector<std::string>& visited_idents() const {
    return visited_idents_;
  }

 private:
  std::vector<std::string> visited_idents_;
};

TEST(AstRewrite, PreAndPostVisitExpample) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed, parser::Parse("x"));
  PreRewriterExample visitor;
  ASSERT_TRUE(
      AstRewrite(parsed.mutable_expr(), &parsed.source_info(), &visitor));

  EXPECT_THAT(parsed.expr(), testutil::EqualsProto(R"pb(
                id: 1
                ident_expr { name: "z" }
              )pb"));
  EXPECT_THAT(visitor.visited_idents(), ElementsAre("y"));
}

}  // namespace

}  // namespace google::api::expr::runtime
