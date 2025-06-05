// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_track.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "quiche/quic/core/quic_alarm.h"
#include "quiche/quic/core/quic_default_clock.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_publisher.h"
#include "quiche/quic/moqt/tools/moqt_mock_visitor.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/platform/api/quiche_mem_slice.h"

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
      /*track_alias=*/2,
      /*full_track_name=*/FullTrackName("foo", "bar"),
      /*subscriber_priority=*/128,
      /*group_order=*/std::nullopt,
      /*start=*/FullSequence(2, 0),
      std::nullopt,
      MoqtSubscribeParameters(),
  };
  SubscribeRemoteTrack track_;
};

TEST_F(SubscribeRemoteTrackTest, Queries) {
  EXPECT_EQ(track_.full_track_name(), FullTrackName("foo", "bar"));
  EXPECT_EQ(track_.subscribe_id(), 1);
  EXPECT_EQ(track_.track_alias(), 2);
  EXPECT_EQ(track_.visitor(), &visitor_);
  EXPECT_FALSE(track_.is_fetch());
}

TEST_F(SubscribeRemoteTrackTest, UpdateDataStreamType) {
  EXPECT_TRUE(
      track_.CheckDataStreamType(MoqtDataStreamType::kStreamHeaderSubgroup));
  EXPECT_TRUE(
      track_.CheckDataStreamType(MoqtDataStreamType::kStreamHeaderSubgroup));
}

TEST_F(SubscribeRemoteTrackTest, AllowError) {
  EXPECT_TRUE(track_.ErrorIsAllowed());
  EXPECT_EQ(track_.GetSubscribe().subscribe_id, subscribe_.subscribe_id);
  track_.OnObjectOrOk();
  EXPECT_FALSE(track_.ErrorIsAllowed());
}

TEST_F(SubscribeRemoteTrackTest, Windows) {
  EXPECT_TRUE(track_.InWindow(FullSequence(2, 0)));
  track_.TruncateStart(FullSequence(2, 1));
  EXPECT_FALSE(track_.InWindow(FullSequence(2, 0)));
  track_.TruncateEnd(2);
  EXPECT_FALSE(track_.InWindow(FullSequence(3, 0)));
}

class UpstreamFetchTest : public quic::test::QuicTest {
 protected:
  UpstreamFetchTest()
      : fetch_(fetch_message_, [&](std::unique_ptr<MoqtFetchTask> task) {
          fetch_task_ = std::move(task);
        }) {}

  MoqtFetch fetch_message_ = {
      /*fetch_id=*/1,
      /*subscriber_priority=*/128,
      /*group_order=*/std::nullopt,
      /*joining_fetch=*/std::nullopt,
      /*full_track_name=*/FullTrackName("foo", "bar"),
      /*start_object=*/FullSequence(1, 1),
      /*end_group=*/3,
      /*end_object=*/100,
      /*parameters=*/MoqtSubscribeParameters(),
  };
  // The pointer held by the application.
  UpstreamFetch fetch_;
  std::unique_ptr<MoqtFetchTask> fetch_task_;
};

TEST_F(UpstreamFetchTest, Queries) {
  EXPECT_EQ(fetch_.subscribe_id(), 1);
  EXPECT_EQ(fetch_.full_track_name(), FullTrackName("foo", "bar"));
  EXPECT_FALSE(
      fetch_.CheckDataStreamType(MoqtDataStreamType::kStreamHeaderSubgroup));
  EXPECT_TRUE(
      fetch_.CheckDataStreamType(MoqtDataStreamType::kStreamHeaderFetch));
  EXPECT_TRUE(fetch_.is_fetch());
  EXPECT_FALSE(fetch_.InWindow(FullSequence{1, 0}));
  EXPECT_TRUE(fetch_.InWindow(FullSequence{1, 1}));
  EXPECT_TRUE(fetch_.InWindow(FullSequence{3, 100}));
  EXPECT_FALSE(fetch_.InWindow(FullSequence{3, 101}));
}

TEST_F(UpstreamFetchTest, AllowError) {
  EXPECT_TRUE(fetch_.ErrorIsAllowed());
  fetch_.OnObjectOrOk();
  EXPECT_FALSE(fetch_.ErrorIsAllowed());
}

TEST_F(UpstreamFetchTest, FetchResponse) {
  EXPECT_EQ(fetch_task_, nullptr);
  fetch_.OnFetchResult(FullSequence(3, 50), absl::OkStatus(), nullptr);
  EXPECT_NE(fetch_task_, nullptr);
  EXPECT_NE(fetch_.task(), nullptr);
  EXPECT_TRUE(fetch_task_->GetStatus().ok());
  EXPECT_EQ(fetch_task_->GetLargestId(), FullSequence(3, 50));
}

TEST_F(UpstreamFetchTest, FetchClosedByMoqt) {
  bool terminated = false;
  fetch_.OnFetchResult(FullSequence(3, 50), absl::OkStatus(),
                       [&]() { terminated = true; });
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
  fetch_.OnFetchResult(FullSequence(3, 50), absl::Status(),
                       [&]() { terminated = true; });
  fetch_task_.reset();
  EXPECT_TRUE(terminated);
}

TEST_F(UpstreamFetchTest, ObjectRetrieval) {
  fetch_.OnFetchResult(FullSequence(3, 50), absl::OkStatus(), nullptr);
  PublishedObject object;
  EXPECT_EQ(fetch_task_->GetNextObject(object),
            MoqtFetchTask::GetNextObjectResult::kPending);
  MoqtObject new_object = {1, 3, 0, 128, "", MoqtObjectStatus::kNormal, 0, 6};
  bool got_object = false;
  fetch_task_->SetObjectAvailableCallback([&]() {
    got_object = true;
    EXPECT_EQ(fetch_task_->GetNextObject(object),
              MoqtFetchTask::GetNextObjectResult::kSuccess);
    EXPECT_EQ(object.sequence, FullSequence(3, 0, 0));
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

}  // namespace test

}  // namespace moqt
