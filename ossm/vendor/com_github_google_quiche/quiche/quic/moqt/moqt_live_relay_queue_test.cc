// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_live_relay_queue.h"

#include <cstdint>
#include <optional>

#include "absl/strings/string_view.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_object.h"
#include "quiche/quic/moqt/moqt_priority.h"
#include "quiche/quic/moqt/moqt_publisher.h"
#include "quiche/quic/moqt/moqt_subscribe_windows.h"
#include "quiche/common/platform/api/quiche_logging.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/web_transport/web_transport.h"

namespace moqt::test {

namespace {

class TestMoqtLiveRelayQueue : public MoqtLiveRelayQueue,
                               public MoqtObjectListener {
 public:
  TestMoqtLiveRelayQueue()
      : MoqtLiveRelayQueue(
            FullTrackName{"test", "track"}, MoqtForwardingPreference::kSubgroup,
            MoqtDeliveryOrder::kAscending, quic::QuicTime::Infinite()) {
    AddObjectListener(this);
  }

  void OnNewObjectAvailable(Location sequence, uint64_t subgroup_id,
                            MoqtPriority /*publisher_priority*/) {
    std::optional<PublishedObject> object =
        GetCachedObject(sequence.group, subgroup_id, sequence.object);
    QUICHE_CHECK(object.has_value());
    if (!object.has_value()) {
      return;
    }
    switch (object->metadata.status) {
      case MoqtObjectStatus::kNormal:
        PublishObject(object->metadata.location.group,
                      object->metadata.location.object,
                      object->payload.AsStringView());
        break;
      case MoqtObjectStatus::kObjectDoesNotExist:
        SkipObject(object->metadata.location.group,
                   object->metadata.location.object);
        break;
      case MoqtObjectStatus::kEndOfGroup:
        CloseStreamForGroup(object->metadata.location.group);
        break;
      case MoqtObjectStatus::kEndOfTrack:
        CloseTrack();
        break;
      default:
        EXPECT_TRUE(false);
    }
    if (object->fin_after_this) {
      CloseStreamForSubgroup(object->metadata.location.group,
                             object->metadata.subgroup);
    }
  }

  void GetObjectsFromPast(const SubscribeWindow& window) {
    ForAllObjects([&](const CachedObject& object) {
      if (window.InWindow(object.metadata.location)) {
        OnNewObjectAvailable(object.metadata.location, object.metadata.subgroup,
                             object.metadata.publisher_priority);
      }
    });
  }

  MOCK_METHOD(void, OnNewFinAvailable, (Location sequence, uint64_t subgroup));
  MOCK_METHOD(void, OnSubgroupAbandoned,
              (uint64_t group, uint64_t subgroup,
               webtransport::StreamErrorCode error_code));
  MOCK_METHOD(void, OnGroupAbandoned, (uint64_t group_id));
  MOCK_METHOD(void, CloseStreamForGroup, (uint64_t group_id), ());
  MOCK_METHOD(void, CloseStreamForSubgroup,
              (uint64_t group_id, uint64_t subgroup_id), ());
  MOCK_METHOD(void, PublishObject,
              (uint64_t group_id, uint64_t object_id,
               absl::string_view payload),
              ());
  MOCK_METHOD(void, SkipObject, (uint64_t group_id, uint64_t object_id), ());
  MOCK_METHOD(void, CloseTrack, (), ());
  MOCK_METHOD(void, OnTrackPublisherGone, (), (override));
  MOCK_METHOD(void, OnSubscribeAccepted, (), (override));
  MOCK_METHOD(void, OnSubscribeRejected, (MoqtSubscribeErrorReason reason),
              (override));
};

// Duplicates of MoqtOutgoingQueue test cases.
TEST(MoqtLiveRelayQueue, SingleGroup) {
  TestMoqtLiveRelayQueue queue;
  {
    testing::InSequence seq;
    EXPECT_CALL(queue, PublishObject(0, 0, "a"));
    EXPECT_CALL(queue, PublishObject(0, 1, "b"));
    EXPECT_CALL(queue, PublishObject(0, 2, "c"));
    EXPECT_CALL(queue, CloseStreamForGroup(0));
  }
  EXPECT_TRUE(queue.AddObject(Location{0, 0}, 0, "a"));
  EXPECT_TRUE(queue.AddObject(Location{0, 1}, 0, "b"));
  EXPECT_TRUE(queue.AddObject(Location{0, 2}, 0, "c"));
  EXPECT_TRUE(
      queue.AddObject(Location{0, 3}, 0, MoqtObjectStatus::kEndOfGroup));
}

TEST(MoqtLiveRelayQueue, SingleGroupPastSubscribeFromZero) {
  TestMoqtLiveRelayQueue queue;
  {
    testing::InSequence seq;
    EXPECT_CALL(queue, PublishObject(0, 0, "a"));
    EXPECT_CALL(queue, PublishObject(0, 1, "b"));
    EXPECT_CALL(queue, PublishObject(0, 2, "c"));

    EXPECT_CALL(queue, PublishObject(0, 0, "a"));
    EXPECT_CALL(queue, PublishObject(0, 1, "b"));
    EXPECT_CALL(queue, PublishObject(0, 2, "c"));
  }
  EXPECT_TRUE(queue.AddObject(Location{0, 0}, 0, "a"));
  EXPECT_TRUE(queue.AddObject(Location{0, 1}, 0, "b"));
  EXPECT_TRUE(queue.AddObject(Location{0, 2}, 0, "c"));
  queue.GetObjectsFromPast(SubscribeWindow());
}

TEST(MoqtLiveRelayQueue, SingleGroupPastSubscribeFromMidGroup) {
  TestMoqtLiveRelayQueue queue;
  {
    testing::InSequence seq;
    EXPECT_CALL(queue, PublishObject(0, 0, "a"));
    EXPECT_CALL(queue, PublishObject(0, 1, "b"));
    EXPECT_CALL(queue, PublishObject(0, 2, "c"));

    EXPECT_CALL(queue, PublishObject(0, 1, "b"));
    EXPECT_CALL(queue, PublishObject(0, 2, "c"));
  }
  EXPECT_TRUE(queue.AddObject(Location{0, 0}, 0, "a"));
  EXPECT_TRUE(queue.AddObject(Location{0, 1}, 0, "b"));
  EXPECT_TRUE(queue.AddObject(Location{0, 2}, 0, "c"));
  queue.GetObjectsFromPast(SubscribeWindow(Location(0, 1)));
}

TEST(MoqtLiveRelayQueue, TwoGroups) {
  TestMoqtLiveRelayQueue queue;
  {
    testing::InSequence seq;
    EXPECT_CALL(queue, PublishObject(0, 0, "a"));
    EXPECT_CALL(queue, PublishObject(0, 1, "b"));
    EXPECT_CALL(queue, PublishObject(0, 2, "c"));
    EXPECT_CALL(queue, CloseStreamForGroup(0));
    EXPECT_CALL(queue, PublishObject(1, 0, "d"));
    EXPECT_CALL(queue, PublishObject(1, 1, "e"));
    EXPECT_CALL(queue, PublishObject(1, 2, "f"));
  }
  EXPECT_TRUE(queue.AddObject(Location{0, 0}, 0, "a"));
  EXPECT_TRUE(queue.AddObject(Location{0, 1}, 0, "b"));
  EXPECT_TRUE(queue.AddObject(Location{0, 2}, 0, "c"));
  EXPECT_TRUE(
      queue.AddObject(Location{0, 3}, 0, MoqtObjectStatus::kEndOfGroup));
  EXPECT_TRUE(queue.AddObject(Location{1, 0}, 0, "d"));
  EXPECT_TRUE(queue.AddObject(Location{1, 1}, 0, "e"));
  EXPECT_TRUE(queue.AddObject(Location{1, 2}, 0, "f"));
}

TEST(MoqtLiveRelayQueue, TwoGroupsPastSubscribe) {
  TestMoqtLiveRelayQueue queue;
  {
    testing::InSequence seq;
    EXPECT_CALL(queue, PublishObject(0, 0, "a"));
    EXPECT_CALL(queue, PublishObject(0, 1, "b"));
    EXPECT_CALL(queue, PublishObject(0, 2, "c"));
    EXPECT_CALL(queue, CloseStreamForGroup(0));
    EXPECT_CALL(queue, PublishObject(1, 0, "d"));
    EXPECT_CALL(queue, PublishObject(1, 1, "e"));
    EXPECT_CALL(queue, PublishObject(1, 2, "f"));

    EXPECT_CALL(queue, PublishObject(0, 1, "b"));
    EXPECT_CALL(queue, PublishObject(0, 2, "c"));
    EXPECT_CALL(queue, CloseStreamForGroup(0));
    EXPECT_CALL(queue, PublishObject(1, 0, "d"));
    EXPECT_CALL(queue, PublishObject(1, 1, "e"));
    EXPECT_CALL(queue, PublishObject(1, 2, "f"));
  }
  EXPECT_TRUE(queue.AddObject(Location{0, 0}, 0, "a"));
  EXPECT_TRUE(queue.AddObject(Location{0, 1}, 0, "b"));
  EXPECT_TRUE(queue.AddObject(Location{0, 2}, 0, "c"));
  EXPECT_TRUE(
      queue.AddObject(Location{0, 3}, 0, MoqtObjectStatus::kEndOfGroup));
  EXPECT_TRUE(queue.AddObject(Location{1, 0}, 0, "d"));
  EXPECT_TRUE(queue.AddObject(Location{1, 1}, 0, "e"));
  EXPECT_TRUE(queue.AddObject(Location{1, 2}, 0, "f"));
  queue.GetObjectsFromPast(SubscribeWindow(Location(0, 1)));
}

TEST(MoqtLiveRelayQueue, FiveGroups) {
  TestMoqtLiveRelayQueue queue;
  ;
  {
    testing::InSequence seq;
    EXPECT_CALL(queue, PublishObject(0, 0, "a"));
    EXPECT_CALL(queue, PublishObject(0, 1, "b"));
    EXPECT_CALL(queue, CloseStreamForGroup(0));
    EXPECT_CALL(queue, PublishObject(1, 0, "c"));
    EXPECT_CALL(queue, PublishObject(1, 1, "d"));
    EXPECT_CALL(queue, CloseStreamForGroup(1));
    EXPECT_CALL(queue, PublishObject(2, 0, "e"));
    EXPECT_CALL(queue, PublishObject(2, 1, "f"));
    EXPECT_CALL(queue, CloseStreamForGroup(2));
    EXPECT_CALL(queue, OnGroupAbandoned(0));
    EXPECT_CALL(queue, PublishObject(3, 0, "g"));
    EXPECT_CALL(queue, PublishObject(3, 1, "h"));
    EXPECT_CALL(queue, CloseStreamForGroup(3));
    EXPECT_CALL(queue, OnGroupAbandoned(1));
    EXPECT_CALL(queue, PublishObject(4, 0, "i"));
    EXPECT_CALL(queue, PublishObject(4, 1, "j"));
  }
  EXPECT_TRUE(queue.AddObject(Location{0, 0}, 0, "a"));
  EXPECT_TRUE(queue.AddObject(Location{0, 1}, 0, "b"));
  EXPECT_TRUE(
      queue.AddObject(Location{0, 2}, 0, MoqtObjectStatus::kEndOfGroup));
  EXPECT_TRUE(queue.AddObject(Location{1, 0}, 0, "c"));
  EXPECT_TRUE(queue.AddObject(Location{1, 1}, 0, "d"));
  EXPECT_TRUE(
      queue.AddObject(Location{1, 2}, 0, MoqtObjectStatus::kEndOfGroup));
  EXPECT_TRUE(queue.AddObject(Location{2, 0}, 0, "e"));
  EXPECT_TRUE(queue.AddObject(Location{2, 1}, 0, "f"));
  EXPECT_TRUE(
      queue.AddObject(Location{2, 2}, 0, MoqtObjectStatus::kEndOfGroup));
  EXPECT_TRUE(queue.AddObject(Location{3, 0}, 0, "g"));
  EXPECT_TRUE(queue.AddObject(Location{3, 1}, 0, "h"));
  EXPECT_TRUE(
      queue.AddObject(Location{3, 2}, 0, MoqtObjectStatus::kEndOfGroup));
  EXPECT_TRUE(queue.AddObject(Location{4, 0}, 0, "i"));
  EXPECT_TRUE(queue.AddObject(Location{4, 1}, 0, "j"));
}

TEST(MoqtLiveRelayQueue, FiveGroupsPastSubscribe) {
  TestMoqtLiveRelayQueue queue;
  {
    testing::InSequence seq;
    EXPECT_CALL(queue, PublishObject(0, 0, "a"));
    EXPECT_CALL(queue, PublishObject(0, 1, "b"));
    EXPECT_CALL(queue, CloseStreamForGroup(0));
    EXPECT_CALL(queue, PublishObject(1, 0, "c"));
    EXPECT_CALL(queue, PublishObject(1, 1, "d"));
    EXPECT_CALL(queue, CloseStreamForGroup(1));
    EXPECT_CALL(queue, PublishObject(2, 0, "e"));
    EXPECT_CALL(queue, PublishObject(2, 1, "f"));
    EXPECT_CALL(queue, CloseStreamForGroup(2));
    EXPECT_CALL(queue, OnGroupAbandoned(0));
    EXPECT_CALL(queue, PublishObject(3, 0, "g"));
    EXPECT_CALL(queue, PublishObject(3, 1, "h"));
    EXPECT_CALL(queue, CloseStreamForGroup(3));
    EXPECT_CALL(queue, OnGroupAbandoned(1));
    EXPECT_CALL(queue, PublishObject(4, 0, "i"));
    EXPECT_CALL(queue, PublishObject(4, 1, "j"));

    // Past SUBSCRIBE would only get the three most recent groups.
    EXPECT_CALL(queue, PublishObject(2, 0, "e"));
    EXPECT_CALL(queue, PublishObject(2, 1, "f"));
    EXPECT_CALL(queue, CloseStreamForGroup(2));
    EXPECT_CALL(queue, PublishObject(3, 0, "g"));
    EXPECT_CALL(queue, PublishObject(3, 1, "h"));
    EXPECT_CALL(queue, CloseStreamForGroup(3));
    EXPECT_CALL(queue, PublishObject(4, 0, "i"));
    EXPECT_CALL(queue, PublishObject(4, 1, "j"));
  }
  EXPECT_TRUE(queue.AddObject(Location{0, 0}, 0, "a"));
  EXPECT_TRUE(queue.AddObject(Location{0, 1}, 0, "b"));
  EXPECT_TRUE(
      queue.AddObject(Location{0, 2}, 0, MoqtObjectStatus::kEndOfGroup));
  EXPECT_TRUE(queue.AddObject(Location{1, 0}, 0, "c"));
  EXPECT_TRUE(queue.AddObject(Location{1, 1}, 0, "d"));
  EXPECT_TRUE(
      queue.AddObject(Location{1, 2}, 0, MoqtObjectStatus::kEndOfGroup));
  EXPECT_TRUE(queue.AddObject(Location{2, 0}, 0, "e"));
  EXPECT_TRUE(queue.AddObject(Location{2, 1}, 0, "f"));
  EXPECT_TRUE(
      queue.AddObject(Location{2, 2}, 0, MoqtObjectStatus::kEndOfGroup));
  EXPECT_TRUE(queue.AddObject(Location{3, 0}, 0, "g"));
  EXPECT_TRUE(queue.AddObject(Location{3, 1}, 0, "h"));
  EXPECT_TRUE(
      queue.AddObject(Location{3, 2}, 0, MoqtObjectStatus::kEndOfGroup));
  EXPECT_TRUE(queue.AddObject(Location{4, 0}, 0, "i"));
  EXPECT_TRUE(queue.AddObject(Location{4, 1}, 0, "j"));
  queue.GetObjectsFromPast(SubscribeWindow());
}

TEST(MoqtLiveRelayQueue, FiveGroupsPastSubscribeFromMidGroup) {
  TestMoqtLiveRelayQueue queue;
  {
    testing::InSequence seq;
    EXPECT_CALL(queue, PublishObject(0, 0, "a"));
    EXPECT_CALL(queue, PublishObject(0, 1, "b"));
    EXPECT_CALL(queue, PublishObject(1, 0, "c"));
    EXPECT_CALL(queue, PublishObject(1, 1, "d"));
    EXPECT_CALL(queue, CloseStreamForGroup(1));
    EXPECT_CALL(queue, PublishObject(2, 0, "e"));
    EXPECT_CALL(queue, PublishObject(2, 1, "f"));
    EXPECT_CALL(queue, CloseStreamForGroup(2));
    EXPECT_CALL(queue, OnGroupAbandoned(0));
    EXPECT_CALL(queue, PublishObject(3, 0, "g"));
    EXPECT_CALL(queue, PublishObject(3, 1, "h"));
    EXPECT_CALL(queue, CloseStreamForGroup(3));
    EXPECT_CALL(queue, OnGroupAbandoned(1));
    EXPECT_CALL(queue, PublishObject(4, 0, "i"));
    EXPECT_CALL(queue, PublishObject(4, 1, "j"));
  }
  EXPECT_TRUE(queue.AddObject(Location{0, 0}, 0, "a"));
  EXPECT_TRUE(queue.AddObject(Location{0, 1}, 0, "b"));
  EXPECT_TRUE(queue.AddObject(Location{1, 0}, 0, "c"));
  EXPECT_TRUE(queue.AddObject(Location{1, 1}, 0, "d"));
  EXPECT_TRUE(
      queue.AddObject(Location{1, 2}, 0, MoqtObjectStatus::kEndOfGroup));
  EXPECT_TRUE(queue.AddObject(Location{2, 0}, 0, "e"));
  EXPECT_TRUE(queue.AddObject(Location{2, 1}, 0, "f"));
  EXPECT_TRUE(
      queue.AddObject(Location{2, 2}, 0, MoqtObjectStatus::kEndOfGroup));
  EXPECT_TRUE(queue.AddObject(Location{3, 0}, 0, "g"));
  EXPECT_TRUE(queue.AddObject(Location{3, 1}, 0, "h"));
  EXPECT_TRUE(
      queue.AddObject(Location{3, 2}, 0, MoqtObjectStatus::kEndOfGroup));
  EXPECT_TRUE(queue.AddObject(Location{4, 0}, 0, "i"));
  EXPECT_TRUE(queue.AddObject(Location{4, 1}, 0, "j"));
  // This object will be ignored, but this is not an error.
  EXPECT_TRUE(
      queue.AddObject(Location{0, 2}, 0, MoqtObjectStatus::kEndOfGroup));
}

TEST(MoqtLiveRelayQueue, EndOfTrack) {
  TestMoqtLiveRelayQueue queue;
  {
    testing::InSequence seq;
    EXPECT_CALL(queue, PublishObject(0, 0, "a"));
    EXPECT_CALL(queue, PublishObject(0, 2, "c"));
    EXPECT_CALL(queue, CloseTrack());
  }
  EXPECT_TRUE(queue.AddObject(Location{0, 0}, 0, "a"));
  EXPECT_TRUE(queue.AddObject(Location{0, 2}, 0, "c"));
  EXPECT_FALSE(
      queue.AddObject(Location{0, 1}, 0, MoqtObjectStatus::kEndOfTrack));
  EXPECT_TRUE(
      queue.AddObject(Location{1, 0}, 0, MoqtObjectStatus::kEndOfTrack));
}

TEST(MoqtLiveRelayQueue, EndOfGroup) {
  TestMoqtLiveRelayQueue queue;
  {
    testing::InSequence seq;
    EXPECT_CALL(queue, PublishObject(0, 0, "a"));
    EXPECT_CALL(queue, PublishObject(0, 2, "c"));
    EXPECT_CALL(queue, CloseStreamForGroup(0));
  }
  EXPECT_TRUE(queue.AddObject(Location{0, 0}, 0, "a"));
  EXPECT_TRUE(queue.AddObject(Location{0, 2}, 0, "c"));
  EXPECT_FALSE(
      queue.AddObject(Location{0, 1}, 0, MoqtObjectStatus::kEndOfGroup));
  EXPECT_TRUE(
      queue.AddObject(Location{0, 3}, 0, MoqtObjectStatus::kEndOfGroup));
  EXPECT_FALSE(queue.AddObject(Location{0, 4}, 0, "e"));
}

TEST(MoqtLiveRelayQueue, OverwriteObject) {
  TestMoqtLiveRelayQueue queue;
  {
    testing::InSequence seq;
    EXPECT_CALL(queue, PublishObject(0, 0, "a"));
    EXPECT_CALL(queue, PublishObject(0, 1, "b"));
    EXPECT_CALL(queue, PublishObject(0, 2, "c"));
  }
  EXPECT_TRUE(queue.AddObject(Location{0, 0}, 0, "a"));
  EXPECT_TRUE(queue.AddObject(Location{0, 1}, 0, "b"));
  EXPECT_TRUE(queue.AddObject(Location{0, 2}, 0, "c"));
  EXPECT_TRUE(
      queue.AddObject(Location{0, 3}, 0, MoqtObjectStatus::kEndOfGroup));
  EXPECT_FALSE(queue.AddObject(Location{0, 1}, 0, "invalid"));
}

TEST(MoqtLiveRelayQueue, DifferentSubgroups) {
  TestMoqtLiveRelayQueue queue;
  {
    testing::InSequence seq;
    EXPECT_CALL(queue, PublishObject(0, 0, "a"));
    EXPECT_CALL(queue, PublishObject(0, 1, "b"));
    EXPECT_CALL(queue, PublishObject(0, 3, "d"));
    EXPECT_CALL(queue, PublishObject(0, 2, "c"));
    EXPECT_CALL(queue, OnNewFinAvailable(Location{0, 3}, 0));
    EXPECT_CALL(queue, PublishObject(0, 5, "e"));
    EXPECT_CALL(queue, PublishObject(0, 7, "f"));
    EXPECT_CALL(queue, OnNewFinAvailable(Location{0, 5}, 1));
    EXPECT_CALL(queue, OnNewFinAvailable(Location{0, 7}, 2));

    // Serve them back in strict subgroup order.
    EXPECT_CALL(queue, PublishObject(0, 0, "a"));
    EXPECT_CALL(queue, PublishObject(0, 3, "d"));
    EXPECT_CALL(queue, CloseStreamForSubgroup(0, 0));
    EXPECT_CALL(queue, PublishObject(0, 1, "b"));
    EXPECT_CALL(queue, PublishObject(0, 5, "e"));
    EXPECT_CALL(queue, CloseStreamForSubgroup(0, 1));
    EXPECT_CALL(queue, PublishObject(0, 2, "c"));
    EXPECT_CALL(queue, PublishObject(0, 7, "f"));
    EXPECT_CALL(queue, CloseStreamForSubgroup(0, 2));
  }
  EXPECT_TRUE(queue.AddObject(Location{0, 0}, 0, "a"));
  EXPECT_TRUE(queue.AddObject(Location{0, 1}, 1, "b"));
  EXPECT_TRUE(queue.AddObject(Location{0, 3}, 0, "d"));
  EXPECT_TRUE(queue.AddObject(Location{0, 2}, 2, "c"));
  EXPECT_TRUE(queue.AddFin(Location{0, 3}, 0));
  EXPECT_TRUE(queue.AddObject(Location{0, 5}, 1, "e"));
  EXPECT_TRUE(queue.AddObject(Location{0, 7}, 2, "f"));
  EXPECT_TRUE(queue.AddFin(Location{0, 5}, 1));
  EXPECT_TRUE(queue.AddFin(Location{0, 7}, 2));
  queue.GetObjectsFromPast(SubscribeWindow());
}

TEST(MoqtLiveRelayQueue, EndOfSubgroup) {
  TestMoqtLiveRelayQueue queue;
  {
    testing::InSequence seq;
    EXPECT_CALL(queue, PublishObject(0, 0, "a"));
    EXPECT_CALL(queue, OnNewFinAvailable(Location{0, 0}, 0));
    EXPECT_CALL(queue, PublishObject(0, 2, "b")).Times(0);
  }
  EXPECT_TRUE(queue.AddObject(Location{0, 0}, 0, "a"));
  EXPECT_TRUE(queue.AddFin(Location{0, 0}, 0));
  EXPECT_FALSE(queue.AddObject(Location{0, 2}, 0, "b"));
}

TEST(MoqtLiveRelayQueue, AddObjectWithFin) {
  TestMoqtLiveRelayQueue queue;
  {
    testing::InSequence seq;
    EXPECT_CALL(queue, PublishObject(0, 0, "a"));
  }
  EXPECT_TRUE(queue.AddObject(Location{0, 0}, 0, "a", true));
  std::optional<PublishedObject> object = queue.GetCachedObject(0, 0, 0);
  ASSERT_TRUE(object.has_value());
  EXPECT_EQ(object->metadata.status, MoqtObjectStatus::kNormal);
  EXPECT_TRUE(object->fin_after_this);
}

TEST(MoqtLiveRelayQueue, LateFin) {
  TestMoqtLiveRelayQueue queue;
  {
    testing::InSequence seq;
    EXPECT_CALL(queue, PublishObject(0, 0, "a"));
  }
  EXPECT_TRUE(queue.AddObject(Location{0, 0}, 0, "a", false));
  EXPECT_CALL(queue, OnNewFinAvailable(Location{0, 0}, 0));
  EXPECT_TRUE(queue.AddFin(Location{0, 0}, 0));
  std::optional<PublishedObject> object = queue.GetCachedObject(0, 0, 0);
  ASSERT_TRUE(object.has_value());
  EXPECT_EQ(object->metadata.status, MoqtObjectStatus::kNormal);
  EXPECT_TRUE(object->fin_after_this);
}

TEST(MoqtLiveRelayQueue, StreamReset) {
  TestMoqtLiveRelayQueue queue;
  {
    testing::InSequence seq;
    EXPECT_CALL(queue, PublishObject(0, 0, "a"));
    EXPECT_CALL(queue, OnSubgroupAbandoned(0, 0, 0x1));
  }
  EXPECT_TRUE(queue.AddObject(Location{0, 0}, 0, "a"));
  EXPECT_TRUE(queue.OnStreamReset(0, 0, 0x1));
}

}  // namespace

}  // namespace moqt::test
