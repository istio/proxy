// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_MOQT_SESSION_NAMESPACE_TREE_H_
#define QUICHE_QUIC_MOQT_SESSION_NAMESPACE_TREE_H_

#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "quiche/quic/moqt/moqt_messages.h"

namespace moqt {

// Publishers MUST respond with an error if a SUBSCRIBE_NAMESPACE arrives that
// in any way intersects with an existing SUBSCRIBE_NAMESPACE. This requires a
// fairly complex data structure where each part of the tuple is a node. If a
// node has no children, it indicates a complete namespace, and there can be no
// other complete namespaces as direct ancestors or descendants.
// For example, if a/b/c and a/b/d are in the tree, then a/b/e is allowed, but
// a/b and a/b/c/d would not be.
class SessionNamespaceTree {
 public:
  SessionNamespaceTree() = default;
  ~SessionNamespaceTree() = default;

  // Returns false if the namespace can't be added because it intersects with an
  // existing namespace.
  bool AddNamespace(const TrackNamespace& track_namespace) {
    if (root_.children.empty()) {
      AddToTree(track_namespace.tuple(), root_);
      return true;
    }
    return TraverseTree(track_namespace.tuple(), root_);
  }
  // Called when UNSUBSCRIBE_NAMESPACE is received.
  void RemoveNamespace(const TrackNamespace& track_namespace) {
    DeleteUniqueBranches(track_namespace.tuple(), root_);
  }

 private:
  struct Node {
    absl::flat_hash_map<std::string, struct Node> children;
  };
  // Recursively add new elements of the tuple to the tree. |start_index| is the
  // element of |tuple| that is added directly to |parent_node|.
  void AddToTree(absl::Span<const std::string> tuple, Node& parent_node) {
    if (tuple.empty()) {
      return;
    }
    auto [it, success] = parent_node.children.emplace(tuple[0], Node());
    AddToTree(tuple.subspan(1), it->second);
  }

  bool TraverseTree(absl::Span<const std::string> tuple, Node& node) {
    if (node.children.empty()) {
      // The new namespace would be a child of an existing namespace.
      return false;
    }
    if (tuple.empty()) {
      // The new namespace would be a parent of an existing namespace.
      return false;
    }
    auto it = node.children.find(tuple[0]);
    if (it == node.children.end()) {
      // The new namespace would be a cousin of an existing namespace. This is
      // allowed.
      AddToTree(tuple, node);
      return true;
    }
    return TraverseTree(tuple.subspan(1), it->second);
  }

  // This recursive function finds the deepest leaf node for this namespace. It
  // then keeps deleting towards the root until it finds a parent node with
  // multiple children.
  // Returns false if there are other children of parent_node, so that it's not
  // safe to keep deleting.
  bool DeleteUniqueBranches(absl::Span<const std::string> tuple,
                            Node& parent_node) {
    if (tuple.empty()) {
      // We've reached the end of the namespace, it's unique if there are no
      // children.
      return parent_node.children.empty();
    }
    if (parent_node.children.empty()) {
      // Ran out of leaves too early. The namespace is not present.
      return false;
    }
    auto it = parent_node.children.find(tuple[0]);
    if (it == parent_node.children.end()) {
      // The namespace was not present.
      return false;
    }
    // Go to the next leaf node.
    if (!DeleteUniqueBranches(tuple.subspan(1), it->second)) {
      // Do no more deletion.
      return false;
    }
    parent_node.children.erase(it);
    // If there other children at this level, stop deleting.
    return parent_node.children.empty();
  }

  Node root_;  // Not a legal namespace. It's the root of the tree.
};

}  // namespace moqt

#endif  // QUICHE_QUIC_MOQT_SESSION_NAMESPACE_TREE_H_
