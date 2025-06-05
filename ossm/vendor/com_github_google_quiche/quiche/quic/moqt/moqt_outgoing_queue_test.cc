// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_outgoing_queue.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/quic_default_clock.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_priority.h"
#include "quiche/quic/moqt/moqt_publisher.h"
#include "quiche/quic/moqt/moqt_subscribe_windows.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/platform/api/quiche_expect_bug.h"
#include "quiche/common/platform/api/quiche_logging.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/test_tools/quiche_test_utils.h"
#include "quiche/web_transport/web_transport.h"

namespace moqt {
namespace {

using ::quic::test::MemSliceFromString;
using ::quiche::test::IsOkAndHolds;
using ::quiche::test::StatusIs;
using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

class TestMoqtOutgoingQueue : public MoqtOutgoingQueue,
                              public MoqtObjectListener {
 public:
  TestMoqtOutgoingQueue()
      : MoqtOutgoingQueue(FullTrackName{"test", "track"},
                          MoqtForwardingPreference::kSubgroup) {
    AddObjectListener(this);
  }

  void OnNewObjectAvailable(FullSequence sequence) override {
    std::optional<PublishedObject> object = GetCachedObject(sequence);
    QUICHE_CHECK(object.has_value());
    ASSERT_THAT(object->status, AnyOf(MoqtObjectStatus::kNormal,
                                      MoqtObjectStatus::kEndOfGroup));
    if (object->status == MoqtObjectStatus::kNormal) {
      PublishObject(object->sequence.group, object->sequence.object,
                    object->payload.AsStringView());
    } else {
      CloseStreamForGroup(object->sequence.group);
    }
  }

  void GetObjectsFromPast(const SubscribeWindow& window) {
    std::vector<FullSequence> objects =
        GetCachedObjectsInRange(FullSequence(0, 0), GetLargestSequence());
    for (FullSequence object : objects) {
      if (window.InWindow(object)) {
        OnNewObjectAvailable(object);
      }
    }
  }

  MOCK_METHOD(void, OnNewFinAvailable, (FullSequence sequence));
  MOCK_METHOD(void, OnSubgroupAbandoned,
              (FullSequence sequence,
               webtransport::StreamErrorCode error_code));
  MOCK_METHOD(void, OnGroupAbandoned, (uint64_t group_id));
  MOCK_METHOD(void, CloseStreamForGroup, (uint64_t group_id), ());
  MOCK_METHOD(void, PublishObject,
              (uint64_t group_id, uint64_t object_id,
               absl::string_view payload),
              ());
  MOCK_METHOD(void, OnTrackPublisherGone, (), (override));
  MOCK_METHOD(void, OnSubscribeAccepted, (), (override));
  MOCK_METHOD(void, OnSubscribeRejected,
              (MoqtSubscribeErrorReason reason,
               std::optional<uint64_t> track_alias),
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
        if (object.status == MoqtObjectStatus::kNormal) {
          objects.emplace_back(object.payload.AsStringView());
        } else {
          EXPECT_EQ(object.status, MoqtObjectStatus::kEndOfGroup);
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
  EXPECT_QUICHE_BUG(queue.AddObject(MemSliceFromString("a"), false),
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
  queue.AddObject(MemSliceFromString("a"), true);
  queue.AddObject(MemSliceFromString("b"), false);
  queue.AddObject(MemSliceFromString("c"), false);
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
  queue.AddObject(MemSliceFromString("a"), true);
  queue.AddObject(MemSliceFromString("b"), false);
  queue.AddObject(MemSliceFromString("c"), false);
  queue.GetObjectsFromPast(SubscribeWindow(FullSequence(0, 0)));
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
  queue.AddObject(MemSliceFromString("a"), true);
  queue.AddObject(MemSliceFromString("b"), false);
  queue.AddObject(MemSliceFromString("c"), false);
  queue.GetObjectsFromPast(SubscribeWindow(FullSequence(0, 1)));
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
  queue.AddObject(MemSliceFromString("a"), true);
  queue.AddObject(MemSliceFromString("b"), false);
  queue.AddObject(MemSliceFromString("c"), false);
  queue.AddObject(MemSliceFromString("d"), true);
  queue.AddObject(MemSliceFromString("e"), false);
  queue.AddObject(MemSliceFromString("f"), false);
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
  queue.AddObject(MemSliceFromString("a"), true);
  queue.AddObject(MemSliceFromString("b"), false);
  queue.AddObject(MemSliceFromString("c"), false);
  queue.AddObject(MemSliceFromString("d"), true);
  queue.AddObject(MemSliceFromString("e"), false);
  queue.AddObject(MemSliceFromString("f"), false);
  queue.GetObjectsFromPast(SubscribeWindow(FullSequence(0, 1)));
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
  queue.AddObject(MemSliceFromString("a"), true);
  queue.AddObject(MemSliceFromString("b"), false);
  queue.AddObject(MemSliceFromString("c"), true);
  queue.AddObject(MemSliceFromString("d"), false);
  queue.AddObject(MemSliceFromString("e"), true);
  queue.AddObject(MemSliceFromString("f"), false);
  queue.AddObject(MemSliceFromString("g"), true);
  queue.AddObject(MemSliceFromString("h"), false);
  queue.AddObject(MemSliceFromString("i"), true);
  queue.AddObject(MemSliceFromString("j"), false);
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
  queue.AddObject(MemSliceFromString("a"), true);
  queue.AddObject(MemSliceFromString("b"), false);
  queue.AddObject(MemSliceFromString("c"), true);
  queue.AddObject(MemSliceFromString("d"), false);
  queue.AddObject(MemSliceFromString("e"), true);
  queue.AddObject(MemSliceFromString("f"), false);
  queue.AddObject(MemSliceFromString("g"), true);
  queue.AddObject(MemSliceFromString("h"), false);
  queue.AddObject(MemSliceFromString("i"), true);
  queue.AddObject(MemSliceFromString("j"), false);
  queue.GetObjectsFromPast(SubscribeWindow(FullSequence(0, 0)));
}

TEST(MoqtOutgoingQueue, Fetch) {
  TestMoqtOutgoingQueue queue;
  EXPECT_THAT(FetchToVector(queue.Fetch(FullSequence{0, 0}, 2, 0,
                                        MoqtDeliveryOrder::kAscending)),
              StatusIs(absl::StatusCode::kNotFound));

  queue.AddObject(MemSliceFromString("a"), true);
  queue.AddObject(MemSliceFromString("b"), false);
  queue.AddObject(MemSliceFromString("c"), true);
  queue.AddObject(MemSliceFromString("d"), false);
  queue.AddObject(MemSliceFromString("e"), true);

  EXPECT_THAT(FetchToVector(queue.Fetch(FullSequence{0, 0}, 2, 0,
                                        MoqtDeliveryOrder::kAscending)),
              IsOkAndHolds(ElementsAre("a", "b", "c", "d", "e")));
  EXPECT_THAT(FetchToVector(queue.Fetch(FullSequence{0, 100}, 0, 1000,
                                        MoqtDeliveryOrder::kAscending)),
              IsOkAndHolds(IsEmpty()));
  EXPECT_THAT(FetchToVector(queue.Fetch(FullSequence{0, 0}, 2, 0,
                                        MoqtDeliveryOrder::kDescending)),
              IsOkAndHolds(ElementsAre("e", "c", "d", "a", "b")));
  EXPECT_THAT(FetchToVector(queue.Fetch(FullSequence{0, 0}, 1, 0,
                                        MoqtDeliveryOrder::kAscending)),
              IsOkAndHolds(ElementsAre("a", "b", "c")));
  EXPECT_THAT(FetchToVector(queue.Fetch(FullSequence{0, 0}, 1, 0,
                                        MoqtDeliveryOrder::kAscending)),
              IsOkAndHolds(ElementsAre("a", "b", "c")));
  EXPECT_THAT(FetchToVector(queue.Fetch(FullSequence{1, 0}, 5, std::nullopt,
                                        MoqtDeliveryOrder::kAscending)),
              IsOkAndHolds(ElementsAre("c", "d", "e")));
  EXPECT_THAT(FetchToVector(queue.Fetch(FullSequence{3, 0}, 5, std::nullopt,
                                        MoqtDeliveryOrder::kAscending)),
              StatusIs(absl::StatusCode::kNotFound));

  queue.AddObject(MemSliceFromString("f"), true);
  queue.AddObject(MemSliceFromString("g"), false);
  EXPECT_THAT(FetchToVector(queue.Fetch(FullSequence{0, 0}, 0, 1,
                                        MoqtDeliveryOrder::kAscending)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(FetchToVector(queue.Fetch(FullSequence{0, 0}, 2, 0,
                                        MoqtDeliveryOrder::kAscending)),
              IsOkAndHolds(ElementsAre("c", "d", "e")));
  EXPECT_THAT(FetchToVector(queue.Fetch(FullSequence{1, 0}, 5, std::nullopt,
                                        MoqtDeliveryOrder::kAscending)),
              IsOkAndHolds(ElementsAre("c", "d", "e", "f", "g")));
  EXPECT_THAT(FetchToVector(queue.Fetch(FullSequence{3, 0}, 5, std::nullopt,
                                        MoqtDeliveryOrder::kAscending)),
              IsOkAndHolds(ElementsAre("f", "g")));
}

TEST(MoqtOutgoingQueue, ObjectsGoneWhileFetching) {
  TestMoqtOutgoingQueue queue;
  queue.AddObject(MemSliceFromString("a"), true);
  queue.AddObject(MemSliceFromString("b"), true);
  queue.AddObject(MemSliceFromString("c"), true);
  queue.AddObject(MemSliceFromString("d"), true);
  queue.AddObject(MemSliceFromString("e"), true);

  EXPECT_THAT(FetchToVector(queue.Fetch(FullSequence{0, 0}, 5, 0,
                                        MoqtDeliveryOrder::kAscending)),
              IsOkAndHolds(ElementsAre("c", "d", "e")));
  std::unique_ptr<MoqtFetchTask> deferred_fetch =
      queue.Fetch(FullSequence{0, 0}, 5, 0, MoqtDeliveryOrder::kAscending);

  queue.AddObject(MemSliceFromString("f"), true);
  queue.AddObject(MemSliceFromString("g"), true);
  queue.AddObject(MemSliceFromString("h"), true);
  queue.AddObject(MemSliceFromString("i"), true);

  EXPECT_THAT(FetchToVector(std::move(deferred_fetch)),
              IsOkAndHolds(IsEmpty()));
}

TEST(MoqtOutgoingQueue, ObjectIsTimestamped) {
  quic::QuicDefaultClock* clock = quic::QuicDefaultClock::Get();
  quic::QuicTime test_start = clock->ApproximateNow();
  TestMoqtOutgoingQueue queue;
  queue.AddObject(MemSliceFromString("a"), true);
  std::optional<PublishedObject> object =
      queue.GetCachedObject(FullSequence{0, 0});
  ASSERT_TRUE(object.has_value());
  EXPECT_GE(object->arrival_time, test_start);
}

}  // namespace
}  // namespace moqt
