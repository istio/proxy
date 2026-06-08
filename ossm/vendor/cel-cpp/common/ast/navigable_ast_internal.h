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
#ifndef THIRD_PARTY_CEL_CPP_COMMON_AST_NAVIGABLE_AST_INTERNAL_H_
#define THIRD_PARTY_CEL_CPP_COMMON_AST_NAVIGABLE_AST_INTERNAL_H_

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <type_traits>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/types/span.h"
#include "common/ast/navigable_ast_kinds.h"  // IWYU pragma: keep

namespace cel::common_internal {

// Implementation for range used for traversals backed by an absl::Span.
//
// This is intended to abstract the metadata layout from clients using the
// traversal methods in navigable_expr.h
//
// RangeTraits provide type info needed to construct the span and adapt to the
// range element type.
template <class RangeTraits>
class NavigableAstRange {
 private:
  using UnderlyingType = typename RangeTraits::UnderlyingType;
  using PtrType = const UnderlyingType*;
  using SpanType = absl::Span<const UnderlyingType>;

 public:
  class Iterator {
   public:
    using difference_type = ptrdiff_t;
    using value_type = decltype(RangeTraits::Adapt(*PtrType()));
    using iterator_category = std::bidirectional_iterator_tag;

    Iterator() : ptr_(nullptr), span_() {}
    Iterator(SpanType span, size_t i) : ptr_(span.data() + i), span_(span) {}

    value_type operator*() const {
      ABSL_DCHECK(ptr_ != nullptr);
      ABSL_DCHECK(span_.data() != nullptr);
      ABSL_DCHECK_GE(ptr_, span_.data());
      ABSL_DCHECK_LT(ptr_, span_.data() + span_.size());
      return RangeTraits::Adapt(*ptr_);
    }

    template <int... Barrier, typename T = value_type>
    std::enable_if_t<std::is_lvalue_reference<T>::value,
                     std::add_pointer_t<std::remove_reference_t<T>>>
    operator->() const {
      return &operator*();
    }

    Iterator& operator++() {
      ++ptr_;
      return *this;
    }

    Iterator operator++(int) {
      Iterator tmp = *this;
      ++ptr_;
      return tmp;
    }

    Iterator& operator--() {
      --ptr_;
      return *this;
    }

    Iterator operator--(int) {
      Iterator tmp = *this;
      --ptr_;
      return tmp;
    }

    bool operator==(const Iterator& other) const {
      return ptr_ == other.ptr_ && span_ == other.span_;
    }

    bool operator!=(const Iterator& other) const { return !(*this == other); }

   private:
    PtrType ptr_;
    SpanType span_;
  };

  explicit NavigableAstRange(SpanType span) : span_(span) {}

  Iterator begin() const { return Iterator(span_, 0); }
  Iterator end() const { return Iterator(span_, span_.size()); }

  explicit operator bool() const { return !span_.empty(); }

 private:
  SpanType span_;
};

template <typename AstTraits>
struct NavigableAstMetadata;

// Internal implementation for data-structures handling cross-referencing nodes.
//
// This is exposed separately to allow building up the AST relationships
// without exposing too much mutable state on the client facing classes.
template <typename AstTraits>
struct NavigableAstNodeData {
  typename AstTraits::NodeType* parent;
  const typename AstTraits::ExprType* expr;
  ChildKind parent_relation;
  NodeKind node_kind;
  const NavigableAstMetadata<AstTraits>* absl_nonnull metadata;
  size_t index;
  size_t tree_size;
  size_t height;
  int child_index;
  std::vector<typename AstTraits::NodeType* absl_nonnull> children;
};

template <typename AstTraits>
struct NavigableAstMetadata {
  // The nodes in the AST in preorder.
  //
  // unique_ptr is used to guarantee pointer stability in the other tables.
  std::vector<std::unique_ptr<typename AstTraits::NodeType>> nodes;
  std::vector<const typename AstTraits::NodeType* absl_nonnull> postorder;
  absl::flat_hash_map<int64_t, const typename AstTraits::NodeType* absl_nonnull>
      id_to_node;
  absl::flat_hash_map<const typename AstTraits::ExprType*,
                      const typename AstTraits::NodeType* absl_nonnull>
      expr_to_node;
};

template <typename AstNode>
struct PostorderTraits {
  using UnderlyingType = const AstNode*;

  static const AstNode& Adapt(const AstNode* const node) { return *node; }
};

template <typename AstNode>
struct PreorderTraits {
  using UnderlyingType = std::unique_ptr<AstNode>;
  static const AstNode& Adapt(const std::unique_ptr<AstNode>& node) {
    return *node;
  }
};

// Base class for NavigableAstNode and NavigableProtoAstNode.
template <typename AstTraits>
class NavigableAstNodeBase {
 private:
  using MetadataType = NavigableAstMetadata<AstTraits>;
  using NodeDataType = NavigableAstNodeData<AstTraits>;
  using Derived = typename AstTraits::NodeType;
  using ExprType = typename AstTraits::ExprType;

 public:
  using PreorderRange = NavigableAstRange<PreorderTraits<Derived>>;
  using PostorderRange = NavigableAstRange<PostorderTraits<Derived>>;

  // The parent of this node or nullptr if it is a root.
  const Derived* absl_nullable parent() const { return data_.parent; }

  const ExprType* absl_nonnull expr() const { return data_.expr; }

  // The index of this node in the parent's children. -1 if this is a root.
  int child_index() const { return data_.child_index; }

  // The type of traversal from parent to this node.
  ChildKind parent_relation() const { return data_.parent_relation; }

  // The type of this node, analogous to Expr::ExprKindCase.
  NodeKind node_kind() const { return data_.node_kind; }

  // The number of nodes in the tree rooted at this node (including self).
  size_t tree_size() const { return data_.tree_size; }

  // The height of this node in the tree (the number of descendants including
  // self on the longest path).
  size_t height() const { return data_.height; }

  absl::Span<const Derived* const> children() const {
    return absl::MakeConstSpan(data_.children);
  }

  // Range over the descendants of this node (including self) using preorder
  // semantics. Each node is visited immediately before all of its descendants.
  PreorderRange DescendantsPreorder() const {
    return PreorderRange(absl::MakeConstSpan(data_.metadata->nodes)
                             .subspan(data_.index, data_.tree_size));
  }

  // Range over the descendants of this node (including self) using postorder
  // semantics. Each node is visited immediately after all of its descendants.
  PostorderRange DescendantsPostorder() const {
    return PostorderRange(absl::MakeConstSpan(data_.metadata->postorder)
                              .subspan(data_.index, data_.tree_size));
  }

 private:
  friend Derived;

  NavigableAstNodeBase() = default;
  NavigableAstNodeBase(const NavigableAstNodeBase&) = delete;
  NavigableAstNodeBase& operator=(const NavigableAstNodeBase&) = delete;

 protected:
  NodeDataType data_;
};

// Shared implementation for NavigableAst and NavigableProtoAst.
//
// AstTraits provides type info for the derived classes that implement building
// the traversal metadata. It provides the following types:
//
// ExprType is the expression node type of the source AST.
//
// AstType is the subclass of NavigableAstBase for the implementation.
//
// NodeType is the subclass of NavigableAstNodeBase for the implementation.
template <class AstTraits>
class NavigableAstBase {
 private:
  using MetadataType = NavigableAstMetadata<AstTraits>;
  using Derived = typename AstTraits::AstType;
  using NodeType = typename AstTraits::NodeType;
  using ExprType = typename AstTraits::ExprType;

 public:
  NavigableAstBase(const NavigableAstBase&) = delete;
  NavigableAstBase& operator=(const NavigableAstBase&) = delete;
  NavigableAstBase(NavigableAstBase&&) = default;
  NavigableAstBase& operator=(NavigableAstBase&&) = default;

  // Return ptr to the AST node with id if present. Otherwise returns nullptr.
  //
  // If ids are non-unique, the first pre-order node encountered with id is
  // returned.
  const NodeType* absl_nullable FindId(int64_t id) const {
    auto it = metadata_->id_to_node.find(id);
    if (it == metadata_->id_to_node.end()) {
      return nullptr;
    }
    return it->second;
  }

  // Return ptr to the AST node representing the given Expr protobuf node.
  const NodeType* absl_nullable FindExpr(
      const ExprType* absl_nonnull expr) const {
    auto it = metadata_->expr_to_node.find(expr);
    if (it == metadata_->expr_to_node.end()) {
      return nullptr;
    }
    return it->second;
  }

  // The root of the AST.
  const NodeType& Root() const { return *metadata_->nodes[0]; }

  // Check whether the source AST used unique IDs for each node.
  //
  // This is typically the case, but older versions of the parsers didn't
  // guarantee uniqueness for nodes generated by some macros and ASTs modified
  // outside of CEL's parse/type check may not have unique IDs.
  bool IdsAreUnique() const {
    return metadata_->id_to_node.size() == metadata_->nodes.size();
  }

  // Equality operators test for identity. They are intended to distinguish
  // moved-from or uninitialized instances from initialized.
  bool operator==(const NavigableAstBase& other) const {
    return metadata_ == other.metadata_;
  }

  bool operator!=(const NavigableAstBase& other) const {
    return metadata_ != other.metadata_;
  }

  // Return true if this instance is initialized.
  explicit operator bool() const { return metadata_ != nullptr; }

 private:
  friend Derived;

  NavigableAstBase() = default;
  explicit NavigableAstBase(std::unique_ptr<MetadataType> metadata)
      : metadata_(std::move(metadata)) {}

  std::unique_ptr<MetadataType> metadata_;
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_AST_NAVIGABLE_AST_INTERNAL_H_
