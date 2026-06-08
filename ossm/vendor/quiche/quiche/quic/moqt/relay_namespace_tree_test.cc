// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/relay_namespace_tree.h"

#include <memory>
#include <utility>

#include "quiche/quic/moqt/moqt_fetch_task.h"
#include "quiche/quic/moqt/moqt_names.h"
#include "quiche/quic/moqt/test_tools/mock_moqt_session.h"
#include "quiche/common/platform/api/quiche_expect_bug.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace moqt {
namespace test {

class TestRelayNamespaceTree : public RelayNamespaceTree {
 public:
  using RelayNamespaceTree::NumNamespaces;
};

using ::testing::_;

class RelayNamespaceTreeTest : public quiche::test::QuicheTest {
 public:
  RelayNamespaceTreeTest() : session_(std::make_unique<MockMoqtSession>()) {}

  TestRelayNamespaceTree tree_;
  TrackNamespace a_{"a"}, ab_{"a", "b"}, abc_{"a", "b", "c"};
  std::unique_ptr<MockMoqtSession> session_;
  int objects_available_ = 0;
  ObjectsAvailableCallback callback_ = [&]() { ++objects_available_; };

  void CheckNextSuffix(MoqtNamespaceTask* task, TrackNamespace& full,
                       bool add = true) {
    TrackNamespace suffix;
    TransactionType type;
    EXPECT_EQ(task->GetNextSuffix(suffix, type), GetNextResult::kSuccess);
    EXPECT_EQ(*task->prefix().AddSuffix(suffix), full);
    EXPECT_EQ(type, add ? TransactionType::kAdd : TransactionType::kDelete);
  }
};

TEST_F(RelayNamespaceTreeTest, AddGetRemovePublisher) {
  EXPECT_EQ(tree_.NumNamespaces(), 0u);
  EXPECT_EQ(tree_.GetValidPublisher(ab_), nullptr);
  tree_.AddPublisher(ab_, session_.get());
  EXPECT_EQ(tree_.NumNamespaces(), 3u);
  EXPECT_EQ(tree_.GetValidPublisher(a_), nullptr);
  EXPECT_EQ(tree_.GetValidPublisher(ab_), session_.get());
  EXPECT_EQ(tree_.GetValidPublisher(abc_), session_.get());
  tree_.RemovePublisher(ab_, session_.get());
  EXPECT_EQ(tree_.NumNamespaces(), 0u);
  EXPECT_EQ(tree_.GetValidPublisher(ab_), nullptr);
}

TEST_F(RelayNamespaceTreeTest, AddGetRemoveListener) {
  // Add a listener to a namespace that has no publishers.
  EXPECT_EQ(tree_.NumNamespaces(), 0u);
  std::unique_ptr<MoqtNamespaceTask> task =
      tree_.AddSubscriber(ab_, session_.get());
  task->SetObjectsAvailableCallback(std::move(callback_));
  EXPECT_EQ(tree_.NumNamespaces(), 3u);
  tree_.AddPublisher(a_, session_.get());
  EXPECT_EQ(objects_available_, 0);
  tree_.AddPublisher(ab_, session_.get());
  EXPECT_EQ(objects_available_, 1);
  CheckNextSuffix(task.get(), ab_);
  tree_.AddPublisher(abc_, session_.get());
  EXPECT_EQ(objects_available_, 2);
  CheckNextSuffix(task.get(), abc_);
  EXPECT_EQ(tree_.NumNamespaces(), 4u);

  // Second publisher creates no new notifications, and delays OnNamespaceDone.
  auto session2 = std::make_unique<MockMoqtSession>();
  tree_.AddPublisher(ab_, session2.get());
  EXPECT_EQ(objects_available_, 2);
  tree_.RemovePublisher(ab_, session_.get());
  EXPECT_EQ(objects_available_, 2);
  tree_.RemovePublisher(ab_, session2.get());
  EXPECT_EQ(objects_available_, 3);
  CheckNextSuffix(task.get(), ab_, /*add=*/false);

  // Removing the listener disables notifications.
  task.reset();
  tree_.AddPublisher(ab_, session2.get());
  EXPECT_EQ(objects_available_, 3);
}

TEST_F(RelayNamespaceTreeTest, SessionDestroyed) {
  std::unique_ptr<MoqtNamespaceTask> task =
      tree_.AddSubscriber(ab_, session_.get());
  task->SetObjectsAvailableCallback(std::move(callback_));
  tree_.AddPublisher(ab_, session_.get());
  EXPECT_EQ(objects_available_, 1);
  CheckNextSuffix(task.get(), ab_);
  EXPECT_NE(tree_.GetValidPublisher(ab_), nullptr);
  // First session dies. In real life, it would have destroyed the stream and
  // therefore the task, removing the entry. But verify that the WeakPtr works.
  session_.reset();
  EXPECT_EQ(tree_.GetValidPublisher(ab_), nullptr);
}

TEST_F(RelayNamespaceTreeTest, AddListenerToExistingPublisher) {
  tree_.AddPublisher(a_, session_.get());
  tree_.AddPublisher(ab_, session_.get());
  tree_.AddPublisher(abc_, session_.get());
  std::unique_ptr<MoqtNamespaceTask> task =
      tree_.AddSubscriber(ab_, session_.get());
  task->SetObjectsAvailableCallback(std::move(callback_));
  EXPECT_EQ(objects_available_, 2);
  CheckNextSuffix(task.get(), ab_);
  CheckNextSuffix(task.get(), abc_);
}

TEST_F(RelayNamespaceTreeTest, MaxSizeNamespace) {
  std::unique_ptr<MoqtNamespaceTask> task =
      tree_.AddSubscriber(a_, session_.get());
  task->SetObjectsAvailableCallback(std::move(callback_));
  TrackNamespace big_namespace{"a", "b", "c", "d", "e", "f", "g", "h",
                               "i", "j", "k", "l", "m", "n", "o", "p",
                               "q", "r", "s", "t", "u", "v", "w", "x",
                               "y", "z", "1", "2", "3", "4", "5", "6"};
  tree_.AddPublisher(big_namespace, session_.get());
  EXPECT_EQ(objects_available_, 1);
  TrackNamespace suffix;
  TransactionType type;
  CheckNextSuffix(task.get(), big_namespace);
  EXPECT_EQ(task->GetNextSuffix(suffix, type), GetNextResult::kPending);
}

// TODO(martinduke): Add tests for published tracks.

}  // namespace test
}  // namespace moqt
