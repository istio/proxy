// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_track.h"

#include <memory>
#include <optional>
#include <utility>

#include "absl/status/status.h"
#include "quiche/quic/core/quic_alarm.h"
#include "quiche/quic/moqt/moqt_fetch_task.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_object.h"
#include "quiche/quic/moqt/moqt_priority.h"
#include "quiche/quic/moqt/test_tools/moqt_mock_visitor.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/common/quiche_mem_slice.h"

namespace moqt {

namespace test {

namespace {

class AlarmDelegate : public quic::QuicAlarm::DelegateWithoutContext {
 public:
  AlarmDelegate(bool* fired) : fired_(fired) {}
  void OnAlarm() override { *fired_ = true; }
  bool* fired_;
};

}  // namespace

class SubscribeRemoteTrackPeer {
 public:
  static MoqtFetchTask* GetFetchTask(SubscribeRemoteTrack* track) {
    return track->fetch_task_.get();
  }
};

class SubscribeRemoteTrackTest : public quic::test::QuicTest {
 public:
  SubscribeRemoteTrackTest() : track_(subscribe_, &visitor_) {}

  MockSubscribeRemoteTrackVisitor visitor_;
  MoqtSubscribe subscribe_ = {
      /*subscribe_id=*/1,
      /*full_track_name=*/FullTrackName("foo", "bar"),
      /*subscriber_priority=*/128,
      /*group_order=*/std::nullopt,
      /*forward=*/true,
      /*filter_type=*/MoqtFilterType::kAbsoluteStart,
      /*start=*/Location(2, 0),
      std::nullopt,
      VersionSpecificParameters(),
  };
  SubscribeRemoteTrack track_;
};

TEST_F(SubscribeRemoteTrackTest, Queries) {
  EXPECT_EQ(track_.full_track_name(), FullTrackName("foo", "bar"));
  EXPECT_EQ(track_.request_id(), 1);
  EXPECT_FALSE(track_.track_alias().has_value());
  EXPECT_EQ(track_.visitor(), &visitor_);
  EXPECT_FALSE(track_.is_fetch());
  track_.set_track_alias(1);
  EXPECT_EQ(track_.track_alias(), 1);
}

TEST_F(SubscribeRemoteTrackTest, UpdateDataStreamType) {
  EXPECT_TRUE(
      track_.CheckDataStreamType(MoqtDataStreamType::Subgroup(1, 1, true)));
  EXPECT_FALSE(track_.CheckDataStreamType(MoqtDataStreamType::Fetch()));
}

TEST_F(SubscribeRemoteTrackTest, AllowError) {
  EXPECT_TRUE(track_.ErrorIsAllowed());
  track_.OnObjectOrOk();
  EXPECT_FALSE(track_.ErrorIsAllowed());
}

TEST_F(SubscribeRemoteTrackTest, Windows) {
  EXPECT_TRUE(track_.InWindow(Location(2, 0)));
  track_.TruncateStart(Location(2, 1));
  EXPECT_FALSE(track_.InWindow(Location(2, 0)));
  track_.TruncateEnd(2);
  EXPECT_FALSE(track_.InWindow(Location(3, 0)));
}

class UpstreamFetchTest : public quic::test::QuicTest {
 protected:
  UpstreamFetchTest()
      : fetch_(fetch_message_, std::get<StandaloneFetch>(fetch_message_.fetch),
               [&](std::unique_ptr<MoqtFetchTask> task) {
                 fetch_task_ = std::move(task);
               }) {}

  MoqtFetch fetch_message_ = {
      /*request_id=*/1,
      /*subscriber_priority=*/128,
      /*group_order=*/std::nullopt,
      /*fetch=*/
      StandaloneFetch(FullTrackName("foo", "bar"), Location(1, 1),
                      Location(3, 100)),
      VersionSpecificParameters(),
  };
  // The pointer held by the application.
  UpstreamFetch fetch_;
  std::unique_ptr<MoqtFetchTask> fetch_task_;
};

TEST_F(UpstreamFetchTest, Queries) {
  EXPECT_EQ(fetch_.request_id(), 1);
  EXPECT_EQ(fetch_.full_track_name(), FullTrackName("foo", "bar"));
  EXPECT_FALSE(
      fetch_.CheckDataStreamType(MoqtDataStreamType::Subgroup(1, 2, true)));
  EXPECT_TRUE(fetch_.CheckDataStreamType(MoqtDataStreamType::Fetch()));
  EXPECT_TRUE(fetch_.is_fetch());
  EXPECT_FALSE(fetch_.InWindow(Location{1, 0}));
  EXPECT_TRUE(fetch_.InWindow(Location{1, 1}));
  EXPECT_TRUE(fetch_.InWindow(Location{3, 100}));
  EXPECT_FALSE(fetch_.InWindow(Location{3, 101}));
}

TEST_F(UpstreamFetchTest, AllowError) {
  EXPECT_TRUE(fetch_.ErrorIsAllowed());
  fetch_.OnObjectOrOk();
  EXPECT_FALSE(fetch_.ErrorIsAllowed());
}

TEST_F(UpstreamFetchTest, FetchResponse) {
  EXPECT_EQ(fetch_task_, nullptr);
  fetch_.OnFetchResult(Location(3, 50), MoqtDeliveryOrder::kAscending,
                       absl::OkStatus(), nullptr);
  EXPECT_NE(fetch_task_, nullptr);
  EXPECT_NE(fetch_.task(), nullptr);
  EXPECT_TRUE(fetch_task_->GetStatus().ok());
}

TEST_F(UpstreamFetchTest, FetchClosedByMoqt) {
  bool terminated = false;
  fetch_.OnFetchResult(Location(3, 50), MoqtDeliveryOrder::kAscending,
                       absl::OkStatus(), [&]() { terminated = true; });
  bool got_eof = false;
  fetch_task_->SetObjectAvailableCallback([&]() {
    PublishedObject object;
    EXPECT_EQ(fetch_task_->GetNextObject(object),
              MoqtFetchTask::GetNextObjectResult::kEof);
    got_eof = true;
  });
  fetch_.task()->OnStreamAndFetchClosed(std::nullopt, "");
  EXPECT_FALSE(terminated);
  EXPECT_TRUE(got_eof);
}

TEST_F(UpstreamFetchTest, FetchClosedByApplication) {
  bool terminated = false;
  fetch_.OnFetchResult(Location(3, 50), MoqtDeliveryOrder::kAscending,
                       absl::Status(), [&]() { terminated = true; });
  fetch_task_.reset();
  EXPECT_TRUE(terminated);
}

TEST_F(UpstreamFetchTest, ObjectRetrieval) {
  fetch_.OnFetchResult(Location(3, 50), MoqtDeliveryOrder::kAscending,
                       absl::OkStatus(), nullptr);
  PublishedObject object;
  EXPECT_EQ(fetch_task_->GetNextObject(object),
            MoqtFetchTask::GetNextObjectResult::kPending);
  MoqtObject new_object = {1, 3, 0, 128, "", MoqtObjectStatus::kNormal, 0, 6};
  bool got_object = false;
  fetch_task_->SetObjectAvailableCallback([&]() {
    got_object = true;
    EXPECT_EQ(fetch_task_->GetNextObject(object),
              MoqtFetchTask::GetNextObjectResult::kSuccess);
    EXPECT_EQ(object.metadata.location, Location(3, 0));
    EXPECT_EQ(object.metadata.subgroup, 0);
    EXPECT_EQ(object.payload.AsStringView(), "foobar");
  });
  int got_read_callback = 0;
  fetch_.OnStreamOpened([&]() { ++got_read_callback; });
  EXPECT_FALSE(fetch_.task()->HasObject());
  EXPECT_FALSE(fetch_.task()->NeedsMorePayload());
  fetch_.task()->NewObject(new_object);
  EXPECT_TRUE(fetch_.task()->HasObject());
  EXPECT_TRUE(fetch_.task()->NeedsMorePayload());
  fetch_.task()->AppendPayloadToObject("foo");
  EXPECT_TRUE(fetch_.task()->HasObject());
  EXPECT_TRUE(fetch_.task()->NeedsMorePayload());
  fetch_.task()->AppendPayloadToObject("bar");
  EXPECT_TRUE(fetch_.task()->HasObject());
  EXPECT_FALSE(fetch_.task()->NeedsMorePayload());
  EXPECT_FALSE(got_object);
  EXPECT_EQ(got_read_callback, 1);  // Call from OnStreamOpened().
  fetch_.task()->NotifyNewObject();
  EXPECT_FALSE(fetch_.task()->HasObject());
  EXPECT_FALSE(fetch_.task()->NeedsMorePayload());
  EXPECT_EQ(got_read_callback, 2);  // Call from GetNextObjectResult().
  EXPECT_TRUE(got_object);
}

TEST_F(UpstreamFetchTest, LocationIsValidOkFirstObjectIdDeclining) {
  fetch_.OnFetchResult(Location(3, 50), MoqtDeliveryOrder::kAscending,
                       absl::OkStatus(), nullptr);
  EXPECT_TRUE(
      fetch_.LocationIsValid(Location(1, 1), MoqtObjectStatus::kNormal, true));
  EXPECT_TRUE(
      fetch_.LocationIsValid(Location(1, 2), MoqtObjectStatus::kNormal, true));
  EXPECT_FALSE(
      fetch_.LocationIsValid(Location(1, 0), MoqtObjectStatus::kNormal, true));
}

TEST_F(UpstreamFetchTest, LocationIsValidPartialObject) {
  fetch_.OnFetchResult(Location(3, 50), MoqtDeliveryOrder::kAscending,
                       absl::OkStatus(), nullptr);
  EXPECT_TRUE(
      fetch_.LocationIsValid(Location(1, 1), MoqtObjectStatus::kNormal, true));
  EXPECT_TRUE(
      fetch_.LocationIsValid(Location(1, 2), MoqtObjectStatus::kNormal, false));
  EXPECT_TRUE(
      fetch_.LocationIsValid(Location(1, 2), MoqtObjectStatus::kNormal, false));
}

TEST_F(UpstreamFetchTest, LocationIsValidOkGroupDescendingIncorrectly) {
  fetch_.OnFetchResult(Location(3, 50), MoqtDeliveryOrder::kAscending,
                       absl::OkStatus(), nullptr);
  EXPECT_TRUE(
      fetch_.LocationIsValid(Location(2, 1), MoqtObjectStatus::kNormal, true));
  EXPECT_TRUE(
      fetch_.LocationIsValid(Location(3, 1), MoqtObjectStatus::kNormal, true));
  EXPECT_FALSE(
      fetch_.LocationIsValid(Location(1, 1), MoqtObjectStatus::kNormal, true));
}

TEST_F(UpstreamFetchTest, LocationIsValidOkGroupAscendingIncorrectly) {
  fetch_.OnFetchResult(Location(3, 50), MoqtDeliveryOrder::kDescending,
                       absl::OkStatus(), nullptr);
  EXPECT_TRUE(
      fetch_.LocationIsValid(Location(2, 1), MoqtObjectStatus::kNormal, true));
  EXPECT_FALSE(
      fetch_.LocationIsValid(Location(3, 1), MoqtObjectStatus::kNormal, true));
}

TEST_F(UpstreamFetchTest, LocationIsValidLearnOrderThenOkSuccess) {
  EXPECT_TRUE(
      fetch_.LocationIsValid(Location(1, 1), MoqtObjectStatus::kNormal, true));
  EXPECT_TRUE(
      fetch_.LocationIsValid(Location(2, 1), MoqtObjectStatus::kNormal, true));
  fetch_.OnFetchResult(Location(3, 50), MoqtDeliveryOrder::kAscending,
                       absl::OkStatus(), nullptr);
  //  Groups arrived in ascending order, but the FETCH_OK reported descending.
  EXPECT_TRUE(fetch_task_->GetStatus().ok());
}

TEST_F(UpstreamFetchTest, LocationIsValidLearnOrderThenOkFailure) {
  EXPECT_TRUE(
      fetch_.LocationIsValid(Location(1, 1), MoqtObjectStatus::kNormal, true));
  EXPECT_TRUE(
      fetch_.LocationIsValid(Location(2, 1), MoqtObjectStatus::kNormal, true));
  bool termination_callback_called = false;
  fetch_.OnFetchResult(Location(3, 50), MoqtDeliveryOrder::kDescending,
                       absl::OkStatus(),
                       [&]() { termination_callback_called = true; });
  //  Groups arrived in ascending order, but the FETCH_OK reported descending.
  EXPECT_TRUE(termination_callback_called);
}

TEST_F(UpstreamFetchTest, LocationIsValidObjectBeyondEndOfGroup) {
  EXPECT_TRUE(fetch_.LocationIsValid(Location(1, 1),
                                     MoqtObjectStatus::kEndOfGroup, true));
  EXPECT_FALSE(
      fetch_.LocationIsValid(Location(1, 2), MoqtObjectStatus::kNormal, true));
}

TEST_F(UpstreamFetchTest, LocationIsValidObjectBeyondEndOfTrack) {
  EXPECT_TRUE(fetch_.LocationIsValid(Location(1, 1),
                                     MoqtObjectStatus::kEndOfTrack, true));
  EXPECT_FALSE(
      fetch_.LocationIsValid(Location(2, 1), MoqtObjectStatus::kNormal, true));
}

TEST_F(UpstreamFetchTest, LocationIsValidTwoEndsOfTrack) {
  EXPECT_TRUE(fetch_.LocationIsValid(Location(1, 1),
                                     MoqtObjectStatus::kEndOfTrack, true));
  EXPECT_FALSE(fetch_.LocationIsValid(Location(1, 2),
                                      MoqtObjectStatus::kEndOfTrack, true));
}

TEST_F(UpstreamFetchTest, LocationIsValidEndOfTrackTooLow) {
  EXPECT_TRUE(
      fetch_.LocationIsValid(Location(1, 2), MoqtObjectStatus::kNormal, true));
  EXPECT_TRUE(
      fetch_.LocationIsValid(Location(3, 0), MoqtObjectStatus::kNormal, true));
  EXPECT_FALSE(fetch_.LocationIsValid(Location(2, 1),
                                      MoqtObjectStatus::kEndOfTrack, true));
}

}  // namespace test

}  // namespace moqt
