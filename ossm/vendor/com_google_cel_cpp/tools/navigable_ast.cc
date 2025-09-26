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

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cel/expr/checked.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "eval/public/ast_traverse.h"
#include "eval/public/ast_visitor.h"
#include "eval/public/ast_visitor_base.h"
#include "eval/public/source_position.h"

namespace cel {

namespace tools_internal {

AstNodeData& AstMetadata::NodeDataAt(size_t index) {
  ABSL_CHECK(index < nodes.size());
  return nodes[index]->data_;
}

size_t AstMetadata::AddNode() {
  size_t index = nodes.size();
  nodes.push_back(absl::WrapUnique(new AstNode()));
  return index;
}

}  // namespace tools_internal

namespace {

using cel::expr::Expr;
using google::api::expr::runtime::AstTraverse;
using google::api::expr::runtime::SourcePosition;

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
ChildKind GetChildKind(const tools_internal::AstNodeData& parent_node,
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
  NavigableExprBuilderVisitor()
      : metadata_(std::make_unique<tools_internal::AstMetadata>()) {}

  void PreVisitExpr(const Expr* expr, const SourcePosition* position) override {
    AstNode* parent = parent_stack_.empty()
                          ? nullptr
                          : metadata_->nodes[parent_stack_.back()].get();
    size_t index = metadata_->AddNode();
    tools_internal::AstNodeData& node_data = metadata_->NodeDataAt(index);
    node_data.parent = parent;
    node_data.expr = expr;
    node_data.parent_relation = ChildKind::kUnspecified;
    node_data.node_kind = GetNodeKind(*expr);
    node_data.weight = 1;
    node_data.index = index;
    node_data.metadata = metadata_.get();

    metadata_->id_to_node.insert({expr->id(), index});
    metadata_->expr_to_node.insert({expr, index});
    if (!parent_stack_.empty()) {
      auto& parent_node_data = metadata_->NodeDataAt(parent_stack_.back());
      size_t child_index = parent_node_data.children.size();
      parent_node_data.children.push_back(metadata_->nodes[index].get());
      node_data.parent_relation = GetChildKind(parent_node_data, child_index);
    }
    parent_stack_.push_back(index);
  }

  void PostVisitExpr(const Expr* expr,
                     const SourcePosition* position) override {
    size_t idx = parent_stack_.back();
    parent_stack_.pop_back();
    metadata_->postorder.push_back(metadata_->nodes[idx].get());
    tools_internal::AstNodeData& node = metadata_->NodeDataAt(idx);
    if (!parent_stack_.empty()) {
      tools_internal::AstNodeData& parent_node_data =
          metadata_->NodeDataAt(parent_stack_.back());
      parent_node_data.weight += node.weight;
    }
  }

  std::unique_ptr<tools_internal::AstMetadata> Consume() && {
    return std::move(metadata_);
  }

 private:
  std::unique_ptr<tools_internal::AstMetadata> metadata_;
  std::vector<size_t> parent_stack_;
};

}  // namespace

std::string ChildKindName(ChildKind kind) {
  switch (kind) {
    case ChildKind::kUnspecified:
      return "Unspecified";
    case ChildKind::kSelectOperand:
      return "SelectOperand";
    case ChildKind::kCallReceiver:
      return "CallReceiver";
    case ChildKind::kCallArg:
      return "CallArg";
    case ChildKind::kListElem:
      return "ListElem";
    case ChildKind::kMapKey:
      return "MapKey";
    case ChildKind::kMapValue:
      return "MapValue";
    case ChildKind::kStructValue:
      return "StructValue";
    case ChildKind::kComprehensionRange:
      return "ComprehensionRange";
    case ChildKind::kComprehensionInit:
      return "ComprehensionInit";
    case ChildKind::kComprehensionCondition:
      return "ComprehensionCondition";
    case ChildKind::kComprehensionLoopStep:
      return "ComprehensionLoopStep";
    case ChildKind::kComprensionResult:
      return "ComprehensionResult";
    default:
      return absl::StrCat("Unknown ChildKind ", static_cast<int>(kind));
  }
}

std::string NodeKindName(NodeKind kind) {
  switch (kind) {
    case NodeKind::kUnspecified:
      return "Unspecified";
    case NodeKind::kConstant:
      return "Constant";
    case NodeKind::kIdent:
      return "Ident";
    case NodeKind::kSelect:
      return "Select";
    case NodeKind::kCall:
      return "Call";
    case NodeKind::kList:
      return "List";
    case NodeKind::kMap:
      return "Map";
    case NodeKind::kStruct:
      return "Struct";
    case NodeKind::kComprehension:
      return "Comprehension";
    default:
      return absl::StrCat("Unknown NodeKind ", static_cast<int>(kind));
  }
}

int AstNode::child_index() const {
  if (data_.parent == nullptr) {
    return -1;
  }
  int i = 0;
  for (const AstNode* ptr : data_.parent->children()) {
    if (ptr->expr() == expr()) {
      return i;
    }
    i++;
  }
  return -1;
}

AstNode::PreorderRange AstNode::DescendantsPreorder() const {
  return AstNode::PreorderRange(absl::MakeConstSpan(data_.metadata->nodes)
                                    .subspan(data_.index, data_.weight));
}

AstNode::PostorderRange AstNode::DescendantsPostorder() const {
  return AstNode::PostorderRange(absl::MakeConstSpan(data_.metadata->postorder)
                                     .subspan(data_.index, data_.weight));
}

NavigableAst NavigableAst::Build(const Expr& expr) {
  NavigableExprBuilderVisitor visitor;
  AstTraverse(&expr, /*source_info=*/nullptr, &visitor);
  return NavigableAst(std::move(visitor).Consume());
}

}  // namespace cel
