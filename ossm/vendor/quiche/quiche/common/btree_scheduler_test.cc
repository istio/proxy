// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/common/btree_scheduler.h"

#include <optional>
#include <ostream>
#include <string>
#include <tuple>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

namespace quiche::test {
namespace {

using ::testing::ElementsAre;
using ::testing::Optional;

template <typename Id, typename Priority>
void ScheduleIds(BTreeScheduler<Id, Priority>& scheduler,
                 absl::Span<const Id> ids) {
  for (Id id : ids) {
    QUICHE_EXPECT_OK(scheduler.Schedule(id));
  }
}

template <typename Id, typename Priority>
std::vector<Id> PopAll(BTreeScheduler<Id, Priority>& scheduler) {
  std::vector<Id> result;
  result.reserve(scheduler.NumScheduled());
  for (;;) {
    absl::StatusOr<Id> id = scheduler.PopFront();
    if (id.ok()) {
      result.push_back(*id);
    } else {
      EXPECT_THAT(id, StatusIs(absl::StatusCode::kNotFound));
      break;
    }
  }
  return result;
}

TEST(BTreeSchedulerTest, SimplePop) {
  BTreeScheduler<int, int> scheduler;
  QUICHE_EXPECT_OK(scheduler.Register(1, 100));
  QUICHE_EXPECT_OK(scheduler.Register(2, 101));
  QUICHE_EXPECT_OK(scheduler.Register(3, 102));

  EXPECT_THAT(scheduler.GetPriorityFor(1), Optional(100));
  EXPECT_THAT(scheduler.GetPriorityFor(3), Optional(102));
  EXPECT_EQ(scheduler.GetPriorityFor(5), std::nullopt);

  EXPECT_EQ(scheduler.NumScheduled(), 0u);
  EXPECT_FALSE(scheduler.HasScheduled());
  QUICHE_EXPECT_OK(scheduler.Schedule(1));
  QUICHE_EXPECT_OK(scheduler.Schedule(2));
  QUICHE_EXPECT_OK(scheduler.Schedule(3));
  EXPECT_EQ(scheduler.NumScheduled(), 3u);
  EXPECT_TRUE(scheduler.HasScheduled());

  EXPECT_THAT(scheduler.PopFront(), IsOkAndHolds(3));
  EXPECT_THAT(scheduler.PopFront(), IsOkAndHolds(2));
  EXPECT_THAT(scheduler.PopFront(), IsOkAndHolds(1));

  QUICHE_EXPECT_OK(scheduler.Schedule(2));
  QUICHE_EXPECT_OK(scheduler.Schedule(1));
  QUICHE_EXPECT_OK(scheduler.Schedule(3));

  EXPECT_THAT(scheduler.PopFront(), IsOkAndHolds(3));
  EXPECT_THAT(scheduler.PopFront(), IsOkAndHolds(2));
  EXPECT_THAT(scheduler.PopFront(), IsOkAndHolds(1));

  QUICHE_EXPECT_OK(scheduler.Schedule(3));
  QUICHE_EXPECT_OK(scheduler.Schedule(1));

  EXPECT_THAT(scheduler.PopFront(), IsOkAndHolds(3));
  EXPECT_THAT(scheduler.PopFront(), IsOkAndHolds(1));
}

TEST(BTreeSchedulerTest, FIFO) {
  BTreeScheduler<int, int> scheduler;
  QUICHE_EXPECT_OK(scheduler.Register(1, 100));
  QUICHE_EXPECT_OK(scheduler.Register(2, 100));
  QUICHE_EXPECT_OK(scheduler.Register(3, 100));

  ScheduleIds(scheduler, {2, 1, 3});
  EXPECT_THAT(PopAll(scheduler), ElementsAre(2, 1, 3));

  QUICHE_EXPECT_OK(scheduler.Register(4, 101));
  QUICHE_EXPECT_OK(scheduler.Register(5, 99));

  ScheduleIds(scheduler, {5, 1, 2, 3, 4});
  EXPECT_THAT(PopAll(scheduler), ElementsAre(4, 1, 2, 3, 5));
  ScheduleIds(scheduler, {1, 5, 2, 4, 3});
  EXPECT_THAT(PopAll(scheduler), ElementsAre(4, 1, 2, 3, 5));
  ScheduleIds(scheduler, {3, 5, 2, 4, 1});
  EXPECT_THAT(PopAll(scheduler), ElementsAre(4, 3, 2, 1, 5));
  ScheduleIds(scheduler, {3, 2, 1, 2, 3});
  EXPECT_THAT(PopAll(scheduler), ElementsAre(3, 2, 1));
}

TEST(BTreeSchedulerTest, NumEntriesInRange) {
  BTreeScheduler<int, int> scheduler;
  QUICHE_EXPECT_OK(scheduler.Register(1, 0));
  QUICHE_EXPECT_OK(scheduler.Register(2, 0));
  QUICHE_EXPECT_OK(scheduler.Register(3, 0));
  QUICHE_EXPECT_OK(scheduler.Register(4, -2));
  QUICHE_EXPECT_OK(scheduler.Register(5, -5));
  QUICHE_EXPECT_OK(scheduler.Register(6, 10));
  QUICHE_EXPECT_OK(scheduler.Register(7, 16));
  QUICHE_EXPECT_OK(scheduler.Register(8, 32));
  QUICHE_EXPECT_OK(scheduler.Register(9, 64));

  EXPECT_EQ(scheduler.NumScheduled(), 0u);
  EXPECT_EQ(scheduler.NumScheduledInPriorityRange(std::nullopt, std::nullopt),
            0u);
  EXPECT_EQ(scheduler.NumScheduledInPriorityRange(-1, 1), 0u);

  for (int stream = 1; stream <= 9; ++stream) {
    QUICHE_ASSERT_OK(scheduler.Schedule(stream));
  }

  EXPECT_EQ(scheduler.NumScheduled(), 9u);
  EXPECT_EQ(scheduler.NumScheduledInPriorityRange(std::nullopt, std::nullopt),
            9u);
  EXPECT_EQ(scheduler.NumScheduledInPriorityRange(0, 0), 3u);
  EXPECT_EQ(scheduler.NumScheduledInPriorityRange(std::nullopt, -1), 2u);
  EXPECT_EQ(scheduler.NumScheduledInPriorityRange(1, std::nullopt), 4u);
}

TEST(BTreeSchedulerTest, Registration) {
  BTreeScheduler<int, int> scheduler;
  QUICHE_EXPECT_OK(scheduler.Register(1, 0));
  QUICHE_EXPECT_OK(scheduler.Register(2, 0));

  QUICHE_EXPECT_OK(scheduler.Schedule(1));
  QUICHE_EXPECT_OK(scheduler.Schedule(2));
  EXPECT_EQ(scheduler.NumScheduled(), 2u);
  EXPECT_TRUE(scheduler.IsScheduled(2));

  EXPECT_THAT(scheduler.Register(2, 0),
              StatusIs(absl::StatusCode::kAlreadyExists));
  QUICHE_EXPECT_OK(scheduler.Unregister(2));
  EXPECT_EQ(scheduler.NumScheduled(), 1u);
  EXPECT_FALSE(scheduler.IsScheduled(2));

  EXPECT_THAT(scheduler.UpdatePriority(2, 1234),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(scheduler.Unregister(2), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(scheduler.Schedule(2), StatusIs(absl::StatusCode::kNotFound));
  QUICHE_EXPECT_OK(scheduler.Register(2, 0));
  EXPECT_EQ(scheduler.NumScheduled(), 1u);
  EXPECT_TRUE(scheduler.IsScheduled(1));
  EXPECT_FALSE(scheduler.IsScheduled(2));
}

TEST(BTreeSchedulerTest, UpdatePriorityUp) {
  BTreeScheduler<int, int> scheduler;
  QUICHE_EXPECT_OK(scheduler.Register(1, 0));
  QUICHE_EXPECT_OK(scheduler.Register(2, 0));
  QUICHE_EXPECT_OK(scheduler.Register(3, 0));

  ScheduleIds(scheduler, {1, 2, 3});
  QUICHE_EXPECT_OK(scheduler.UpdatePriority(2, 1000));
  EXPECT_THAT(PopAll(scheduler), ElementsAre(2, 1, 3));
}

TEST(BTreeSchedulerTest, UpdatePriorityDown) {
  BTreeScheduler<int, int> scheduler;
  QUICHE_EXPECT_OK(scheduler.Register(1, 0));
  QUICHE_EXPECT_OK(scheduler.Register(2, 0));
  QUICHE_EXPECT_OK(scheduler.Register(3, 0));

  ScheduleIds(scheduler, {1, 2, 3});
  QUICHE_EXPECT_OK(scheduler.UpdatePriority(2, -1000));
  EXPECT_THAT(PopAll(scheduler), ElementsAre(1, 3, 2));
}

TEST(BTreeSchedulerTest, UpdatePriorityEqual) {
  BTreeScheduler<int, int> scheduler;
  QUICHE_EXPECT_OK(scheduler.Register(1, 0));
  QUICHE_EXPECT_OK(scheduler.Register(2, 0));
  QUICHE_EXPECT_OK(scheduler.Register(3, 0));

  ScheduleIds(scheduler, {1, 2, 3});
  QUICHE_EXPECT_OK(scheduler.UpdatePriority(2, 0));
  EXPECT_THAT(PopAll(scheduler), ElementsAre(1, 2, 3));
}

TEST(BTreeSchedulerTest, UpdatePriorityIntoSameBucket) {
  BTreeScheduler<int, int> scheduler;
  QUICHE_EXPECT_OK(scheduler.Register(1, 0));
  QUICHE_EXPECT_OK(scheduler.Register(2, -100));
  QUICHE_EXPECT_OK(scheduler.Register(3, 0));

  ScheduleIds(scheduler, {1, 2, 3});
  QUICHE_EXPECT_OK(scheduler.UpdatePriority(2, 0));
  EXPECT_THAT(PopAll(scheduler), ElementsAre(1, 2, 3));
}

TEST(BTreeSchedulerTest, ShouldYield) {
  BTreeScheduler<int, int> scheduler;
  QUICHE_EXPECT_OK(scheduler.Register(10, 100));
  QUICHE_EXPECT_OK(scheduler.Register(20, 101));
  QUICHE_EXPECT_OK(scheduler.Register(21, 101));
  QUICHE_EXPECT_OK(scheduler.Register(30, 102));

  EXPECT_THAT(scheduler.ShouldYield(10), IsOkAndHolds(false));
  EXPECT_THAT(scheduler.ShouldYield(20), IsOkAndHolds(false));
  EXPECT_THAT(scheduler.ShouldYield(21), IsOkAndHolds(false));
  EXPECT_THAT(scheduler.ShouldYield(30), IsOkAndHolds(false));
  EXPECT_THAT(scheduler.ShouldYield(40), StatusIs(absl::StatusCode::kNotFound));

  QUICHE_EXPECT_OK(scheduler.Schedule(20));

  EXPECT_THAT(scheduler.ShouldYield(10), IsOkAndHolds(true));
  EXPECT_THAT(scheduler.ShouldYield(20), IsOkAndHolds(false));
  EXPECT_THAT(scheduler.ShouldYield(21), IsOkAndHolds(true));
  EXPECT_THAT(scheduler.ShouldYield(30), IsOkAndHolds(false));
}

TEST(BTreeSchedulerTest, Deschedule) {
  BTreeScheduler<int, int> scheduler;
  QUICHE_EXPECT_OK(scheduler.Register(10, 100));
  QUICHE_EXPECT_OK(scheduler.Register(20, 101));

  EXPECT_THAT(scheduler.Deschedule(10),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(scheduler.Deschedule(11), StatusIs(absl::StatusCode::kNotFound));

  EXPECT_FALSE(scheduler.IsScheduled(10));
  QUICHE_EXPECT_OK(scheduler.Schedule(10));
  EXPECT_TRUE(scheduler.IsScheduled(10));
  QUICHE_EXPECT_OK(scheduler.Deschedule(10));
  EXPECT_FALSE(scheduler.IsScheduled(10));
  QUICHE_EXPECT_OK(scheduler.Unregister(10));
}

struct CustomPriority {
  int a;
  int b;

  bool operator<(const CustomPriority& other) const {
    return std::make_tuple(a, b) < std::make_tuple(other.a, other.b);
  }
};

TEST(BTreeSchedulerTest, CustomPriority) {
  BTreeScheduler<int, CustomPriority> scheduler;
  QUICHE_EXPECT_OK(scheduler.Register(10, CustomPriority{0, 1}));
  QUICHE_EXPECT_OK(scheduler.Register(11, CustomPriority{0, 0}));
  QUICHE_EXPECT_OK(scheduler.Register(12, CustomPriority{0, 0}));
  QUICHE_EXPECT_OK(scheduler.Register(13, CustomPriority{10, 0}));
  QUICHE_EXPECT_OK(scheduler.Register(14, CustomPriority{-10, 0}));

  ScheduleIds(scheduler, {10, 11, 12, 13, 14});
  EXPECT_THAT(PopAll(scheduler), ElementsAre(13, 10, 11, 12, 14));
}

struct CustomId {
  int a;
  std::string b;

  bool operator==(const CustomId& other) const {
    return a == other.a && b == other.b;
  }

  template <typename H>
  friend H AbslHashValue(H h, const CustomId& c) {
    return H::combine(std::move(h), c.a, c.b);
  }
};

std::ostream& operator<<(std::ostream& os, const CustomId& id) {
  os << id.a << ":" << id.b;
  return os;
}

TEST(BTreeSchedulerTest, CustomIds) {
  BTreeScheduler<CustomId, int> scheduler;
  QUICHE_EXPECT_OK(scheduler.Register(CustomId{1, "foo"}, 10));
  QUICHE_EXPECT_OK(scheduler.Register(CustomId{1, "bar"}, 12));
  QUICHE_EXPECT_OK(scheduler.Register(CustomId{2, "foo"}, 11));
  EXPECT_THAT(scheduler.Register(CustomId{1, "foo"}, 10),
              StatusIs(absl::StatusCode::kAlreadyExists));

  ScheduleIds(scheduler,
              {CustomId{1, "foo"}, CustomId{1, "bar"}, CustomId{2, "foo"}});
  EXPECT_THAT(scheduler.ShouldYield(CustomId{1, "foo"}), IsOkAndHolds(true));
  EXPECT_THAT(scheduler.ShouldYield(CustomId{1, "bar"}), IsOkAndHolds(false));
  EXPECT_THAT(
      PopAll(scheduler),
      ElementsAre(CustomId{1, "bar"}, CustomId{2, "foo"}, CustomId{1, "foo"}));
}

}  // namespace
}  // namespace quiche::test
