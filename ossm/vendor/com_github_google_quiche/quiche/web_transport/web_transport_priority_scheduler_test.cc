// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/web_transport/web_transport_priority_scheduler.h"

#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/test_tools/quiche_test_utils.h"
#include "quiche/web_transport/web_transport.h"

namespace webtransport {
namespace {

using ::quiche::test::IsOkAndHolds;
using ::quiche::test::StatusIs;
using ::testing::ElementsAre;

void ScheduleIds(PriorityScheduler& scheduler, absl::Span<const StreamId> ids) {
  for (StreamId id : ids) {
    QUICHE_EXPECT_OK(scheduler.Schedule(id));
  }
}

std::vector<StreamId> PopAll(PriorityScheduler& scheduler) {
  std::vector<StreamId> result;
  result.reserve(scheduler.NumScheduled());
  for (;;) {
    absl::StatusOr<StreamId> id = scheduler.PopFront();
    if (!id.ok()) {
      EXPECT_THAT(id, StatusIs(absl::StatusCode::kNotFound));
      break;
    }
    result.push_back(*id);
  }
  return result;
}

TEST(WebTransportSchedulerTest, Register) {
  PriorityScheduler scheduler;

  // Register two streams in the same group.
  QUICHE_EXPECT_OK(scheduler.Register(0, StreamPriority{0, 0}));
  QUICHE_EXPECT_OK(scheduler.Register(1, StreamPriority{0, 0}));
  QUICHE_EXPECT_OK(scheduler.Register(2, StreamPriority{1, 0}));
  QUICHE_EXPECT_OK(scheduler.Register(3, StreamPriority{1, 0}));
  QUICHE_EXPECT_OK(scheduler.Register(4, StreamPriority{0, 0}));

  // Attempt re-registering.
  EXPECT_THAT(scheduler.Register(4, StreamPriority{0, 0}),
              StatusIs(absl::StatusCode::kAlreadyExists));
  EXPECT_THAT(scheduler.Register(4, StreamPriority{1, 0}),
              StatusIs(absl::StatusCode::kAlreadyExists));
}

TEST(WebTransportSchedulerTest, Unregister) {
  PriorityScheduler scheduler;

  EXPECT_FALSE(scheduler.HasRegistered());
  QUICHE_EXPECT_OK(scheduler.Register(0, StreamPriority{0, 0}));
  QUICHE_EXPECT_OK(scheduler.Register(1, StreamPriority{0, 0}));

  EXPECT_TRUE(scheduler.HasRegistered());
  QUICHE_EXPECT_OK(scheduler.Unregister(1));
  EXPECT_TRUE(scheduler.HasRegistered());
  QUICHE_EXPECT_OK(scheduler.Register(1, StreamPriority{0, 0}));

  ScheduleIds(scheduler, {0, 1});
  QUICHE_EXPECT_OK(scheduler.Unregister(0));
  QUICHE_EXPECT_OK(scheduler.Unregister(1));
  EXPECT_FALSE(scheduler.HasRegistered());
  QUICHE_EXPECT_OK(scheduler.Register(0, StreamPriority{0, 0}));
  QUICHE_EXPECT_OK(scheduler.Register(1, StreamPriority{0, 0}));
  EXPECT_TRUE(scheduler.HasRegistered());
  EXPECT_FALSE(scheduler.HasScheduled());
}

TEST(WebTransportSchedulerTest, UpdatePriority) {
  PriorityScheduler scheduler;

  QUICHE_EXPECT_OK(scheduler.Register(0, StreamPriority{0, 10}));
  QUICHE_EXPECT_OK(scheduler.Register(1, StreamPriority{0, 20}));
  EXPECT_EQ(scheduler.GetPriorityFor(0), (StreamPriority{0, 10}));
  EXPECT_EQ(scheduler.GetPriorityFor(1), (StreamPriority{0, 20}));

  QUICHE_EXPECT_OK(scheduler.UpdateSendGroup(0, 1));
  QUICHE_EXPECT_OK(scheduler.UpdateSendOrder(1, 40));
  EXPECT_EQ(scheduler.GetPriorityFor(0), (StreamPriority{1, 10}));
  EXPECT_EQ(scheduler.GetPriorityFor(1), (StreamPriority{0, 40}));

  EXPECT_THAT(scheduler.UpdateSendGroup(1000, 1),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(scheduler.UpdateSendOrder(1000, 1),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_EQ(scheduler.GetPriorityFor(1000), std::nullopt);
}

TEST(WebTransportSchedulerTest, Schedule) {
  PriorityScheduler scheduler;

  QUICHE_EXPECT_OK(scheduler.Register(0, StreamPriority{0, 0}));
  QUICHE_EXPECT_OK(scheduler.Register(1, StreamPriority{0, 0}));

  EXPECT_FALSE(scheduler.IsScheduled(0));
  EXPECT_FALSE(scheduler.IsScheduled(1));
  EXPECT_FALSE(scheduler.IsScheduled(1000));

  QUICHE_EXPECT_OK(scheduler.Schedule(0));
  EXPECT_TRUE(scheduler.IsScheduled(0));
  EXPECT_FALSE(scheduler.IsScheduled(1));

  QUICHE_EXPECT_OK(scheduler.Schedule(1));
  EXPECT_TRUE(scheduler.IsScheduled(0));
  EXPECT_TRUE(scheduler.IsScheduled(1));

  EXPECT_THAT(scheduler.Schedule(0), StatusIs(absl::StatusCode::kOk));
  EXPECT_THAT(scheduler.Schedule(2), StatusIs(absl::StatusCode::kNotFound));
}

TEST(WebTransportSchedulerTest, SamePriority) {
  PriorityScheduler scheduler;

  QUICHE_EXPECT_OK(scheduler.Register(0, StreamPriority{0, 0}));
  QUICHE_EXPECT_OK(scheduler.Register(1, StreamPriority{0, 0}));
  QUICHE_EXPECT_OK(scheduler.Register(2, StreamPriority{0, 0}));
  QUICHE_EXPECT_OK(scheduler.Register(3, StreamPriority{0, 0}));

  ScheduleIds(scheduler, {0, 1, 2, 3});
  EXPECT_EQ(scheduler.NumScheduled(), 4);
  EXPECT_THAT(PopAll(scheduler), ElementsAre(0, 1, 2, 3));
  ScheduleIds(scheduler, {3, 1, 2});
  EXPECT_THAT(PopAll(scheduler), ElementsAre(3, 1, 2));
}

TEST(WebTransportSchedulerTest, SingleBucketOrdered) {
  PriorityScheduler scheduler;

  QUICHE_EXPECT_OK(scheduler.Register(0, StreamPriority{0, 0}));
  QUICHE_EXPECT_OK(scheduler.Register(1, StreamPriority{0, 1}));
  QUICHE_EXPECT_OK(scheduler.Register(2, StreamPriority{0, 2}));
  QUICHE_EXPECT_OK(scheduler.Register(3, StreamPriority{0, 3}));

  ScheduleIds(scheduler, {0, 1, 2, 3});
  EXPECT_THAT(PopAll(scheduler), ElementsAre(3, 2, 1, 0));
  ScheduleIds(scheduler, {3, 1, 2, 0});
  EXPECT_THAT(PopAll(scheduler), ElementsAre(3, 2, 1, 0));
}

TEST(WebTransportSchedulerTest, EveryStreamInItsOwnBucket) {
  PriorityScheduler scheduler;

  QUICHE_EXPECT_OK(scheduler.Register(0, StreamPriority{0, 0}));
  QUICHE_EXPECT_OK(scheduler.Register(1, StreamPriority{1, 1}));
  QUICHE_EXPECT_OK(scheduler.Register(2, StreamPriority{2, 2}));
  QUICHE_EXPECT_OK(scheduler.Register(3, StreamPriority{3, 3}));

  ScheduleIds(scheduler, {0, 1, 2, 3});
  EXPECT_THAT(PopAll(scheduler), ElementsAre(0, 1, 2, 3));
  ScheduleIds(scheduler, {3, 1, 2});
  EXPECT_THAT(PopAll(scheduler), ElementsAre(3, 1, 2));
}

TEST(WebTransportSchedulerTest, TwoBucketsNoSendOrder) {
  PriorityScheduler scheduler;

  QUICHE_EXPECT_OK(scheduler.Register(0, StreamPriority{0, 0}));
  QUICHE_EXPECT_OK(scheduler.Register(1, StreamPriority{0, 0}));
  QUICHE_EXPECT_OK(scheduler.Register(2, StreamPriority{1, 0}));
  QUICHE_EXPECT_OK(scheduler.Register(3, StreamPriority{1, 0}));

  ScheduleIds(scheduler, {0, 1, 2, 3});
  EXPECT_THAT(PopAll(scheduler), ElementsAre(0, 2, 1, 3));
  ScheduleIds(scheduler, {0, 2, 1, 3});
  EXPECT_THAT(PopAll(scheduler), ElementsAre(0, 2, 1, 3));
  ScheduleIds(scheduler, {3, 2, 1, 0});
  EXPECT_THAT(PopAll(scheduler), ElementsAre(3, 1, 2, 0));

  ScheduleIds(scheduler, {0, 2});
  EXPECT_THAT(scheduler.PopFront(), IsOkAndHolds(0));
  ScheduleIds(scheduler, {1, 3, 0});
  EXPECT_THAT(PopAll(scheduler), ElementsAre(2, 1, 3, 0));
}

TEST(WebTransportSchedulerTest, TwoBucketsWithSendOrder) {
  PriorityScheduler scheduler;

  QUICHE_EXPECT_OK(scheduler.Register(0, StreamPriority{0, 0}));
  QUICHE_EXPECT_OK(scheduler.Register(1, StreamPriority{0, 10}));
  QUICHE_EXPECT_OK(scheduler.Register(2, StreamPriority{1, 20}));
  QUICHE_EXPECT_OK(scheduler.Register(3, StreamPriority{1, 30}));

  ScheduleIds(scheduler, {0, 1, 2, 3});
  EXPECT_THAT(PopAll(scheduler), ElementsAre(1, 3, 0, 2));
  ScheduleIds(scheduler, {3, 2, 1, 0});
  EXPECT_THAT(PopAll(scheduler), ElementsAre(3, 1, 2, 0));
}

TEST(WebTransportSchedulerTest, ShouldYield) {
  PriorityScheduler scheduler;

  QUICHE_EXPECT_OK(scheduler.Register(0, StreamPriority{0, 0}));
  QUICHE_EXPECT_OK(scheduler.Register(1, StreamPriority{0, 0}));
  QUICHE_EXPECT_OK(scheduler.Register(2, StreamPriority{0, 10}));
  QUICHE_EXPECT_OK(scheduler.Register(3, StreamPriority{1, 0}));

  EXPECT_THAT(scheduler.ShouldYield(0), IsOkAndHolds(false));
  EXPECT_THAT(scheduler.ShouldYield(1), IsOkAndHolds(false));
  EXPECT_THAT(scheduler.ShouldYield(2), IsOkAndHolds(false));
  EXPECT_THAT(scheduler.ShouldYield(3), IsOkAndHolds(false));
  EXPECT_THAT(scheduler.ShouldYield(4), StatusIs(absl::StatusCode::kNotFound));

  QUICHE_EXPECT_OK(scheduler.Schedule(0));
  EXPECT_THAT(scheduler.ShouldYield(0), IsOkAndHolds(false));
  EXPECT_THAT(scheduler.ShouldYield(1), IsOkAndHolds(true));
  EXPECT_THAT(scheduler.ShouldYield(2), IsOkAndHolds(false));
  EXPECT_THAT(scheduler.ShouldYield(3), IsOkAndHolds(true));
  PopAll(scheduler);

  QUICHE_EXPECT_OK(scheduler.Schedule(2));
  EXPECT_THAT(scheduler.ShouldYield(0), IsOkAndHolds(true));
  EXPECT_THAT(scheduler.ShouldYield(1), IsOkAndHolds(true));
  EXPECT_THAT(scheduler.ShouldYield(2), IsOkAndHolds(false));
  EXPECT_THAT(scheduler.ShouldYield(3), IsOkAndHolds(true));
  PopAll(scheduler);

  QUICHE_EXPECT_OK(scheduler.Schedule(3));
  EXPECT_THAT(scheduler.ShouldYield(0), IsOkAndHolds(true));
  EXPECT_THAT(scheduler.ShouldYield(1), IsOkAndHolds(true));
  EXPECT_THAT(scheduler.ShouldYield(2), IsOkAndHolds(true));
  EXPECT_THAT(scheduler.ShouldYield(3), IsOkAndHolds(false));
  PopAll(scheduler);
}

TEST(WebTransportSchedulerTest, UpdatePriorityWhileScheduled) {
  PriorityScheduler scheduler;

  QUICHE_EXPECT_OK(scheduler.Register(0, StreamPriority{0, 0}));
  QUICHE_EXPECT_OK(scheduler.Register(1, StreamPriority{0, 0}));
  QUICHE_EXPECT_OK(scheduler.Register(2, StreamPriority{1, 0}));
  QUICHE_EXPECT_OK(scheduler.Register(3, StreamPriority{1, 0}));

  ScheduleIds(scheduler, {0, 1, 2, 3});
  EXPECT_THAT(PopAll(scheduler), ElementsAre(0, 2, 1, 3));

  ScheduleIds(scheduler, {0, 1, 2, 3});
  QUICHE_EXPECT_OK(scheduler.UpdateSendOrder(1, 10));
  EXPECT_THAT(PopAll(scheduler), ElementsAre(1, 2, 0, 3));

  ScheduleIds(scheduler, {0, 1, 2, 3});
  QUICHE_EXPECT_OK(scheduler.UpdateSendGroup(1, 1));
  EXPECT_THAT(PopAll(scheduler), ElementsAre(0, 1, 2, 3));
}

}  // namespace
}  // namespace webtransport
