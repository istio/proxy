// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_outgoing_queue.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/quic_default_clock.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/moqt/moqt_fetch_task.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_names.h"
#include "quiche/quic/moqt/moqt_object.h"
#include "quiche/quic/moqt/moqt_priority.h"
#include "quiche/quic/moqt/moqt_publisher.h"
#include "quiche/quic/moqt/moqt_subscribe_windows.h"
#include "quiche/quic/moqt/moqt_types.h"
#include "quiche/quic/moqt/test_tools/moqt_mock_visitor.h"
#include "quiche/common/platform/api/quiche_expect_bug.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_mem_slice.h"
#include "quiche/common/test_tools/quiche_test_utils.h"
#include "quiche/web_transport/web_transport.h"

namespace moqt::test {
namespace {

using ::quiche::test::IsOkAndHolds;
using ::quiche::test::StatusIs;
using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Return;

class TestMoqtOutgoingQueue : public MoqtOutgoingQueue,
                              public MoqtObjectListener {
 public:
  TestMoqtOutgoingQueue() : MoqtOutgoingQueue(FullTrackName{"test", "track"}) {
    EXPECT_CALL(*this, OnSubscribeAccepted).WillOnce(Return());
    AddObjectListener(this);
  }

  void OnNewObjectAvailable(Location sequence, std::optional<uint64_t> subgroup,
                            MoqtPriority publisher_priority) override {
    // MoqtOutgoingQueue does not create datagrams.
    ASSERT_THAT(subgroup, testing::Optional(0));
    std::optional<PublishedObject> object =
        GetCachedObject(sequence.group, subgroup, sequence.object);
    ASSERT_THAT(object,
                Optional(Field(&PublishedObject::metadata,
                               Field(&PublishedObjectMetadata::status,
                                     AnyOf(MoqtObjectStatus::kNormal,
                                           MoqtObjectStatus::kEndOfGroup,
                                           MoqtObjectStatus::kEndOfTrack)))));
    if (object->metadata.status == MoqtObjectStatus::kNormal) {
      PublishObject(object->metadata.location.group,
                    object->metadata.location.object,
                    object->payload.AsStringView());
    } else {
      CloseStreamForGroup(object->metadata.location.group);
    }
  }

  void GetObjectsFromPast(const SubscribeWindow& window) {
    if (!largest_location().has_value()) {
      return;
    }
    std::vector<Location> objects =
        GetCachedObjectsInRange(Location(0, 0), *largest_location());
    for (Location object : objects) {
      if (window.InWindow(object)) {
        OnNewObjectAvailable(object, 0, default_publisher_priority());
      }
    }
  }

  MOCK_METHOD(void, OnNewFinAvailable, (Location sequence, uint64_t subgroup));
  MOCK_METHOD(void, OnSubgroupAbandoned,
              (uint64_t group, uint64_t subgroup,
               webtransport::StreamErrorCode error_code));
  MOCK_METHOD(void, OnGroupAbandoned, (uint64_t group_id));
  MOCK_METHOD(void, CloseStreamForGroup, (uint64_t group_id), ());
  MOCK_METHOD(void, PublishObject,
              (uint64_t group_id, uint64_t object_id,
               absl::string_view payload),
              ());
  MOCK_METHOD(void, OnTrackPublisherGone, (), (override));
  MOCK_METHOD(void, OnSubscribeAccepted, (), (override));
  MOCK_METHOD(void, OnSubscribeRejected, (MoqtRequestErrorInfo reason),
              (override));
};

absl::StatusOr<std::vector<std::string>> FetchToVector(
    std::unique_ptr<MoqtFetchTask> fetch) {
  std::vector<std::string> objects;
  for (;;) {
    PublishedObject object;
    MoqtFetchTask::GetNextObjectResult result = fetch->GetNextObject(object);
    switch (result) {
      case MoqtFetchTask::kSuccess:
        if (object.metadata.status == MoqtObjectStatus::kNormal) {
          objects.emplace_back(object.payload.AsStringView());
        } else {
          EXPECT_EQ(object.metadata.status, MoqtObjectStatus::kEndOfGroup);
        }
        continue;
      case MoqtFetchTask::kPending:
        return absl::InternalError(
            "Unexpected kPending from MoqtOutgoingQueue");
      case MoqtFetchTask::kEof:
        return objects;
      case MoqtFetchTask::kError:
        return fetch->GetStatus();
    }
  }
}

TEST(MoqtOutgoingQueue, FirstObjectNotKeyframe) {
  TestMoqtOutgoingQueue queue;
  EXPECT_QUICHE_BUG(queue.AddObject(quiche::QuicheMemSlice::Copy("a"), false),
                    "The first object");
}

TEST(MoqtOutgoingQueue, SingleGroup) {
  TestMoqtOutgoingQueue queue;
  {
    testing::InSequence seq;
    EXPECT_CALL(queue, PublishObject(0, 0, "a"));
    EXPECT_CALL(queue, PublishObject(0, 1, "b"));
    EXPECT_CALL(queue, PublishObject(0, 2, "c"));
  }
  queue.AddObject(quiche::QuicheMemSlice::Copy("a"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("b"), false);
  queue.AddObject(quiche::QuicheMemSlice::Copy("c"), false);
}

TEST(MoqtOutgoingQueue, SingleGroupPastSubscribeFromZero) {
  TestMoqtOutgoingQueue queue;
  {
    testing::InSequence seq;
    EXPECT_CALL(queue, PublishObject(0, 0, "a"));
    EXPECT_CALL(queue, PublishObject(0, 1, "b"));
    EXPECT_CALL(queue, PublishObject(0, 2, "c"));

    EXPECT_CALL(queue, PublishObject(0, 0, "a"));
    EXPECT_CALL(queue, PublishObject(0, 1, "b"));
    EXPECT_CALL(queue, PublishObject(0, 2, "c"));
  }
  queue.AddObject(quiche::QuicheMemSlice::Copy("a"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("b"), false);
  queue.AddObject(quiche::QuicheMemSlice::Copy("c"), false);
  queue.GetObjectsFromPast(SubscribeWindow(Location(0, 0)));
}

TEST(MoqtOutgoingQueue, SingleGroupPastSubscribeFromMidGroup) {
  TestMoqtOutgoingQueue queue;
  {
    testing::InSequence seq;
    EXPECT_CALL(queue, PublishObject(0, 0, "a"));
    EXPECT_CALL(queue, PublishObject(0, 1, "b"));
    EXPECT_CALL(queue, PublishObject(0, 2, "c"));

    EXPECT_CALL(queue, PublishObject(0, 1, "b"));
    EXPECT_CALL(queue, PublishObject(0, 2, "c"));
  }
  queue.AddObject(quiche::QuicheMemSlice::Copy("a"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("b"), false);
  queue.AddObject(quiche::QuicheMemSlice::Copy("c"), false);
  queue.GetObjectsFromPast(SubscribeWindow(Location(0, 1)));
}

TEST(MoqtOutgoingQueue, TwoGroups) {
  TestMoqtOutgoingQueue queue;
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
  queue.AddObject(quiche::QuicheMemSlice::Copy("a"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("b"), false);
  queue.AddObject(quiche::QuicheMemSlice::Copy("c"), false);
  queue.AddObject(quiche::QuicheMemSlice::Copy("d"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("e"), false);
  queue.AddObject(quiche::QuicheMemSlice::Copy("f"), false);
}

TEST(MoqtOutgoingQueue, TwoGroupsPastSubscribe) {
  TestMoqtOutgoingQueue queue;
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
  queue.AddObject(quiche::QuicheMemSlice::Copy("a"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("b"), false);
  queue.AddObject(quiche::QuicheMemSlice::Copy("c"), false);
  queue.AddObject(quiche::QuicheMemSlice::Copy("d"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("e"), false);
  queue.AddObject(quiche::QuicheMemSlice::Copy("f"), false);
  queue.GetObjectsFromPast(SubscribeWindow(Location(0, 1)));
}

TEST(MoqtOutgoingQueue, FiveGroups) {
  TestMoqtOutgoingQueue queue;
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
    EXPECT_CALL(queue, PublishObject(3, 0, "g"));
    EXPECT_CALL(queue, PublishObject(3, 1, "h"));
    EXPECT_CALL(queue, CloseStreamForGroup(3));
    EXPECT_CALL(queue, PublishObject(4, 0, "i"));
    EXPECT_CALL(queue, PublishObject(4, 1, "j"));
  }
  queue.AddObject(quiche::QuicheMemSlice::Copy("a"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("b"), false);
  queue.AddObject(quiche::QuicheMemSlice::Copy("c"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("d"), false);
  queue.AddObject(quiche::QuicheMemSlice::Copy("e"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("f"), false);
  queue.AddObject(quiche::QuicheMemSlice::Copy("g"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("h"), false);
  queue.AddObject(quiche::QuicheMemSlice::Copy("i"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("j"), false);
}

TEST(MoqtOutgoingQueue, FiveGroupsPastSubscribe) {
  TestMoqtOutgoingQueue queue;
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
    EXPECT_CALL(queue, PublishObject(3, 0, "g"));
    EXPECT_CALL(queue, PublishObject(3, 1, "h"));
    EXPECT_CALL(queue, CloseStreamForGroup(3));
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
  queue.AddObject(quiche::QuicheMemSlice::Copy("a"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("b"), false);
  queue.AddObject(quiche::QuicheMemSlice::Copy("c"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("d"), false);
  queue.AddObject(quiche::QuicheMemSlice::Copy("e"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("f"), false);
  queue.AddObject(quiche::QuicheMemSlice::Copy("g"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("h"), false);
  queue.AddObject(quiche::QuicheMemSlice::Copy("i"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("j"), false);
  queue.GetObjectsFromPast(SubscribeWindow(Location(0, 0)));
}

TEST(MoqtOutgoingQueue, StandaloneFetch) {
  TestMoqtOutgoingQueue queue;
  EXPECT_THAT(
      FetchToVector(queue.StandaloneFetch(Location{0, 0}, Location{2, 0},
                                          MoqtDeliveryOrder::kAscending)),
      StatusIs(absl::StatusCode::kNotFound));

  queue.AddObject(quiche::QuicheMemSlice::Copy("a"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("b"), false);
  queue.AddObject(quiche::QuicheMemSlice::Copy("c"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("d"), false);
  queue.AddObject(quiche::QuicheMemSlice::Copy("e"), true);

  EXPECT_THAT(
      FetchToVector(queue.StandaloneFetch(Location{0, 0}, Location{2, 0},
                                          MoqtDeliveryOrder::kAscending)),
      IsOkAndHolds(ElementsAre("a", "b", "c", "d", "e")));
  EXPECT_THAT(
      FetchToVector(queue.StandaloneFetch(Location{0, 100}, Location{0, 1000},
                                          MoqtDeliveryOrder::kAscending)),
      IsOkAndHolds(IsEmpty()));
  EXPECT_THAT(
      FetchToVector(queue.StandaloneFetch(Location{0, 0}, Location{2, 0},
                                          MoqtDeliveryOrder::kDescending)),
      IsOkAndHolds(ElementsAre("e", "c", "d", "a", "b")));
  EXPECT_THAT(
      FetchToVector(queue.StandaloneFetch(Location{0, 0}, Location{1, 0},
                                          MoqtDeliveryOrder::kAscending)),
      IsOkAndHolds(ElementsAre("a", "b", "c")));
  EXPECT_THAT(
      FetchToVector(queue.StandaloneFetch(Location{0, 0}, Location{1, 0},
                                          MoqtDeliveryOrder::kAscending)),
      IsOkAndHolds(ElementsAre("a", "b", "c")));
  EXPECT_THAT(FetchToVector(queue.StandaloneFetch(
                  Location{1, 0}, Location{5, kMaxObjectId},
                  MoqtDeliveryOrder::kAscending)),
              IsOkAndHolds(ElementsAre("c", "d", "e")));
  EXPECT_THAT(FetchToVector(queue.StandaloneFetch(
                  Location{3, 0}, Location{5, kMaxObjectId},
                  MoqtDeliveryOrder::kAscending)),
              StatusIs(absl::StatusCode::kNotFound));

  queue.AddObject(quiche::QuicheMemSlice::Copy("f"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("g"), false);
  EXPECT_THAT(
      FetchToVector(queue.StandaloneFetch(Location{0, 0}, Location{0, 1},
                                          MoqtDeliveryOrder::kAscending)),
      StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(
      FetchToVector(queue.StandaloneFetch(Location{0, 0}, Location{2, 0},
                                          MoqtDeliveryOrder::kAscending)),
      IsOkAndHolds(ElementsAre("c", "d", "e")));
  EXPECT_THAT(FetchToVector(queue.StandaloneFetch(
                  Location{1, 0}, Location{5, kMaxObjectId},
                  MoqtDeliveryOrder::kAscending)),
              IsOkAndHolds(ElementsAre("c", "d", "e", "f", "g")));
  EXPECT_THAT(FetchToVector(queue.StandaloneFetch(
                  Location{3, 0}, Location{5, kMaxObjectId},
                  MoqtDeliveryOrder::kAscending)),
              IsOkAndHolds(ElementsAre("f", "g")));
}

TEST(MoqtOutgoingQueue, RelativeJoiningFetch) {
  TestMoqtOutgoingQueue queue;
  EXPECT_QUICHE_BUG(
      queue.RelativeFetch(1, MoqtDeliveryOrder::kAscending),
      "Calling RelativeFetch\\(\\) on an established subscription");
}

TEST(MoqtOutgoingQueue, AbsoluteJoiningFetch) {
  TestMoqtOutgoingQueue queue;
  EXPECT_QUICHE_BUG(
      queue.AbsoluteFetch(1, MoqtDeliveryOrder::kAscending),
      "Calling AbsoluteFetch\\(\\) on an established subscription");
}

TEST(MoqtOutgoingQueue, ObjectsGoneWhileFetching) {
  TestMoqtOutgoingQueue queue;
  queue.AddObject(quiche::QuicheMemSlice::Copy("a"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("b"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("c"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("d"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("e"), true);

  EXPECT_THAT(
      FetchToVector(queue.StandaloneFetch(Location{0, 0}, Location{5, 0},
                                          MoqtDeliveryOrder::kAscending)),
      IsOkAndHolds(ElementsAre("c", "d", "e")));
  std::unique_ptr<MoqtFetchTask> deferred_fetch = queue.StandaloneFetch(
      Location{0, 0}, Location{5, 0}, MoqtDeliveryOrder::kAscending);

  queue.AddObject(quiche::QuicheMemSlice::Copy("f"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("g"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("h"), true);
  queue.AddObject(quiche::QuicheMemSlice::Copy("i"), true);

  EXPECT_THAT(FetchToVector(std::move(deferred_fetch)),
              IsOkAndHolds(IsEmpty()));
}

TEST(MoqtOutgoingQueue, ObjectIsTimestamped) {
  quic::QuicDefaultClock* clock = quic::QuicDefaultClock::Get();
  quic::QuicTime test_start = clock->ApproximateNow();
  TestMoqtOutgoingQueue queue;
  queue.AddObject(quiche::QuicheMemSlice::Copy("a"), true);
  std::optional<PublishedObject> object = queue.GetCachedObject(0, 0, 0);
  ASSERT_TRUE(object.has_value());
  EXPECT_GE(object->metadata.arrival_time, test_start);
}

TEST(MoqtOutgoingQueue, EndOfTrack) {
  TestMoqtOutgoingQueue queue;
  queue.AddObject(quiche::QuicheMemSlice::Copy("a"), true);  // Create (0, 0)
  queue.AddObject(quiche::QuicheMemSlice::Copy("b"), true);  // Create (1, 0)
  std::unique_ptr<MoqtFetchTask> fetch = queue.StandaloneFetch(
      Location{0, 0}, Location{5, kMaxObjectId}, MoqtDeliveryOrder::kAscending);
  bool end_of_track = false;
  Location end_location;
  // end_of_track is false before Close() is called.
  fetch->SetFetchResponseCallback(
      [&end_of_track,
       &end_location](std::variant<MoqtFetchOk, MoqtRequestError> arg) {
        end_of_track = std::get<MoqtFetchOk>(arg).end_of_track;
        end_location = std::get<MoqtFetchOk>(arg).end_location;
      });
  EXPECT_FALSE(end_of_track);
  EXPECT_EQ(end_location, Location(1, 0));

  queue.Close();  // Create (2, 0)
  EXPECT_EQ(queue.largest_location(), Location(2, 0));
  fetch = queue.StandaloneFetch(Location{0, 0}, Location{1, kMaxObjectId},
                                MoqtDeliveryOrder::kAscending);
  // end_of_track is false if the fetch does not include the last object.
  fetch->SetFetchResponseCallback(
      [&end_of_track,
       &end_location](std::variant<MoqtFetchOk, MoqtRequestError> arg) {
        end_of_track = std::get<MoqtFetchOk>(arg).end_of_track;
        end_location = std::get<MoqtFetchOk>(arg).end_location;
      });
  EXPECT_FALSE(end_of_track);
  EXPECT_EQ(end_location, Location(1, 1));

  fetch = queue.StandaloneFetch(Location{0, 0}, Location{5, kMaxObjectId},
                                MoqtDeliveryOrder::kAscending);
  // end_of_track is true if the fetch includes the last object.
  fetch->SetFetchResponseCallback(
      [&end_of_track,
       &end_location](std::variant<MoqtFetchOk, MoqtRequestError> arg) {
        end_of_track = std::get<MoqtFetchOk>(arg).end_of_track;
        end_location = std::get<MoqtFetchOk>(arg).end_location;
      });
  EXPECT_TRUE(end_of_track);
  EXPECT_EQ(end_location, Location(2, 0));
}

// Regression test for b/459527759. `RemoveAllSubscriptions()` calls
// `MoqtObjectListener::OnTrackPublisherGone()` on each of its listeners. The
// implementation must iterate carefully because `OnTrackPublisherGone()`
// removes the listener from the container.
TEST(MoqtOutgoingQueue, RemoveAllSubscriptionsDoesNotCrash) {
  TestMoqtOutgoingQueue queue;
  queue.RemoveObjectListener(&queue);

  std::vector<MockMoqtObjectListener> listeners(2);
  for (MockMoqtObjectListener& listener : listeners) {
    EXPECT_CALL(listener, OnSubscribeAccepted).WillOnce(Return());
    EXPECT_CALL(listener, OnTrackPublisherGone).WillOnce([&] {
      queue.RemoveObjectListener(&listener);
    });
    queue.AddObjectListener(&listener);
  }

  queue.RemoveAllSubscriptions();
  EXPECT_FALSE(queue.HasSubscribers());
}

}  // namespace
}  // namespace moqt::test
