// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/session_namespace_tree.h"

#include <cstdint>

#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/test_tools/mock_moqt_session.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace moqt {
namespace test {

class TestSessionNamespaceTree : public SessionNamespaceTree {
 public:
  TestSessionNamespaceTree() = default;
  using SessionNamespaceTree::NumSubscriptions;
};

class SessionNamespaceTreeTest : public quic::test::QuicTest {
 public:
  void TestAddSucceeds(TrackNamespace track_namespace) {
    uint64_t num_subscriptions_before = tree_.NumSubscriptions();
    EXPECT_TRUE(tree_.SubscribeNamespace(track_namespace));
    EXPECT_EQ(tree_.NumSubscriptions(), num_subscriptions_before + 1);
  }

  void TestAddFails(TrackNamespace track_namespace) {
    uint64_t num_subscriptions_before = tree_.NumSubscriptions();
    EXPECT_FALSE(tree_.SubscribeNamespace(track_namespace));
    EXPECT_EQ(tree_.NumSubscriptions(), num_subscriptions_before);
  }

  void TestRemoveSucceeds(TrackNamespace track_namespace) {
    uint64_t num_subscriptions_before = tree_.NumSubscriptions();
    tree_.UnsubscribeNamespace(track_namespace);
    EXPECT_EQ(tree_.NumSubscriptions(), num_subscriptions_before - 1);
  }

  void TestRemoveFails(TrackNamespace track_namespace) {
    uint64_t num_subscriptions_before = tree_.NumSubscriptions();
    tree_.UnsubscribeNamespace(track_namespace);
    EXPECT_EQ(tree_.NumSubscriptions(), num_subscriptions_before);
  }

  MockMoqtSession session_;
  TestSessionNamespaceTree tree_;
  TrackNamespace ab_{"a", "b"}, abc_{"a", "b", "c"}, abcd_{"a", "b", "c", "d"};
};

TEST_F(SessionNamespaceTreeTest, AddNamespaces) {
  TestAddSucceeds(abc_);
  // No parents, children, or equivalents of what's already there.
  TestAddFails(ab_);
  TestAddFails(abc_);
  TestAddFails(abcd_);

  // Siblings are fine.
  TestAddSucceeds(TrackNamespace{"a", "b", "d"});

  // Totally different root is fine.
  TestAddSucceeds(TrackNamespace{"b", "c"});
}

TEST_F(SessionNamespaceTreeTest, RemoveNamespaces) {
  // Removing from an empty tree doesn't do anything.
  TestRemoveFails(abc_);

  // UnsubscribeNamespace doesn't do anything if the namespace isn't present.
  TestAddSucceeds(abc_);
  TestRemoveFails(ab_);
  TestRemoveFails(abcd_);

  // Exact match works. Now re-adding can succeed.
  TestRemoveSucceeds(abc_);
  TestAddSucceeds(abc_);

  // Add another ref count on ab_.
  TrackNamespace abd{"a", "b", "d"};
  TestAddSucceeds(abd);
  // Higher namespace is blocked.
  TestAddFails(ab_);
  // Removing one doesn't remove the block.
  TestRemoveSucceeds(abc_);
  TestAddFails(ab_);
  // Removing both allows add.
  TestRemoveSucceeds(abd);
  TestAddSucceeds(ab_);
}

}  // namespace test
}  // namespace moqt
