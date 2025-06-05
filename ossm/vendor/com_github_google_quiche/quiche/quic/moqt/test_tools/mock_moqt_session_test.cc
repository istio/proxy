// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/test_tools/mock_moqt_session.h"

#include <memory>
#include <optional>
#include <utility>

#include "quiche/quic/moqt/moqt_known_track_publisher.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_outgoing_queue.h"
#include "quiche/quic/moqt/moqt_publisher.h"
#include "quiche/quic/moqt/tools/moqt_mock_visitor.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace moqt::test {
namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Optional;

FullTrackName TrackName() { return FullTrackName("foo", "bar"); }

class MockMoqtSessionTest : public quiche::test::QuicheTest {
 protected:
  MockMoqtSessionTest() : session_(&publisher_) {
    track_ = std::make_shared<MoqtOutgoingQueue>(
        TrackName(), MoqtForwardingPreference::kSubgroup);
    publisher_.Add(track_);
  }

  MoqtKnownTrackPublisher publisher_;
  std::shared_ptr<MoqtOutgoingQueue> track_;
  MockMoqtSession session_;
};

TEST_F(MockMoqtSessionTest, MissingTrack) {
  testing::StrictMock<MockSubscribeRemoteTrackVisitor> visitor;
  EXPECT_CALL(visitor,
              OnReply(FullTrackName("doesn't", "exist"), Eq(std::nullopt),
                      Optional(HasSubstr("not found"))));
  session_.SubscribeCurrentObject(FullTrackName("doesn't", "exist"), &visitor,
                                  MoqtSubscribeParameters());
}

TEST_F(MockMoqtSessionTest, SubscribeCurrentObject) {
  testing::StrictMock<MockSubscribeRemoteTrackVisitor> visitor;
  EXPECT_CALL(visitor,
              OnReply(TrackName(), Eq(std::nullopt), Eq(std::nullopt)));
  session_.SubscribeCurrentObject(TrackName(), &visitor,
                                  MoqtSubscribeParameters());
  EXPECT_CALL(visitor, OnObjectFragment(TrackName(), FullSequence(0, 0), _, _,
                                        "test", _));
  track_->AddObject(quic::test::MemSliceFromString("test"), /*key=*/true);

  session_.Unsubscribe(TrackName());
  track_->AddObject(quic::test::MemSliceFromString("test2"), /*key=*/true);
  // No visitor call here.
}

TEST_F(MockMoqtSessionTest, SubscribeAbsolute) {
  testing::StrictMock<MockSubscribeRemoteTrackVisitor> visitor;
  EXPECT_CALL(visitor,
              OnReply(TrackName(), Eq(std::nullopt), Eq(std::nullopt)));
  session_.SubscribeAbsolute(TrackName(), 1, 0, 1, &visitor,
                             MoqtSubscribeParameters());
  EXPECT_CALL(visitor, OnObjectFragment(TrackName(), FullSequence(1, 0), _,
                                        MoqtObjectStatus::kNormal, "b", _));
  EXPECT_CALL(visitor, OnObjectFragment(TrackName(), FullSequence(1, 1), _,
                                        MoqtObjectStatus::kEndOfGroup, "", _));
  track_->AddObject(quic::test::MemSliceFromString("a"), /*key=*/true);
  track_->AddObject(quic::test::MemSliceFromString("b"), /*key=*/true);
  track_->AddObject(quic::test::MemSliceFromString("c"), /*key=*/true);
}

TEST_F(MockMoqtSessionTest, Fetch) {
  track_->AddObject(quic::test::MemSliceFromString("a"), /*key=*/true);
  track_->AddObject(quic::test::MemSliceFromString("b"), /*key=*/false);
  track_->AddObject(quic::test::MemSliceFromString("c"), /*key=*/false);
  track_->AddObject(quic::test::MemSliceFromString("d"), /*key=*/true);
  std::unique_ptr<MoqtFetchTask> fetch;
  session_.Fetch(
      TrackName(),
      [&](std::unique_ptr<MoqtFetchTask> new_fetch) {
        fetch = std::move(new_fetch);
      },
      FullSequence(0, 1), 0, 2, 0x80, std::nullopt, MoqtSubscribeParameters());
  PublishedObject object;
  ASSERT_EQ(fetch->GetNextObject(object), MoqtFetchTask::kSuccess);
  EXPECT_EQ(object.payload.AsStringView(), "b");
  ASSERT_EQ(fetch->GetNextObject(object), MoqtFetchTask::kSuccess);
  EXPECT_EQ(object.payload.AsStringView(), "c");
  ASSERT_EQ(fetch->GetNextObject(object), MoqtFetchTask::kEof);
}

TEST_F(MockMoqtSessionTest, JoiningFetch) {
  track_->AddObject(quic::test::MemSliceFromString("a"), /*key=*/true);
  track_->AddObject(quic::test::MemSliceFromString("b"), /*key=*/true);
  track_->AddObject(quic::test::MemSliceFromString("c"), /*key=*/true);
  track_->AddObject(quic::test::MemSliceFromString("d"), /*key=*/true);

  testing::StrictMock<MockSubscribeRemoteTrackVisitor> visitor;
  EXPECT_CALL(visitor,
              OnReply(TrackName(), Eq(FullSequence(3, 0)), Eq(std::nullopt)));
  EXPECT_CALL(visitor, OnObjectFragment(TrackName(), FullSequence(2, 0), _,
                                        MoqtObjectStatus::kNormal, "c", _));
  EXPECT_CALL(visitor, OnObjectFragment(TrackName(), FullSequence(2, 1), _,
                                        MoqtObjectStatus::kEndOfGroup, "", _));
  EXPECT_CALL(visitor, OnObjectFragment(TrackName(), FullSequence(3, 0), _,
                                        MoqtObjectStatus::kNormal, "d", _));
  session_.JoiningFetch(TrackName(), &visitor, 2, MoqtSubscribeParameters());
  EXPECT_CALL(visitor, OnObjectFragment(TrackName(), FullSequence(3, 1), _,
                                        MoqtObjectStatus::kEndOfGroup, "", _));
  EXPECT_CALL(visitor, OnObjectFragment(TrackName(), FullSequence(4, 0), _,
                                        MoqtObjectStatus::kNormal, "e", _));
  track_->AddObject(quic::test::MemSliceFromString("e"), /*key=*/true);
}

TEST_F(MockMoqtSessionTest, JoiningFetchNoObjects) {
  testing::StrictMock<MockSubscribeRemoteTrackVisitor> visitor;
  EXPECT_CALL(visitor,
              OnReply(TrackName(), Eq(std::nullopt), Eq(std::nullopt)));
  session_.JoiningFetch(TrackName(), &visitor, 0, MoqtSubscribeParameters());
  EXPECT_CALL(visitor, OnObjectFragment(TrackName(), FullSequence(0, 0), _, _,
                                        "test", _));
  track_->AddObject(quic::test::MemSliceFromString("test"), /*key=*/true);
}

}  // namespace
}  // namespace moqt::test
