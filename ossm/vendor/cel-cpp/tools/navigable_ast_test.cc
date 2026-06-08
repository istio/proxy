// Copyright 2023 Google LLC
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

#include "tools/navigable_ast.h"

#include <utility>
#include <vector>

#include "cel/expr/syntax.pb.h"
#include "base/builtins.h"
#include "internal/testing.h"
#include "parser/parser.h"

namespace cel {
namespace {

using ::cel::expr::Expr;
using ::google::api::expr::parser::Parse;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::SizeIs;

TEST(NavigableProtoAst, Basic) {
  Expr const_node;
  const_node.set_id(1);
  const_node.mutable_const_expr()->set_int64_value(42);

  NavigableProtoAst ast = NavigableProtoAst::Build(const_node);
  EXPECT_TRUE(ast.IdsAreUnique());

  const NavigableProtoAstNode& root = ast.Root();

  EXPECT_EQ(root.expr(), &const_node);
  EXPECT_THAT(root.children(), IsEmpty());
  EXPECT_TRUE(root.parent() == nullptr);
  EXPECT_EQ(root.child_index(), -1);
  EXPECT_EQ(root.node_kind(), NodeKind::kConstant);
  EXPECT_EQ(root.parent_relation(), ChildKind::kUnspecified);
}

TEST(NavigableProtoAst, DefaultCtorEmpty) {
  Expr const_node;
  const_node.set_id(1);
  const_node.mutable_const_expr()->set_int64_value(42);

  NavigableProtoAst ast = NavigableProtoAst::Build(const_node);
  EXPECT_EQ(ast, ast);

  NavigableProtoAst empty;

  EXPECT_NE(ast, empty);
  EXPECT_EQ(empty, empty);

  EXPECT_TRUE(static_cast<bool>(ast));
  EXPECT_FALSE(static_cast<bool>(empty));

  NavigableProtoAst moved = std::move(ast);
  EXPECT_EQ(ast, empty);
  EXPECT_FALSE(static_cast<bool>(ast));
  EXPECT_TRUE(static_cast<bool>(moved));
}

TEST(NavigableProtoAst, FindById) {
  Expr const_node;
  const_node.set_id(1);
  const_node.mutable_const_expr()->set_int64_value(42);

  NavigableProtoAst ast = NavigableProtoAst::Build(const_node);

  const NavigableProtoAstNode& root = ast.Root();

  EXPECT_EQ(ast.FindId(const_node.id()), &root);
  EXPECT_EQ(ast.FindId(-1), nullptr);
}

MATCHER_P(AstNodeWrapping, expr, "") {
  const NavigableProtoAstNode* ptr = arg;
  return ptr != nullptr && ptr->expr() == expr;
}

TEST(NavigableProtoAst, ToleratesNonUnique) {
  Expr call_node;
  call_node.set_id(1);
  call_node.mutable_call_expr()->set_function(cel::builtin::kNot);
  Expr* const_node = call_node.mutable_call_expr()->add_args();
  const_node->mutable_const_expr()->set_bool_value(false);
  const_node->set_id(1);

  NavigableProtoAst ast = NavigableProtoAst::Build(call_node);

  const NavigableProtoAstNode& root = ast.Root();

  EXPECT_EQ(ast.FindId(1), &root);
  EXPECT_EQ(ast.FindExpr(&call_node), &root);
  EXPECT_FALSE(ast.IdsAreUnique());
  EXPECT_THAT(ast.FindExpr(const_node), AstNodeWrapping(const_node));
}

TEST(NavigableProtoAst, FindByExprPtr) {
  Expr const_node;
  const_node.set_id(1);
  const_node.mutable_const_expr()->set_int64_value(42);

  NavigableProtoAst ast = NavigableProtoAst::Build(const_node);

  const NavigableProtoAstNode& root = ast.Root();

  EXPECT_EQ(ast.FindExpr(&const_node), &root);
  EXPECT_EQ(ast.FindExpr(&Expr::default_instance()), nullptr);
}

TEST(NavigableProtoAst, Children) {
  ASSERT_OK_AND_ASSIGN(auto parsed_expr, Parse("1 + 2"));

  NavigableProtoAst ast = NavigableProtoAst::Build(parsed_expr.expr());
  const NavigableProtoAstNode& root = ast.Root();

  EXPECT_EQ(root.expr(), &parsed_expr.expr());
  EXPECT_THAT(root.children(), SizeIs(2));
  EXPECT_TRUE(root.parent() == nullptr);
  EXPECT_EQ(root.child_index(), -1);
  EXPECT_EQ(root.parent_relation(), ChildKind::kUnspecified);
  EXPECT_EQ(root.node_kind(), NodeKind::kCall);

  EXPECT_THAT(
      root.children(),
      ElementsAre(AstNodeWrapping(&parsed_expr.expr().call_expr().args(0)),
                  AstNodeWrapping(&parsed_expr.expr().call_expr().args(1))));

  ASSERT_THAT(root.children(), SizeIs(2));
  const auto* child1 = root.children()[0];
  EXPECT_EQ(child1->child_index(), 0);
  EXPECT_EQ(child1->parent(), &root);
  EXPECT_EQ(child1->parent_relation(), ChildKind::kCallArg);
  EXPECT_EQ(child1->node_kind(), NodeKind::kConstant);
  EXPECT_THAT(child1->children(), IsEmpty());

  const auto* child2 = root.children()[1];
  EXPECT_EQ(child2->child_index(), 1);
}

TEST(NavigableProtoAst, UnspecifiedExpr) {
  Expr expr;
  expr.set_id(1);
  NavigableProtoAst ast = NavigableProtoAst::Build(expr);
  const NavigableProtoAstNode& root = ast.Root();

  EXPECT_EQ(root.expr(), &expr);
  EXPECT_THAT(root.children(), SizeIs(0));
  EXPECT_TRUE(root.parent() == nullptr);
  EXPECT_EQ(root.child_index(), -1);
  EXPECT_EQ(root.node_kind(), NodeKind::kUnspecified);
}

TEST(NavigableProtoAst, ParentRelationSelect) {
  ASSERT_OK_AND_ASSIGN(auto parsed_expr, Parse("a.b"));

  NavigableProtoAst ast = NavigableProtoAst::Build(parsed_expr.expr());
  const NavigableProtoAstNode& root = ast.Root();

  ASSERT_THAT(root.children(), SizeIs(1));
  const auto* child = root.children()[0];

  EXPECT_EQ(child->parent_relation(), ChildKind::kSelectOperand);
  EXPECT_EQ(child->node_kind(), NodeKind::kIdent);
}

TEST(NavigableProtoAst, ParentRelationCallReceiver) {
  ASSERT_OK_AND_ASSIGN(auto parsed_expr, Parse("a.b()"));

  NavigableProtoAst ast = NavigableProtoAst::Build(parsed_expr.expr());
  const NavigableProtoAstNode& root = ast.Root();

  ASSERT_THAT(root.children(), SizeIs(1));
  const auto* child = root.children()[0];

  EXPECT_EQ(child->parent_relation(), ChildKind::kCallReceiver);
  EXPECT_EQ(child->node_kind(), NodeKind::kIdent);
}

TEST(NavigableProtoAst, ParentRelationCreateStruct) {
  ASSERT_OK_AND_ASSIGN(auto parsed_expr,
                       Parse("com.example.Type{field: '123'}"));

  NavigableProtoAst ast = NavigableProtoAst::Build(parsed_expr.expr());
  const NavigableProtoAstNode& root = ast.Root();

  EXPECT_EQ(root.node_kind(), NodeKind::kStruct);
  ASSERT_THAT(root.children(), SizeIs(1));
  const auto* child = root.children()[0];

  EXPECT_EQ(child->parent_relation(), ChildKind::kStructValue);
  EXPECT_EQ(child->node_kind(), NodeKind::kConstant);
}

TEST(NavigableProtoAst, ParentRelationCreateMap) {
  ASSERT_OK_AND_ASSIGN(auto parsed_expr, Parse("{'a': 123}"));

  NavigableProtoAst ast = NavigableProtoAst::Build(parsed_expr.expr());
  const NavigableProtoAstNode& root = ast.Root();

  EXPECT_EQ(root.node_kind(), NodeKind::kMap);
  ASSERT_THAT(root.children(), SizeIs(2));
  const auto* key = root.children()[0];
  const auto* value = root.children()[1];

  EXPECT_EQ(key->parent_relation(), ChildKind::kMapKey);
  EXPECT_EQ(key->node_kind(), NodeKind::kConstant);

  EXPECT_EQ(value->parent_relation(), ChildKind::kMapValue);
  EXPECT_EQ(value->node_kind(), NodeKind::kConstant);
}

TEST(NavigableProtoAst, ParentRelationCreateList) {
  ASSERT_OK_AND_ASSIGN(auto parsed_expr, Parse("[123]"));

  NavigableProtoAst ast = NavigableProtoAst::Build(parsed_expr.expr());
  const NavigableProtoAstNode& root = ast.Root();

  EXPECT_EQ(root.node_kind(), NodeKind::kList);
  ASSERT_THAT(root.children(), SizeIs(1));
  const auto* child = root.children()[0];

  EXPECT_EQ(child->parent_relation(), ChildKind::kListElem);
  EXPECT_EQ(child->node_kind(), NodeKind::kConstant);
}

TEST(NavigableProtoAst, ParentRelationComprehension) {
  ASSERT_OK_AND_ASSIGN(auto parsed_expr, Parse("[1].all(x, x < 2)"));

  NavigableProtoAst ast = NavigableProtoAst::Build(parsed_expr.expr());
  const NavigableProtoAstNode& root = ast.Root();

  EXPECT_EQ(root.node_kind(), NodeKind::kComprehension);
  ASSERT_THAT(root.children(), SizeIs(5));
  const auto* range = root.children()[0];
  const auto* init = root.children()[1];
  const auto* condition = root.children()[2];
  const auto* step = root.children()[3];
  const auto* finish = root.children()[4];

  EXPECT_EQ(range->parent_relation(), ChildKind::kComprehensionRange);
  EXPECT_EQ(init->parent_relation(), ChildKind::kComprehensionInit);
  EXPECT_EQ(condition->parent_relation(), ChildKind::kComprehensionCondition);
  EXPECT_EQ(step->parent_relation(), ChildKind::kComprehensionLoopStep);
  EXPECT_EQ(finish->parent_relation(), ChildKind::kComprensionResult);
}

TEST(NavigableProtoAst, DescendantsPostorder) {
  ASSERT_OK_AND_ASSIGN(auto parsed_expr, Parse("1 + (x * 3)"));

  NavigableProtoAst ast = NavigableProtoAst::Build(parsed_expr.expr());
  const NavigableProtoAstNode& root = ast.Root();

  EXPECT_EQ(root.node_kind(), NodeKind::kCall);

  std::vector<int> constants;
  std::vector<NodeKind> node_kinds;

  for (const NavigableProtoAstNode& node : root.DescendantsPostorder()) {
    if (node.node_kind() == NodeKind::kConstant) {
      constants.push_back(node.expr()->const_expr().int64_value());
    }
    node_kinds.push_back(node.node_kind());
  }

  EXPECT_THAT(node_kinds, ElementsAre(NodeKind::kConstant, NodeKind::kIdent,
                                      NodeKind::kConstant, NodeKind::kCall,
                                      NodeKind::kCall));
  EXPECT_THAT(constants, ElementsAre(1, 3));
}

TEST(NavigableProtoAst, DescendantsPreorder) {
  ASSERT_OK_AND_ASSIGN(auto parsed_expr, Parse("1 + (x * 3)"));

  NavigableProtoAst ast = NavigableProtoAst::Build(parsed_expr.expr());
  const NavigableProtoAstNode& root = ast.Root();

  EXPECT_EQ(root.node_kind(), NodeKind::kCall);

  std::vector<int> constants;
  std::vector<NodeKind> node_kinds;

  for (const NavigableProtoAstNode& node : root.DescendantsPreorder()) {
    if (node.node_kind() == NodeKind::kConstant) {
      constants.push_back(node.expr()->const_expr().int64_value());
    }
    node_kinds.push_back(node.node_kind());
  }

  EXPECT_THAT(node_kinds,
              ElementsAre(NodeKind::kCall, NodeKind::kConstant, NodeKind::kCall,
                          NodeKind::kIdent, NodeKind::kConstant));
  EXPECT_THAT(constants, ElementsAre(1, 3));
}

TEST(NavigableProtoAst, DescendantsPreorderComprehension) {
  ASSERT_OK_AND_ASSIGN(auto parsed_expr, Parse("[1, 2, 3].map(x, x + 1)"));

  NavigableProtoAst ast = NavigableProtoAst::Build(parsed_expr.expr());
  const NavigableProtoAstNode& root = ast.Root();

  EXPECT_EQ(root.node_kind(), NodeKind::kComprehension);

  std::vector<std::pair<NodeKind, ChildKind>> node_kinds;

  for (const NavigableProtoAstNode& node : root.DescendantsPreorder()) {
    node_kinds.push_back(
        std::make_pair(node.node_kind(), node.parent_relation()));
  }

  EXPECT_THAT(
      node_kinds,
      ElementsAre(Pair(NodeKind::kComprehension, ChildKind::kUnspecified),
                  Pair(NodeKind::kList, ChildKind::kComprehensionRange),
                  Pair(NodeKind::kConstant, ChildKind::kListElem),
                  Pair(NodeKind::kConstant, ChildKind::kListElem),
                  Pair(NodeKind::kConstant, ChildKind::kListElem),
                  Pair(NodeKind::kList, ChildKind::kComprehensionInit),
                  Pair(NodeKind::kConstant, ChildKind::kComprehensionCondition),
                  Pair(NodeKind::kCall, ChildKind::kComprehensionLoopStep),
                  Pair(NodeKind::kIdent, ChildKind::kCallArg),
                  Pair(NodeKind::kList, ChildKind::kCallArg),
                  Pair(NodeKind::kCall, ChildKind::kListElem),
                  Pair(NodeKind::kIdent, ChildKind::kCallArg),
                  Pair(NodeKind::kConstant, ChildKind::kCallArg),
                  Pair(NodeKind::kIdent, ChildKind::kComprensionResult)));
}

TEST(NavigableProtoAst, TreeSize) {
  ASSERT_OK_AND_ASSIGN(auto parsed_expr, Parse("[1, 2, 3].map(x, x + 1)"));

  NavigableProtoAst ast = NavigableProtoAst::Build(parsed_expr.expr());
  const NavigableProtoAstNode& root = ast.Root();

  EXPECT_EQ(root.node_kind(), NodeKind::kComprehension);

  std::vector<std::pair<NodeKind, ChildKind>> node_kinds;

  EXPECT_EQ(root.tree_size(), 14);
  auto it = root.DescendantsPostorder().begin();
  EXPECT_EQ(it->tree_size(), 1);
}

TEST(NavigableProtoAst, Height) {
  ASSERT_OK_AND_ASSIGN(auto parsed_expr, Parse("[1, 2, 3].map(x, x + 1)"));

  NavigableProtoAst ast = NavigableProtoAst::Build(parsed_expr.expr());
  const NavigableProtoAstNode& root = ast.Root();

  EXPECT_EQ(root.node_kind(), NodeKind::kComprehension);

  std::vector<std::pair<NodeKind, ChildKind>> node_kinds;

  EXPECT_EQ(root.height(), 5);
  auto it = root.DescendantsPostorder().begin();
  EXPECT_EQ(it->height(), 1);
}

TEST(NavigableProtoAst, DescendantsPreorderCreateMap) {
  ASSERT_OK_AND_ASSIGN(auto parsed_expr, Parse("{'key1': 1, 'key2': 2}"));

  NavigableProtoAst ast = NavigableProtoAst::Build(parsed_expr.expr());
  const NavigableProtoAstNode& root = ast.Root();

  EXPECT_EQ(root.node_kind(), NodeKind::kMap);

  std::vector<std::pair<NodeKind, ChildKind>> node_kinds;

  for (const NavigableProtoAstNode& node : root.DescendantsPreorder()) {
    node_kinds.push_back(
        std::make_pair(node.node_kind(), node.parent_relation()));
  }

  EXPECT_THAT(node_kinds,
              ElementsAre(Pair(NodeKind::kMap, ChildKind::kUnspecified),
                          Pair(NodeKind::kConstant, ChildKind::kMapKey),
                          Pair(NodeKind::kConstant, ChildKind::kMapValue),
                          Pair(NodeKind::kConstant, ChildKind::kMapKey),
                          Pair(NodeKind::kConstant, ChildKind::kMapValue)));
}

}  // namespace
}  // namespace cel
