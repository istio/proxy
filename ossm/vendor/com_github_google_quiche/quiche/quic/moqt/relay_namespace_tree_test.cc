// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/relay_namespace_tree.h"

#include <memory>

#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/test_tools/mock_moqt_session.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace moqt {
namespace test {

class RelayNamespaceTreeTest : public quiche::test::QuicheTest {
 public:
  RelayNamespaceTreeTest() : session_(std::make_unique<MockMoqtSession>()) {}

  RelayNamespaceTree tree_;
  TrackNamespace ns1_{"foo", "bar"}, ns2_{"foo"}, ns3_{"foo", "bar", "baz"};
  std::unique_ptr<MockMoqtSession> session_;
};

TEST_F(RelayNamespaceTreeTest, AddGetRemovePublisher) {
  EXPECT_EQ(tree_.GetValidPublisher(ns1_).GetIfAvailable(), nullptr);
  tree_.AddPublisher(ns1_, session_.get());
  EXPECT_EQ(tree_.GetValidPublisher(ns1_).GetIfAvailable(), session_.get());
  EXPECT_EQ(tree_.GetValidPublisher(ns2_).GetIfAvailable(), nullptr);
  EXPECT_EQ(tree_.GetValidPublisher(ns3_).GetIfAvailable(), nullptr);
  tree_.RemovePublisher(ns1_, session_.get());
  EXPECT_EQ(tree_.GetValidPublisher(ns1_).GetIfAvailable(), nullptr);
}

TEST_F(RelayNamespaceTreeTest, SessionDestroyed) {
  EXPECT_EQ(tree_.GetValidPublisher(ns1_).GetIfAvailable(), nullptr);
  tree_.AddPublisher(ns1_, session_.get());
  EXPECT_EQ(tree_.GetValidPublisher(ns1_).GetIfAvailable(), session_.get());
  session_.reset();
  EXPECT_EQ(tree_.GetValidPublisher(ns1_).GetIfAvailable(), nullptr);
}

}  // namespace test
}  // namespace moqt
