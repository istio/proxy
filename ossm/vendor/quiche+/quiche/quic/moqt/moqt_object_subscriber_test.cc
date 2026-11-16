// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_object_subscriber.h"

#include <memory>
#include <optional>
#include <utility>

#include "absl/status/status.h"
#include "quiche/quic/core/quic_alarm.h"
#include "quiche/quic/moqt/moqt_error.h"
#include "quiche/quic/moqt/moqt_fetch_task.h"
#include "quiche/quic/moqt/moqt_key_value_pair.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_names.h"
#include "quiche/quic/moqt/moqt_object.h"
#include "quiche/quic/moqt/moqt_session_callbacks.h"
#include "quiche/quic/moqt/moqt_types.h"
#include "quiche/quic/moqt/test_tools/mock_moqt_session.h"
#include "quiche/quic/moqt/test_tools/moqt_mock_visitor.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/mock_clock.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/quiche_mem_slice.h"
#include "quiche/common/test_tools/quiche_test_utils.h"
#include "quiche/web_transport/test_tools/mock_web_transport.h"

namespace moqt {

namespace test {

namespace {

using ::testing::_;

class AlarmDelegate : public quic::QuicAlarm::DelegateWithoutContext {
 public:
  AlarmDelegate(bool* fired) : fired_(fired) {}
  void OnAlarm() override { *fired_ = true; }
  bool* fired_;
};

}  // namespace

class LiveSubscriberPeer {
 public:
  static MoqtFetchTask* GetFetchTask(LiveSubscriber* track) {
    return track->fetch_task_.get();
  }
  static quic::QuicAlarm* GetPublishDoneAlarm(LiveSubscriber* track) {
    return track->publish_done_alarm_.get();
  }
};

class LiveSubscriberTest : public quic::test::QuicTest {
 public:
  LiveSubscriberTest() : track_(subscribe_, &visitor_, &stream_) {
    stream_.BindStream(&wt_stream_);
  }

  MockLiveSubscriberVisitor visitor_;
  MoqtSubscribe subscribe_ = {
      /*request_id=*/1, FullTrackName("foo", "bar"),
      MessageParameters(LocationFilter::AbsoluteStartFilter(Location(2, 0)))};
  MockBidiStream stream_;
  webtransport::test::MockStream wt_stream_;
  LiveSubscriber track_;
  quic::MockClock clock_;
  quic::test::MockAlarmFactory alarm_factory_;
};

TEST_F(LiveSubscriberTest, Queries) {
  EXPECT_EQ(track_.full_track_name(), FullTrackName("foo", "bar"));
  EXPECT_EQ(track_.request_id(), 1);
  EXPECT_FALSE(track_.track_alias().has_value());
  EXPECT_EQ(track_.visitor(), &visitor_);
  EXPECT_FALSE(track_.is_fetch());
  track_.set_track_alias(1);
  EXPECT_EQ(track_.track_alias(), 1);
}

TEST_F(LiveSubscriberTest, AllowError) {
  EXPECT_TRUE(track_.ErrorIsAllowed());
  track_.OnObjectOrOk();
  EXPECT_FALSE(track_.ErrorIsAllowed());
}

TEST_F(LiveSubscriberTest, Windows) {
  EXPECT_TRUE(track_.InWindow(Location(2, 0)));
  EXPECT_FALSE(track_.InWindow(Location(1, 25)));
}

TEST_F(LiveSubscriberTest, UpdateLargestObjectValidation) {
  MessageParameters update1;
  update1.largest_object = Location(2, 5);
  QUICHE_EXPECT_OK(track_.Update(update1));
  EXPECT_EQ(track_.const_parameters().largest_object, Location(2, 5));

  // Increasing LARGEST_OBJECT succeeds.
  MessageParameters update2;
  update2.largest_object = Location(3, 0);
  QUICHE_EXPECT_OK(track_.Update(update2));
  EXPECT_EQ(track_.const_parameters().largest_object, Location(3, 0));

  // Omitting LARGEST_OBJECT once set is allowed and preserves the previous
  // value.
  MessageParameters omitted;
  omitted.set_forward(false);
  QUICHE_EXPECT_OK(track_.Update(omitted));
  EXPECT_EQ(track_.const_parameters().largest_object, Location(3, 0));

  // Decreasing LARGEST_OBJECT is rejected.
  MessageParameters decreased;
  decreased.largest_object = Location(2, 9);
  EXPECT_FALSE(track_.Update(decreased).ok());
  EXPECT_EQ(track_.const_parameters().largest_object, Location(3, 0));
}

TEST_F(LiveSubscriberTest, OnObjectOrOkAnchorsFilterWithoutLargestObject) {
  MoqtSubscribe subscribe = {
      /*request_id=*/2, FullTrackName("foo", "bar"),
      MessageParameters(LocationFilter::NextObjectFilter())};
  LiveSubscriber subscriber(subscribe, &visitor_, &stream_);
  EXPECT_FALSE(subscriber.const_parameters().location_filter->WindowKnown());

  EXPECT_CALL(visitor_, OnReply);
  subscriber.OnObjectOrOk(SubscribeOkData{});
  EXPECT_TRUE(subscriber.const_parameters().location_filter->WindowKnown());
  EXPECT_EQ(subscriber.const_parameters().location_filter->start(),
            Location(0, 0));
  EXPECT_CALL(visitor_, OnPublishDone).Times(testing::AtLeast(1));
}

TEST_F(LiveSubscriberTest, OnPublishDoneReadyToClose) {
  track_.OnStreamOpened(nullptr);
  track_.OnStreamClosed(absl::OkStatus(), std::nullopt);
  EXPECT_CALL(visitor_, OnPublishDone);
  ExpectFin(wt_stream_);
  track_.OnPublishDone(1, &clock_, &alarm_factory_);
}

TEST_F(LiveSubscriberTest, OnPublishDoneAllStreamsCloseLater) {
  track_.OnStreamOpened(nullptr);
  EXPECT_CALL(visitor_, OnPublishDone).Times(0);
  EXPECT_CALL(wt_stream_, Writev).Times(0);
  track_.OnPublishDone(2, &clock_, &alarm_factory_);
  track_.OnStreamClosed(absl::OkStatus(), std::nullopt);
  track_.OnStreamOpened(nullptr);
  ExpectFin(wt_stream_);
  EXPECT_CALL(visitor_, OnPublishDone);
  track_.OnStreamClosed(absl::OkStatus(), std::nullopt);
}

TEST_F(LiveSubscriberTest, OnPublishDoneTimesOut) {
  track_.OnStreamOpened(nullptr);
  EXPECT_CALL(visitor_, OnPublishDone).Times(0);
  EXPECT_CALL(wt_stream_, Writev).Times(0);
  track_.OnPublishDone(2, &clock_, &alarm_factory_);
  track_.OnStreamClosed(absl::OkStatus(), std::nullopt);
  // No streams are open; timer set.
  quic::QuicAlarm* alarm = LiveSubscriberPeer::GetPublishDoneAlarm(&track_);
  EXPECT_NE(alarm, nullptr);
  EXPECT_TRUE(alarm->IsSet());
  EXPECT_CALL(visitor_, OnPublishDone);
  EXPECT_CALL(wt_stream_, ResetWithUserCode(kResetCodeCancelled));
  alarm_factory_.FireAlarm(alarm);
}

TEST_F(LiveSubscriberTest, JoiningFetchMultiObject) {
  auto fetch_task = std::make_unique<MockFetchTask>();
  MockFetchTask* task_ptr = fetch_task.get();
  track_.OnJoiningFetchReady(std::move(fetch_task));

  PublishedObject o1, o2;
  o1.metadata.location = Location(2, 0);
  o1.metadata.payload_length = 3;
  o1.payload.push_back(quiche::QuicheMemSlice::Copy("abc"));

  o2.metadata.location = Location(2, 1);
  o2.metadata.payload_length = 3;
  o2.payload.push_back(quiche::QuicheMemSlice::Copy("def"));

  EXPECT_CALL(visitor_,
              OnObjectFragment(track_.full_track_name(), _, "abc", 0));
  EXPECT_CALL(visitor_,
              OnObjectFragment(track_.full_track_name(), _, "def", 0));
  EXPECT_CALL(*task_ptr, GetNextObject)
      .WillOnce([&](PublishedObject& output) {
        output = std::move(o1);
        return MoqtFetchTask::GetNextObjectResult::kSuccess;
      })
      .WillOnce([&](PublishedObject& output) {
        output = std::move(o2);
        return MoqtFetchTask::GetNextObjectResult::kSuccess;
      })
      .WillOnce(testing::Return(MoqtFetchTask::GetNextObjectResult::kPending));
  task_ptr->CallObjectsAvailableCallback();
  EXPECT_NE(LiveSubscriberPeer::GetFetchTask(&track_), nullptr);
  EXPECT_CALL(*task_ptr, GetNextObject)
      .WillOnce(testing::Return(MoqtFetchTask::GetNextObjectResult::kEof));
  task_ptr->CallObjectsAvailableCallback();
  EXPECT_EQ(LiveSubscriberPeer::GetFetchTask(&track_), nullptr);
}

TEST_F(LiveSubscriberTest, JoiningFetchFragmented) {
  auto fetch_task = std::make_unique<MockFetchTask>();
  MockFetchTask* task_ptr = fetch_task.get();
  track_.OnJoiningFetchReady(std::move(fetch_task));

  PublishedObject part1, part2;
  part1.metadata.location = Location(2, 0);
  part1.metadata.payload_length = 6;
  part1.payload.push_back(quiche::QuicheMemSlice::Copy("abc"));

  part2.metadata.location = Location(2, 0);
  part2.metadata.payload_length = 6;
  part2.payload.push_back(quiche::QuicheMemSlice::Copy("def"));

  EXPECT_CALL(visitor_,
              OnObjectFragment(track_.full_track_name(), _, "abc", 0));
  EXPECT_CALL(visitor_,
              OnObjectFragment(track_.full_track_name(), _, "def", 3));
  EXPECT_CALL(*task_ptr, GetNextObject)
      .WillOnce([&](PublishedObject& output) {
        output = std::move(part1);
        return MoqtFetchTask::GetNextObjectResult::kSuccess;
      })
      .WillOnce([&](PublishedObject& output) {
        output = std::move(part2);
        return MoqtFetchTask::GetNextObjectResult::kSuccess;
      })
      .WillOnce(testing::Return(MoqtFetchTask::GetNextObjectResult::kPending));
  task_ptr->CallObjectsAvailableCallback();
}

TEST_F(LiveSubscriberTest, JoiningFetchEmptyPayload) {
  auto fetch_task = std::make_unique<MockFetchTask>();
  MockFetchTask* task_ptr = fetch_task.get();
  track_.OnJoiningFetchReady(std::move(fetch_task));

  PublishedObject o1;
  o1.metadata.location = Location(2, 0);
  o1.metadata.payload_length = 0;
  o1.metadata.status = MoqtObjectStatus::kEndOfGroup;

  // Since object.payload is empty, is called once.
  EXPECT_CALL(visitor_,
              OnObjectFragment(track_.full_track_name(), o1.metadata, "", 0));
  EXPECT_CALL(*task_ptr, GetNextObject)
      .WillOnce([&](PublishedObject& output) {
        output = std::move(o1);
        return MoqtFetchTask::GetNextObjectResult::kSuccess;
      })
      .WillOnce(testing::Return(MoqtFetchTask::GetNextObjectResult::kPending));
  task_ptr->CallObjectsAvailableCallback();
}

TEST_F(LiveSubscriberTest, JoiningFetchError) {
  auto fetch_task = std::make_unique<MockFetchTask>();
  MockFetchTask* task_ptr = fetch_task.get();
  track_.OnJoiningFetchReady(std::move(fetch_task));

  EXPECT_NE(LiveSubscriberPeer::GetFetchTask(&track_), nullptr);
  EXPECT_CALL(*task_ptr, GetNextObject)
      .WillOnce(testing::Return(MoqtFetchTask::GetNextObjectResult::kError));
  task_ptr->CallObjectsAvailableCallback();
  EXPECT_EQ(LiveSubscriberPeer::GetFetchTask(&track_), nullptr);
}

class UpstreamFetchTaskTest : public quiche::test::QuicheTest {
 protected:
  UpstreamFetchTaskTest() {
    EXPECT_CALL(task_destroyed_callback_, Call).Times(testing::AnyNumber());
    EXPECT_CALL(can_read_callback_, Call).Times(testing::AnyNumber());
    task_.set_task_destroyed_callback(task_destroyed_callback_.AsStdFunction());
    task_.set_can_read_callback(can_read_callback_.AsStdFunction());
  }

  const Location kEndLocation = Location(3, 50);
  testing::StrictMock<testing::MockFunction<void()>> task_destroyed_callback_;
  testing::StrictMock<testing::MockFunction<void()>> can_read_callback_;
  UpstreamFetchTask task_;
};

TEST_F(UpstreamFetchTaskTest, ObjectRetrievalMultiSlice) {
  int can_read_calls = 0;
  task_.set_can_read_callback([&]() { ++can_read_calls; });
  EXPECT_EQ(can_read_calls, 1);

  PublishedObject output;
  EXPECT_EQ(task_.GetNextObject(output),
            MoqtFetchTask::GetNextObjectResult::kPending);
  MoqtObject new_object = {
      /*track_alias=*/1,
      /*group_id=*/3,
      /*object_id=*/0,
      /*publisher_priority=*/128,
      /*properties=*/"",
      /*object_status=*/MoqtObjectStatus::kNormal,
      /*subgroup_id=*/1,
      /*first_object_in_subgroup=*/true,
      /*payload_length=*/6,
  };
  EXPECT_FALSE(task_.HasObject());
  EXPECT_FALSE(task_.NeedsMorePayload());

  task_.NewObject(new_object);
  EXPECT_TRUE(task_.HasObject());
  EXPECT_TRUE(task_.NeedsMorePayload());
  EXPECT_EQ(task_.payload_length(), 0);
  EXPECT_EQ(task_.payload_offset(), 0);

  task_.AppendPayloadToObject("foo");
  EXPECT_TRUE(task_.HasObject());
  EXPECT_TRUE(task_.NeedsMorePayload());
  EXPECT_EQ(task_.payload_length(), 3);

  task_.AppendPayloadToObject("bar");
  EXPECT_TRUE(task_.HasObject());
  EXPECT_FALSE(task_.NeedsMorePayload());
  EXPECT_EQ(task_.payload_length(), 6);

  bool object_available_called = false;
  task_.SetObjectAvailableCallback([&]() { object_available_called = true; });
  task_.NotifyNewObject();
  EXPECT_TRUE(object_available_called);

  EXPECT_EQ(task_.GetNextObject(output),
            MoqtFetchTask::GetNextObjectResult::kSuccess);
  EXPECT_EQ(output.metadata.location, Location(3, 0));
  EXPECT_EQ(output.metadata.subgroup, 1);
  EXPECT_EQ(output.metadata.status, MoqtObjectStatus::kNormal);
  EXPECT_EQ(output.metadata.publisher_priority, 128);
  EXPECT_EQ(output.metadata.payload_length, 6);
  EXPECT_FALSE(output.fin_after_this);
  ASSERT_EQ(output.payload.size(), 2);
  EXPECT_EQ(output.payload[0].AsStringView(), "foo");
  EXPECT_EQ(output.payload[1].AsStringView(), "bar");
  EXPECT_EQ(can_read_calls, 2);

  EXPECT_FALSE(task_.HasObject());
  EXPECT_FALSE(task_.NeedsMorePayload());
}

TEST_F(UpstreamFetchTaskTest, ObjectRetrievalEmptyPayload) {
  MoqtObject moqt_obj = {
      /*track_alias=*/1,
      /*group_id=*/3,
      /*object_id=*/0,
      /*publisher_priority=*/128,
      /*properties=*/"",
      /*object_status=*/MoqtObjectStatus::kEndOfGroup,
      /*subgroup_id=*/0,
      /*first_object_in_subgroup=*/true,
      /*payload_length=*/0,
  };
  task_.NewObject(moqt_obj);
  task_.NotifyNewObject();

  PublishedObject output;
  EXPECT_EQ(task_.GetNextObject(output),
            MoqtFetchTask::GetNextObjectResult::kSuccess);
  EXPECT_TRUE(output.payload.empty());
  EXPECT_EQ(output.metadata.status, MoqtObjectStatus::kEndOfGroup);
  EXPECT_EQ(output.metadata.location, Location(3, 0));
}

TEST_F(UpstreamFetchTaskTest, PartialPayloadPending) {
  MoqtObject moqt_obj = {
      /*track_alias=*/1,
      /*group_id=*/3,
      /*object_id=*/0,
      /*publisher_priority=*/128,
      /*properties=*/"",
      /*object_status=*/MoqtObjectStatus::kNormal,
      /*subgroup_id=*/0,
      /*first_object_in_subgroup=*/true,
      /*payload_length=*/10,
  };
  task_.NewObject(moqt_obj);

  PublishedObject output;
  EXPECT_EQ(task_.GetNextObject(output),
            MoqtFetchTask::GetNextObjectResult::kPending);
}

TEST_F(UpstreamFetchTaskTest, OnStreamAndFetchClosedFin) {
  task_.OnStreamAndFetchClosed(absl::OkStatus());

  PublishedObject out;
  EXPECT_EQ(task_.GetNextObject(out), MoqtFetchTask::GetNextObjectResult::kEof);
  EXPECT_EQ(task_.GetNextObject(out), MoqtFetchTask::GetNextObjectResult::kEof);
  EXPECT_TRUE(task_.GetStatus().ok());
}

TEST_F(UpstreamFetchTaskTest, OnStreamAndFetchClosedError) {
  task_.OnStreamAndFetchClosed(absl::InternalError("custom reason"));

  PublishedObject out;
  EXPECT_EQ(task_.GetNextObject(out),
            MoqtFetchTask::GetNextObjectResult::kError);
  EXPECT_FALSE(task_.GetStatus().ok());
}

TEST_F(UpstreamFetchTaskTest, DestroyedByApplication) {
  testing::StrictMock<testing::MockFunction<void()>> callback;
  EXPECT_CALL(callback, Call);
  auto dynamic_task = std::make_unique<UpstreamFetchTask>();
  dynamic_task->set_task_destroyed_callback(callback.AsStdFunction());
  dynamic_task.reset();
}

}  // namespace test

}  // namespace moqt
