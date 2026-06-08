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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_NAVIGABLE_AST_H_
#define THIRD_PARTY_CEL_CPP_COMMON_NAVIGABLE_AST_H_

#include "common/ast/navigable_ast_internal.h"
#include "common/ast/navigable_ast_kinds.h"  // IWYU pragma: export
#include "common/expr.h"

namespace cel {

class NavigableAst;
class NavigableAstNode;

namespace common_internal {

struct NativeAstTraits {
  using ExprType = Expr;
  using AstType = NavigableAst;
  using NodeType = NavigableAstNode;
};

}  // namespace common_internal

// Wrapper around a CEL AST node that exposes traversal information.
class NavigableAstNode : public common_internal::NavigableAstNodeBase<
                             common_internal::NativeAstTraits> {
 private:
  using Base =
      common_internal::NavigableAstNodeBase<common_internal::NativeAstTraits>;

 public:
  // A const Span like type that provides pre-order traversal for a sub tree.
  // provides .begin() and .end() returning bidirectional iterators to
  // const AstNode&.
  using PreorderRange = Base::PreorderRange;

  // A const Span like type that provides post-order traversal for a sub tree.
  // provides .begin() and .end() returning bidirectional iterators to
  // const AstNode&.
  using PostorderRange = Base::PostorderRange;

  // The parent of this node or nullptr if it is a root.
  using Base::parent;

  // The ptr to the backing Expr in the source AST.
  //
  // This may dangle if the source AST is mutated or destroyed.
  using Base::expr;

  // The index of this node in the parent's children. -1 if this is a root.
  using Base::child_index;

  // The type of traversal from parent to this node.
  using Base::parent_relation;

  // The type of this node, analogous to Expr::ExprKindCase.
  using Base::node_kind;

  // The number of nodes in the tree rooted at this node (including self).
  using Base::tree_size;

  // The height of this node in the tree (the number of descendants including
  // self on the longest path).
  using Base::height;

  // The children of this node in their natural order.
  using Base::children;

  // Range over the descendants of this node (including self) using preorder
  // semantics. Each node is visited immediately before all of its descendants.
  //
  // example:
  //  for (const cel::NavigableAstNode& node :
  //  ast.Root().DescendantsPreorder()) {
  //    ...
  //  }
  //
  // Children are traversed in their natural order:
  //   - call arguments are traversed in order (receiver if present is first)
  //   - list elements are traversed in order
  //   - maps are traversed in order (alternating key, value per entry)
  //   - comprehensions are traversed in the order: range, accu_init, condition,
  //   step, result
  using Base::DescendantsPreorder;

  // Range over the descendants of this node (including self) using postorder
  // semantics. Each node is visited immediately after all of its descendants.
  using Base::DescendantsPostorder;

 private:
  friend class NavigableAst;

  NavigableAstNode() = default;
};

// NavigableExpr provides a view over a CEL AST that allows for generalized
// traversal. The traversal structures are eagerly built on construction,
// requiring a full traversal of the AST. This is intended for use in tools that
// might require random access or multiple passes over the AST, amortizing the
// cost of building the traversal structures.
//
// Pointers to AstNodes are owned by this instance and must not outlive it.
//
// `NavigableAst` and Navigable nodes are independent of the input Expr and may
// outlive it, but may contain dangling pointers if the input Expr is modified
// or destroyed.
class NavigableAst : public common_internal::NavigableAstBase<
                         common_internal::NativeAstTraits> {
 private:
  using Base =
      common_internal::NavigableAstBase<common_internal::NativeAstTraits>;

 public:
  static NavigableAst Build(const Expr& expr);

  // Default constructor creates an empty instance.
  //
  // Operations other than equality are undefined on an empty instance.
  //
  // This is intended for composed object construction, a new NavigableAst
  // should be obtained from the Build factory function.
  NavigableAst() = default;

  // Move only.
  NavigableAst(const NavigableAst&) = delete;
  NavigableAst& operator=(const NavigableAst&) = delete;
  NavigableAst(NavigableAst&&) = default;
  NavigableAst& operator=(NavigableAst&&) = default;

  // Return ptr to the AST node with id if present. Otherwise returns nullptr.
  //
  // If ids are non-unique, the first pre-order node encountered with id is
  // returned.
  using Base::FindId;

  // Return ptr to the AST node representing the given Expr node.
  using Base::FindExpr;

  // Returns the root of the AST.
  using Base::Root;

  // Return whether the source AST used unique IDs for each node.
  //
  // This is typically the case, but older versions of the parsers didn't
  // guarantee uniqueness for nodes generated by some macros and ASTs modified
  // outside of CEL's parse/type check may not have unique IDs.
  using Base::IdsAreUnique;

 private:
  using Base::Base;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_NAVIGABLE_AST_H_
