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

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cel/expr/checked.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/memory/memory.h"
#include "common/ast/navigable_ast_internal.h"
#include "eval/public/ast_traverse.h"
#include "eval/public/ast_visitor.h"
#include "eval/public/ast_visitor_base.h"
#include "eval/public/source_position.h"

namespace cel {

namespace {

using ::cel::expr::Expr;
using ::google::api::expr::runtime::AstTraverse;
using ::google::api::expr::runtime::SourcePosition;

using AstNode = NavigableProtoAstNode;
using NavigableAstNodeData =
    common_internal::NavigableAstNodeData<common_internal::ProtoAstTraits>;
using NavigableAstMetadata =
    common_internal::NavigableAstMetadata<common_internal::ProtoAstTraits>;

NodeKind GetNodeKind(const Expr& expr) {
  switch (expr.expr_kind_case()) {
    case Expr::kConstExpr:
      return NodeKind::kConstant;
    case Expr::kIdentExpr:
      return NodeKind::kIdent;
    case Expr::kSelectExpr:
      return NodeKind::kSelect;
    case Expr::kCallExpr:
      return NodeKind::kCall;
    case Expr::kListExpr:
      return NodeKind::kList;
    case Expr::kStructExpr:
      if (!expr.struct_expr().message_name().empty()) {
        return NodeKind::kStruct;
      } else {
        return NodeKind::kMap;
      }
    case Expr::kComprehensionExpr:
      return NodeKind::kComprehension;
    case Expr::EXPR_KIND_NOT_SET:
    default:
      return NodeKind::kUnspecified;
  }
}

// Get the traversal relationship from parent to the given node.
// Note: these depend on the ast_visitor utility's traversal ordering.
ChildKind GetChildKind(const NavigableAstNodeData& parent_node,
                       size_t child_index) {
  constexpr size_t kComprehensionRangeArgIndex =
      google::api::expr::runtime::ITER_RANGE;
  constexpr size_t kComprehensionInitArgIndex =
      google::api::expr::runtime::ACCU_INIT;
  constexpr size_t kComprehensionConditionArgIndex =
      google::api::expr::runtime::LOOP_CONDITION;
  constexpr size_t kComprehensionLoopStepArgIndex =
      google::api::expr::runtime::LOOP_STEP;
  constexpr size_t kComprehensionResultArgIndex =
      google::api::expr::runtime::RESULT;

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
      switch (child_index) {
        case kComprehensionRangeArgIndex:
          return ChildKind::kComprehensionRange;
        case kComprehensionInitArgIndex:
          return ChildKind::kComprehensionInit;
        case kComprehensionConditionArgIndex:
          return ChildKind::kComprehensionCondition;
        case kComprehensionLoopStepArgIndex:
          return ChildKind::kComprehensionLoopStep;
        case kComprehensionResultArgIndex:
          return ChildKind::kComprensionResult;
        default:
          return ChildKind::kUnspecified;
      }
    default:
      return ChildKind::kUnspecified;
  }
}

class NavigableExprBuilderVisitor
    : public google::api::expr::runtime::AstVisitorBase {
 public:
  NavigableExprBuilderVisitor(
      absl::AnyInvocable<std::unique_ptr<AstNode>()> node_factory,
      absl::AnyInvocable<NavigableAstNodeData&(AstNode&)> node_data_accessor)
      : node_factory_(std::move(node_factory)),
        node_data_accessor_(std::move(node_data_accessor)),
        metadata_(std::make_unique<NavigableAstMetadata>()) {}

  NavigableAstNodeData& NodeDataAt(size_t index) {
    return node_data_accessor_(*metadata_->nodes[index]);
  }

  void PreVisitExpr(const Expr* expr, const SourcePosition* position) override {
    NavigableProtoAstNode* parent =
        parent_stack_.empty() ? nullptr
                              : metadata_->nodes[parent_stack_.back()].get();
    size_t index = metadata_->nodes.size();
    metadata_->nodes.push_back(node_factory_());
    NavigableProtoAstNode* node = metadata_->nodes[index].get();
    auto& node_data = NodeDataAt(index);
    node_data.parent = parent;
    node_data.expr = expr;
    node_data.parent_relation = ChildKind::kUnspecified;
    node_data.node_kind = GetNodeKind(*expr);
    node_data.tree_size = 1;
    node_data.height = 1;
    node_data.index = index;
    node_data.child_index = -1;
    node_data.metadata = metadata_.get();

    metadata_->id_to_node.insert({expr->id(), node});
    metadata_->expr_to_node.insert({expr, node});
    if (!parent_stack_.empty()) {
      auto& parent_node_data = NodeDataAt(parent_stack_.back());
      size_t child_index = parent_node_data.children.size();
      parent_node_data.children.push_back(node);
      node_data.parent_relation = GetChildKind(parent_node_data, child_index);
      node_data.child_index = child_index;
    }
    parent_stack_.push_back(index);
  }

  void PostVisitExpr(const Expr* expr,
                     const SourcePosition* position) override {
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
  absl::AnyInvocable<std::unique_ptr<AstNode>()> node_factory_;
  absl::AnyInvocable<NavigableAstNodeData&(AstNode&)> node_data_accessor_;
  std::unique_ptr<NavigableAstMetadata> metadata_;
  std::vector<size_t> parent_stack_;
};

}  // namespace

NavigableProtoAst NavigableProtoAst::Build(const Expr& expr) {
  NavigableExprBuilderVisitor visitor(
      []() { return absl::WrapUnique(new AstNode()); },
      [](AstNode& node) -> NavigableAstNodeData& { return node.data_; });
  AstTraverse(&expr, /*source_info=*/nullptr, &visitor);
  return NavigableProtoAst(std::move(visitor).Consume());
}

}  // namespace cel
