// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_outstanding_objects.h"

#include <cstdint>

#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace moqt::test {
namespace {

class MoqtOutstandingObjectsTest : public quiche::test::QuicheTest {};

constexpr uint64_t kDefaultMaxOutOfOrderObjects = 10;

TEST_F(MoqtOutstandingObjectsTest, AckMissingObject) {
  MoqtOutstandingObjects objects(kDefaultMaxOutOfOrderObjects);
  EXPECT_EQ(objects.tracked_objects_count(), 0);
  EXPECT_EQ(objects.OnObjectAcked(Location(5, 6)), -1);
}

TEST_F(MoqtOutstandingObjectsTest, AckFutureObject) {
  MoqtOutstandingObjects objects(kDefaultMaxOutOfOrderObjects);
  objects.OnObjectAdded(Location(5, 5));
  EXPECT_EQ(objects.OnObjectAcked(Location(5, 4)), -1);
  EXPECT_EQ(objects.OnObjectAcked(Location(5, 6)), -1);
}

TEST_F(MoqtOutstandingObjectsTest, AckOneObject) {
  MoqtOutstandingObjects objects(kDefaultMaxOutOfOrderObjects);
  objects.OnObjectAdded(Location(5, 6));
  EXPECT_EQ(objects.tracked_objects_count(), 1);
  EXPECT_EQ(objects.OnObjectAcked(Location(5, 6)), 0);
  EXPECT_EQ(objects.tracked_objects_count(), 0);
}

TEST_F(MoqtOutstandingObjectsTest, AckTwoObjectsInOrder) {
  MoqtOutstandingObjects objects(kDefaultMaxOutOfOrderObjects);
  objects.OnObjectAdded(Location(5, 6));
  objects.OnObjectAdded(Location(5, 7));
  EXPECT_EQ(objects.tracked_objects_count(), 2);

  EXPECT_EQ(objects.OnObjectAcked(Location(5, 6)), 0);
  EXPECT_EQ(objects.tracked_objects_count(), 1);
  EXPECT_EQ(objects.OnObjectAcked(Location(5, 7)), 0);
  EXPECT_EQ(objects.tracked_objects_count(), 0);
}

TEST_F(MoqtOutstandingObjectsTest, AckTwoObjectsOutOfOrder) {
  MoqtOutstandingObjects objects(kDefaultMaxOutOfOrderObjects);
  objects.OnObjectAdded(Location(5, 6));
  objects.OnObjectAdded(Location(5, 7));
  EXPECT_EQ(objects.tracked_objects_count(), 2);

  EXPECT_EQ(objects.OnObjectAcked(Location(5, 7)), 1);
  EXPECT_EQ(objects.tracked_objects_count(), 2);
  EXPECT_EQ(objects.OnObjectAcked(Location(5, 6)), 0);
  EXPECT_EQ(objects.tracked_objects_count(), 0);
}

TEST_F(MoqtOutstandingObjectsTest, AddTwoObjectsOutOfOrder) {
  MoqtOutstandingObjects objects(kDefaultMaxOutOfOrderObjects);
  objects.OnObjectAdded(Location(10, 0));
  objects.OnObjectAdded(Location(5, 0));

  EXPECT_EQ(objects.OnObjectAcked(Location(10, 0)), 0);
  EXPECT_EQ(objects.tracked_objects_count(), 1);
  EXPECT_EQ(objects.OnObjectAcked(Location(5, 0)), 0);
  EXPECT_EQ(objects.tracked_objects_count(), 0);
}

TEST_F(MoqtOutstandingObjectsTest,
       RemoveObjectsWhenOutOfOrderThresholdExceeded) {
  MoqtOutstandingObjects objects(3);
  for (int i = 0; i < 10; ++i) {
    objects.OnObjectAdded(Location(i, 0));
  }

  EXPECT_EQ(objects.tracked_objects_count(), 10);
  objects.OnObjectAcked(Location(1, 0));
  EXPECT_EQ(objects.tracked_objects_count(), 10);
  objects.OnObjectAcked(Location(2, 0));
  EXPECT_EQ(objects.tracked_objects_count(), 10);
  objects.OnObjectAcked(Location(3, 0));
  EXPECT_EQ(objects.tracked_objects_count(), 10);
  objects.OnObjectAcked(Location(4, 0));
  EXPECT_EQ(objects.tracked_objects_count(), 5);

  EXPECT_EQ(-1, objects.OnObjectAcked(Location(0, 0)));
  EXPECT_EQ(0, objects.OnObjectAcked(Location(5, 0)));
}

TEST_F(MoqtOutstandingObjectsTest,
       RemoveObjectsWhenOutOfOrderThresholdExceeded2) {
  MoqtOutstandingObjects objects(3);
  for (int i = 0; i < 10; ++i) {
    objects.OnObjectAdded(Location(i, 0));
  }

  EXPECT_EQ(objects.tracked_objects_count(), 10);
  objects.OnObjectAcked(Location(5, 0));
  EXPECT_EQ(objects.tracked_objects_count(), 8);

  EXPECT_EQ(-1, objects.OnObjectAcked(Location(0, 0)));
  EXPECT_EQ(-1, objects.OnObjectAcked(Location(1, 0)));
  EXPECT_EQ(0, objects.OnObjectAcked(Location(2, 0)));
}

TEST_F(MoqtOutstandingObjectsTest, ObjectsRemovedWhenLimitExceeded) {
  MoqtOutstandingObjects objects(kDefaultMaxOutOfOrderObjects);
  for (int i = 0; i < MoqtOutstandingObjects::kMaxObjectsTracked; ++i) {
    objects.OnObjectAdded(Location(i, 0));
  }
  EXPECT_EQ(objects.tracked_objects_count(),
            MoqtOutstandingObjects::kMaxObjectsTracked);
  objects.OnObjectAdded(Location(10000, 0));
  EXPECT_EQ(objects.tracked_objects_count(),
            MoqtOutstandingObjects::kMaxObjectsTracked);
  EXPECT_EQ(-1, objects.OnObjectAcked(Location(0, 0)));
  EXPECT_EQ(0, objects.OnObjectAcked(Location(1, 0)));
}

}  // namespace
}  // namespace moqt::test
