// Copyright 2025 Google LLC
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

#include "common/navigable_ast.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "common/ast/navigable_ast_internal.h"
#include "common/ast_traverse.h"
#include "common/ast_visitor.h"
#include "common/ast_visitor_base.h"
#include "common/expr.h"

namespace cel {

namespace {

using NavigableAstNodeData =
    common_internal::NavigableAstNodeData<common_internal::NativeAstTraits>;
using NavigableAstMetadata =
    common_internal::NavigableAstMetadata<common_internal::NativeAstTraits>;

NodeKind GetNodeKind(const Expr& expr) {
  switch (expr.kind_case()) {
    case ExprKindCase::kConstant:
      return NodeKind::kConstant;
    case ExprKindCase::kIdentExpr:
      return NodeKind::kIdent;
    case ExprKindCase::kSelectExpr:
      return NodeKind::kSelect;
    case ExprKindCase::kCallExpr:
      return NodeKind::kCall;
    case ExprKindCase::kListExpr:
      return NodeKind::kList;
    case ExprKindCase::kStructExpr:
      return NodeKind::kStruct;
    case ExprKindCase::kMapExpr:
      return NodeKind::kMap;
    case ExprKindCase::kComprehensionExpr:
      return NodeKind::kComprehension;
    case ExprKindCase::kUnspecifiedExpr:
    default:
      return NodeKind::kUnspecified;
  }
}

// Get the traversal relationship from parent to the given node.
// Note: these depend on the ast_visitor utility's traversal ordering.
ChildKind GetChildKind(const NavigableAstNodeData& parent_node,
                       size_t child_index,
                       absl::optional<ComprehensionArg> comprehension_arg) {
  switch (parent_node.node_kind) {
    case NodeKind::kStruct:
      return ChildKind::kStructValue;
    case NodeKind::kMap:
      if (child_index % 2 == 0) {
        return ChildKind::kMapKey;
      }
      return ChildKind::kMapValue;
    case NodeKind::kList:
      return ChildKind::kListElem;
    case NodeKind::kSelect:
      return ChildKind::kSelectOperand;
    case NodeKind::kCall:
      if (child_index == 0 && parent_node.expr->call_expr().has_target()) {
        return ChildKind::kCallReceiver;
      }
      return ChildKind::kCallArg;
    case NodeKind::kComprehension:
      if (!comprehension_arg.has_value()) {
        return ChildKind::kUnspecified;
      }
      switch (*comprehension_arg) {
        case ComprehensionArg::ITER_RANGE:
          return ChildKind::kComprehensionRange;
        case ComprehensionArg::ACCU_INIT:
          return ChildKind::kComprehensionInit;
        case ComprehensionArg::LOOP_CONDITION:
          return ChildKind::kComprehensionCondition;
        case ComprehensionArg::LOOP_STEP:
          return ChildKind::kComprehensionLoopStep;
        case ComprehensionArg::RESULT:
          return ChildKind::kComprensionResult;
        default:
          return ChildKind::kUnspecified;
      }
    default:
      return ChildKind::kUnspecified;
  }
}

class NavigableExprBuilderVisitor : public cel::AstVisitorBase {
 public:
  NavigableExprBuilderVisitor(
      absl::AnyInvocable<std::unique_ptr<NavigableAstNode>()> node_factory,
      absl::AnyInvocable<NavigableAstNodeData&(NavigableAstNode&)>
          node_data_accessor)
      : node_factory_(std::move(node_factory)),
        node_data_accessor_(std::move(node_data_accessor)),
        metadata_(std::make_unique<NavigableAstMetadata>()) {}

  NavigableAstNodeData& NodeDataAt(size_t index) {
    return node_data_accessor_(*metadata_->nodes[index]);
  }

  void PreVisitExpr(const Expr& expr) override {
    NavigableAstNode* parent =
        parent_stack_.empty() ? nullptr
                              : metadata_->nodes[parent_stack_.back()].get();
    size_t index = metadata_->nodes.size();
    metadata_->nodes.push_back(node_factory_());
    NavigableAstNode* node = metadata_->nodes[index].get();
    auto& node_data = NodeDataAt(index);
    node_data.parent = parent;
    node_data.expr = &expr;
    node_data.parent_relation = ChildKind::kUnspecified;
    node_data.node_kind = GetNodeKind(expr);
    node_data.tree_size = 1;
    node_data.height = 1;
    node_data.index = index;
    node_data.child_index = -1;
    node_data.metadata = metadata_.get();

    metadata_->id_to_node.insert({expr.id(), node});
    metadata_->expr_to_node.insert({&expr, node});
    if (!parent_stack_.empty()) {
      auto& parent_node_data = NodeDataAt(parent_stack_.back());
      size_t child_index = parent_node_data.children.size();
      parent_node_data.children.push_back(node);
      node_data.parent_relation =
          GetChildKind(parent_node_data, child_index, comprehension_arg_);
      node_data.child_index = child_index;
    }
    parent_stack_.push_back(index);
  }

  void PreVisitComprehensionSubexpression(
      const Expr& expr, const ComprehensionExpr& comprehension,
      ComprehensionArg comprehension_arg) override {
    comprehension_arg_ = comprehension_arg;
  }

  void PostVisitExpr(const Expr& expr) override {
    size_t idx = parent_stack_.back();
    parent_stack_.pop_back();
    metadata_->postorder.push_back(metadata_->nodes[idx].get());
    NavigableAstNodeData& node = NodeDataAt(idx);
    if (!parent_stack_.empty()) {
      auto& parent_node_data = NodeDataAt(parent_stack_.back());
      parent_node_data.tree_size += node.tree_size;
      parent_node_data.height =
          std::max(parent_node_data.height, node.height + 1);
    }
  }

  std::unique_ptr<NavigableAstMetadata> Consume() && {
    return std::move(metadata_);
  }

 private:
  absl::AnyInvocable<std::unique_ptr<NavigableAstNode>()> node_factory_;
  absl::AnyInvocable<NavigableAstNodeData&(NavigableAstNode&)>
      node_data_accessor_;
  std::unique_ptr<NavigableAstMetadata> metadata_;
  std::vector<size_t> parent_stack_;
  absl::optional<ComprehensionArg> comprehension_arg_;
};

}  // namespace

NavigableAst NavigableAst::Build(const Expr& expr) {
  cel::TraversalOptions opts;
  opts.use_comprehension_callbacks = true;
  NavigableExprBuilderVisitor visitor(
      []() { return absl::WrapUnique(new NavigableAstNode()); },
      [](NavigableAstNode& node) -> NavigableAstNodeData& {
        return node.data_;
      });
  AstTraverse(expr, visitor, opts);
  return NavigableAst(std::move(visitor).Consume());
}

}  // namespace cel
