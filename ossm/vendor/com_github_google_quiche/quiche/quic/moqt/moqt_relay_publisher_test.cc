// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_relay_publisher.h"

#include <memory>
#include <optional>
#include <utility>

#include "absl/strings/string_view.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_publisher.h"
#include "quiche/quic/moqt/moqt_session_callbacks.h"
#include "quiche/quic/moqt/test_tools/mock_moqt_session.h"
#include "quiche/quic/moqt/test_tools/moqt_mock_visitor.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_weak_ptr.h"

namespace moqt {
namespace test {

class MoqtRelayPublisherTest : public quiche::test::QuicheTest {
 public:
  MoqtSessionCallbacks callbacks_;
  MockMoqtSession session_;
  MoqtRelayPublisher publisher_;
  MockMoqtObjectListener object_listener_;
};

TEST_F(MoqtRelayPublisherTest, SetDefaultUpstreamSession) {
  EXPECT_FALSE(publisher_.GetDefaultUpstreamSession().IsValid());
  EXPECT_CALL(session_, callbacks).WillOnce(testing::ReturnRef(callbacks_));
  publisher_.SetDefaultUpstreamSession(&session_);
  EXPECT_TRUE(publisher_.GetDefaultUpstreamSession().IsValid());
  EXPECT_EQ(publisher_.GetDefaultUpstreamSession().GetIfAvailable(), &session_);
  // Destroy the session.
  std::move(callbacks_.session_terminated_callback)("test");
  EXPECT_FALSE(publisher_.GetDefaultUpstreamSession().IsValid());
}

TEST_F(MoqtRelayPublisherTest, SetDefaultUpstreamSessionTwice) {
  EXPECT_FALSE(publisher_.GetDefaultUpstreamSession().IsValid());
  EXPECT_CALL(session_, callbacks()).WillOnce(testing::ReturnRef(callbacks_));
  publisher_.SetDefaultUpstreamSession(&session_);
  EXPECT_TRUE(publisher_.GetDefaultUpstreamSession().IsValid());
  EXPECT_EQ(publisher_.GetDefaultUpstreamSession().GetIfAvailable(), &session_);

  MockMoqtSession session2;
  MoqtSessionCallbacks callbacks2;
  EXPECT_CALL(session_, callbacks).WillOnce(testing::ReturnRef(callbacks_));
  EXPECT_CALL(session2, callbacks).WillOnce(testing::ReturnRef(callbacks2));
  publisher_.SetDefaultUpstreamSession(&session2);
  EXPECT_TRUE(publisher_.GetDefaultUpstreamSession().IsValid());
  EXPECT_EQ(publisher_.GetDefaultUpstreamSession().GetIfAvailable(), &session2);

  // Destroying the old session doesn't affect the publisher.
  std::move(callbacks_.session_terminated_callback)("test");
  EXPECT_TRUE(publisher_.GetDefaultUpstreamSession().IsValid());

  // Destroying the new session does.
  std::move(callbacks2.session_terminated_callback)("test");
  EXPECT_FALSE(publisher_.GetDefaultUpstreamSession().IsValid());
}

TEST_F(MoqtRelayPublisherTest, GetTrackFromDefaultUpstream) {
  EXPECT_EQ(publisher_.GetTrack(FullTrackName("foo", "bar")), nullptr);
  EXPECT_CALL(session_, callbacks).WillOnce(testing::ReturnRef(callbacks_));
  publisher_.SetDefaultUpstreamSession(&session_);
  std::shared_ptr<MoqtTrackPublisher> track =
      publisher_.GetTrack(FullTrackName("foo", "bar"));
  EXPECT_NE(track, nullptr);
  EXPECT_EQ(track->GetTrackName(), FullTrackName("foo", "bar"));
}

TEST_F(MoqtRelayPublisherTest, PublishNamespaceLifecycle) {
  EXPECT_EQ(publisher_.GetTrack(FullTrackName("foo", "bar")), nullptr);
  std::optional<MoqtRequestError> response;
  publisher_.OnPublishNamespace(
      TrackNamespace({"foo"}), VersionSpecificParameters(), &session_,
      [&](std::optional<MoqtRequestError> error_response) {
        response = error_response;
      });
  EXPECT_EQ(response, std::nullopt);
  std::shared_ptr<MoqtTrackPublisher> track =
      publisher_.GetTrack(FullTrackName("foo", "bar"));
  EXPECT_NE(track, nullptr);
  EXPECT_CALL(session_, SubscribeCurrentObject);
  track->AddObjectListener(&object_listener_);
  track->RemoveObjectListener(&object_listener_);
  publisher_.OnPublishNamespaceDone(TrackNamespace({"foo"}), &session_);
  EXPECT_EQ(publisher_.GetTrack(FullTrackName("foo", "bar")), nullptr);
}

}  // namespace test
}  // namespace moqt
