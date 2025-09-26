// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/session_namespace_tree.h"

#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace moqt {
namespace test {

TEST(SessionNamespaceTreeTest, AddNamespaces) {
  SessionNamespaceTree tree;
  EXPECT_TRUE(tree.AddNamespace(TrackNamespace({"a", "b", "c"})));
  // No parents, children, or equivalents of what's already there.
  EXPECT_FALSE(tree.AddNamespace(TrackNamespace({"a", "b", "c"})));
  EXPECT_FALSE(tree.AddNamespace(TrackNamespace({"a", "b", "c", "d"})));
  EXPECT_FALSE(tree.AddNamespace(TrackNamespace({"a", "b"})));

  // Siblings are fine.
  EXPECT_TRUE(tree.AddNamespace(TrackNamespace({"a", "b", "d"})));
  // Totally different root is fine.
  EXPECT_TRUE(tree.AddNamespace(TrackNamespace({"b", "c"})));
  EXPECT_FALSE(tree.AddNamespace(TrackNamespace({"b"})));
  EXPECT_FALSE(tree.AddNamespace(TrackNamespace({"b", "c", "e"})));
}

TEST(NamespaceTreeTest, RemoveNamespaces) {
  SessionNamespaceTree tree;
  // Removing from an empty tree doesn't do anything.
  tree.RemoveNamespace(TrackNamespace({"a", "b", "c"}));
  // RemoveNamespace doesn't do anything if the namespace isn't present.
  EXPECT_TRUE(tree.AddNamespace(TrackNamespace({"a", "b", "c"})));
  EXPECT_FALSE(tree.AddNamespace(TrackNamespace({"a", "b", "c"})));

  tree.RemoveNamespace(TrackNamespace({"a", "b", "c"}));
  EXPECT_TRUE(tree.AddNamespace(TrackNamespace({"a", "b", "c"})));
  tree.RemoveNamespace(TrackNamespace({"a", "b"}));
  // Inexact match didn't delete anything.
  EXPECT_FALSE(tree.AddNamespace(TrackNamespace({"a", "b", "c"})));
  tree.RemoveNamespace(TrackNamespace({"a", "b", "c", "d"}));
  // Inexact match didn't delete anything..
  EXPECT_FALSE(tree.AddNamespace(TrackNamespace({"a", "b", "c"})));
}

}  // namespace test
}  // namespace moqt
