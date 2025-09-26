// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_relay_track_publisher.h"

#include <cstdint>
#include <memory>
#include <optional>

#include "absl/strings/string_view.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_object.h"
#include "quiche/quic/moqt/moqt_priority.h"
#include "quiche/quic/moqt/moqt_publisher.h"
#include "quiche/quic/moqt/moqt_session_interface.h"
#include "quiche/quic/moqt/test_tools/mock_moqt_session.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/web_transport/web_transport.h"

namespace moqt::test {

namespace {

const FullTrackName kTrackName = {"test", "track"};

class MockMoqtObjectListener : public MoqtObjectListener {
 public:
  MOCK_METHOD(void, OnSubscribeAccepted, (), (override));
  MOCK_METHOD(void, OnSubscribeRejected, (MoqtSubscribeErrorReason reason),
              (override));
  MOCK_METHOD(void, OnNewObjectAvailable,
              (Location sequence, uint64_t subgroup,
               MoqtPriority publisher_priority),
              (override));
  MOCK_METHOD(void, OnNewFinAvailable,
              (Location final_object_in_subgroup, uint64_t subgroup_id),
              (override));
  MOCK_METHOD(void, OnSubgroupAbandoned,
              (uint64_t group, uint64_t subgroup,
               webtransport::StreamErrorCode error_code),
              (override));
  MOCK_METHOD(void, OnGroupAbandoned, (uint64_t group_id), (override));
  MOCK_METHOD(void, OnTrackPublisherGone, (), (override));
};

class MoqtRelayTrackPublisherTest : public quiche::test::QuicheTest {
 public:
  MoqtRelayTrackPublisherTest()
      : session_(std::make_unique<MockMoqtSession>()),
        publisher_(
            kTrackName, session_->GetWeakPtr(),
            [this]() { track_deleted_ = true; }, std::nullopt, std::nullopt,
            std::nullopt) {}

  void SubscribeAndOk() {
    EXPECT_CALL(*session_, SubscribeCurrentObject)
        .WillOnce(testing::Return(true));
    publisher_.AddObjectListener(&listener_);
    EXPECT_CALL(listener_, OnSubscribeAccepted);
    publisher_.OnReply(
        kTrackName,
        SubscribeOkData{quic::QuicTimeDelta::Infinite(),
                        MoqtDeliveryOrder::kAscending, kLargestLocation});
  }

  void ObjectArrives(Location location, uint64_t subgroup,
                     MoqtObjectStatus status, absl::string_view payload,
                     bool fin_after_this = false) {
    EXPECT_CALL(listener_, OnNewObjectAvailable(location, subgroup, 128));
    if (fin_after_this || status == MoqtObjectStatus::kEndOfTrack ||
        status == MoqtObjectStatus::kEndOfGroup) {
      EXPECT_CALL(listener_, OnNewFinAvailable(location, subgroup));
    }
    publisher_.OnObjectFragment(
        kTrackName,
        PublishedObjectMetadata{location, subgroup, "", status, 128}, payload,
        true);
    std::optional<PublishedObject> object =
        publisher_.GetCachedObject(location.group, subgroup, location.object);
    ASSERT_TRUE(object.has_value());
    if (object.has_value()) {
      EXPECT_EQ(object->metadata.location, location);
      EXPECT_EQ(object->metadata.subgroup, subgroup);
      EXPECT_EQ(object->metadata.status, status);
      EXPECT_EQ(object->metadata.publisher_priority, 128);
      EXPECT_EQ(object->payload.AsStringView(), payload);
      EXPECT_EQ(object->fin_after_this, fin_after_this);
    }
  }

  const Location kLargestLocation = Location(3, 2);

  bool track_deleted_ = false;
  std::unique_ptr<MockMoqtSession> session_;
  MockMoqtObjectListener listener_;
  MoqtRelayTrackPublisher publisher_;
};

TEST_F(MoqtRelayTrackPublisherTest, Queries) {
  EXPECT_EQ(publisher_.GetTrackName(), kTrackName);
  EXPECT_EQ(publisher_.largest_location(), std::nullopt);
  EXPECT_EQ(publisher_.forwarding_preference(), std::nullopt);
  EXPECT_EQ(publisher_.delivery_order(), std::nullopt);
  EXPECT_EQ(publisher_.expiration(), std::nullopt);

  SubscribeAndOk();
  EXPECT_EQ(publisher_.largest_location(), kLargestLocation);
  EXPECT_EQ(publisher_.forwarding_preference(), std::nullopt);
  EXPECT_EQ(publisher_.delivery_order(), MoqtDeliveryOrder::kAscending);
  EXPECT_TRUE(publisher_.expiration().has_value() &&
              publisher_.expiration()->IsInfinite());
}

TEST_F(MoqtRelayTrackPublisherTest, FiniteExpiration) {
  EXPECT_CALL(*session_, SubscribeCurrentObject)
      .WillOnce(testing::Return(true));
  publisher_.AddObjectListener(&listener_);
  EXPECT_CALL(listener_, OnSubscribeAccepted);
  publisher_.OnReply(
      kTrackName,
      SubscribeOkData{quic::QuicTimeDelta::FromSeconds(30),
                      MoqtDeliveryOrder::kAscending, kLargestLocation});
  EXPECT_LT(publisher_.expiration(), quic::QuicTimeDelta::FromSeconds(31));
}

// TODO(martinduke): Write a test for track expiration. It will require
// altering private members in publisher_.

TEST_F(MoqtRelayTrackPublisherTest, SubscribeLifeCycle) {
  SubscribeAndOk();
  uint64_t subgroup = 0;
  Location last_location(3, 6);
  std::optional<PublishedObject> object;
  for (Location location = kLargestLocation.Next(); location < last_location;
       location = location.Next()) {
    ObjectArrives(location, subgroup, MoqtObjectStatus::kNormal, "object");
    // Two objects per subgroup.
    if (location.object % 2 == 0) {
      ++subgroup;
    }
  }
  // End of Group object.
  ObjectArrives(last_location, subgroup, MoqtObjectStatus::kEndOfGroup, "",
                true);
  // End of Track object.
  last_location = Location(4, 0);
  subgroup = 0;
  ObjectArrives(last_location, subgroup, MoqtObjectStatus::kEndOfTrack, "",
                true);

  // TODO(martinduke): Gracefully close the subscription.
}

TEST_F(MoqtRelayTrackPublisherTest, GroupAbandoned) {
  SubscribeAndOk();
  for (uint64_t group = kLargestLocation.group + 1;
       group < kLargestLocation.group + 5; ++group) {
    if (group - kLargestLocation.group > 3) {
      EXPECT_CALL(listener_, OnGroupAbandoned(group - 3));
    }
    EXPECT_CALL(listener_, OnNewObjectAvailable(Location(group, 0), 0, 128));
    publisher_.OnObjectFragment(
        kTrackName,
        PublishedObjectMetadata{Location(group, 0), 0, "",
                                MoqtObjectStatus::kEndOfGroup, 128},
        "", true);
  }
}

TEST_F(MoqtRelayTrackPublisherTest, BeyondEndOfTrack) {
  SubscribeAndOk();
  Location location = kLargestLocation.Next();
  ObjectArrives(location, 0, MoqtObjectStatus::kEndOfTrack, "", true);
  EXPECT_FALSE(track_deleted_);
  location = location.Next();
  EXPECT_CALL(listener_, OnTrackPublisherGone);
  EXPECT_CALL(*session_, Unsubscribe(kTrackName));
  publisher_.OnObjectFragment(
      kTrackName,
      PublishedObjectMetadata{location, 0, "", MoqtObjectStatus::kNormal, 128},
      "object", true);
  EXPECT_TRUE(track_deleted_);
}

TEST_F(MoqtRelayTrackPublisherTest, EndOfTrackTooEarly) {
  SubscribeAndOk();
  Location first_location = kLargestLocation.Next();
  Location second_location = first_location.Next();
  ObjectArrives(second_location, 0, MoqtObjectStatus::kNormal, "object", false);
  EXPECT_FALSE(track_deleted_);
  EXPECT_CALL(listener_, OnTrackPublisherGone);
  EXPECT_CALL(*session_, Unsubscribe(kTrackName));
  publisher_.OnObjectFragment(
      kTrackName,
      PublishedObjectMetadata{first_location, 0, "",
                              MoqtObjectStatus::kEndOfTrack, 128},
      "", true);
  EXPECT_TRUE(track_deleted_);
}

TEST_F(MoqtRelayTrackPublisherTest, BeyondEndOfGroup) {
  SubscribeAndOk();
  Location location = kLargestLocation.Next();
  ObjectArrives(location, 0, MoqtObjectStatus::kEndOfGroup, "", true);
  EXPECT_FALSE(track_deleted_);
  location = location.Next();
  EXPECT_CALL(listener_, OnTrackPublisherGone);
  EXPECT_CALL(*session_, Unsubscribe(kTrackName));
  publisher_.OnObjectFragment(
      kTrackName,
      PublishedObjectMetadata{location, 1, "", MoqtObjectStatus::kEndOfGroup,
                              128},
      "object", true);
  EXPECT_TRUE(track_deleted_);
}

TEST_F(MoqtRelayTrackPublisherTest, EndOfGroupTooEarly) {
  SubscribeAndOk();
  Location first_location = kLargestLocation.Next();
  Location second_location = first_location.Next();
  ObjectArrives(second_location, 0, MoqtObjectStatus::kNormal, "object", false);
  EXPECT_FALSE(track_deleted_);
  EXPECT_CALL(listener_, OnTrackPublisherGone);
  EXPECT_CALL(*session_, Unsubscribe(kTrackName));
  publisher_.OnObjectFragment(
      kTrackName,
      PublishedObjectMetadata{first_location, 1, "",
                              MoqtObjectStatus::kEndOfGroup, 128},
      "", true);
  EXPECT_TRUE(track_deleted_);
}

TEST_F(MoqtRelayTrackPublisherTest, PriorityChange) {
  SubscribeAndOk();
  Location location = kLargestLocation.Next();
  ObjectArrives(location, 0, MoqtObjectStatus::kNormal, "object", false);
  EXPECT_FALSE(track_deleted_);
  location = location.Next();
  EXPECT_CALL(listener_, OnTrackPublisherGone);
  EXPECT_CALL(*session_, Unsubscribe(kTrackName));
  publisher_.OnObjectFragment(
      kTrackName,
      PublishedObjectMetadata{location, 0, "", MoqtObjectStatus::kNormal, 200},
      "object", true);
  EXPECT_TRUE(track_deleted_);
}

// TODO(martinduke): Enable this test once the class supports explicit FIN.
#if 0
TEST_F(MoqtRelayTrackPublisherTest, ObjectAfterFin) {
  SubscribeAndOk();
  Location location = kLargestLocation.Next();
  ObjectArrives(location, 0, MoqtObjectStatus::kNormal, "object", true);
  EXPECT_FALSE(track_deleted_);
  location = location.Next();
  EXPECT_CALL(listener_, OnTrackPublisherGone);
  EXPECT_CALL(*session_, Unsubscribe(kTrackName));
  publisher_.OnObjectFragment(
      kTrackName,
      PublishedObjectMetadata{location, 0, "", MoqtObjectStatus::kNormal, 128},
      "object", true);
  EXPECT_TRUE(track_deleted_);
}
#endif

TEST_F(MoqtRelayTrackPublisherTest, ObjectOutOfOrder) {
  SubscribeAndOk();
  Location first_location = kLargestLocation.Next();
  Location second_location = first_location.Next();
  ObjectArrives(second_location, 0, MoqtObjectStatus::kNormal, "object", false);
  EXPECT_FALSE(track_deleted_);
  EXPECT_CALL(listener_, OnNewObjectAvailable).Times(0);
  publisher_.OnObjectFragment(
      kTrackName,
      PublishedObjectMetadata{first_location, 0, "", MoqtObjectStatus::kNormal,
                              128},
      "object", true);
  // Object is simply ignored; track is not malformed.
  EXPECT_FALSE(track_deleted_);
}

TEST_F(MoqtRelayTrackPublisherTest, CacheMisses) {
  SubscribeAndOk();
  Location location = kLargestLocation.Next();
  ObjectArrives(location, 0, MoqtObjectStatus::kNormal, "object", false);
  // Nonexistent group.
  EXPECT_FALSE(
      publisher_.GetCachedObject(location.group + 1, 0, location.object)
          .has_value());
  // Nonexistent subgroup.
  EXPECT_FALSE(publisher_.GetCachedObject(location.group, 1, location.object)
                   .has_value());
  // Object ID too high.
  EXPECT_FALSE(
      publisher_.GetCachedObject(location.group, 0, location.object + 1)
          .has_value());
}

TEST_F(MoqtRelayTrackPublisherTest, SubscribeRejected) {
  EXPECT_CALL(*session_, SubscribeCurrentObject)
      .WillOnce(testing::Return(true));
  publisher_.AddObjectListener(&listener_);
  EXPECT_CALL(listener_, OnSubscribeRejected);
  publisher_.OnReply(
      kTrackName,
      MoqtRequestError{RequestErrorCode::kUnauthorized, "Unauthorized"});
  EXPECT_TRUE(track_deleted_);
}

TEST_F(MoqtRelayTrackPublisherTest, LastListenerGone) {
  EXPECT_CALL(*session_, SubscribeCurrentObject)
      .WillOnce(testing::Return(true));
  publisher_.AddObjectListener(&listener_);
  EXPECT_CALL(*session_, Unsubscribe(kTrackName));
  publisher_.RemoveObjectListener(&listener_);
  EXPECT_TRUE(track_deleted_);
}

TEST_F(MoqtRelayTrackPublisherTest, SessionDies) {
  session_.reset();
  EXPECT_CALL(listener_, OnSubscribeRejected);
  publisher_.AddObjectListener(&listener_);
  EXPECT_TRUE(track_deleted_);
}

TEST_F(MoqtRelayTrackPublisherTest, SecondListenerNoSubscribe) {
  EXPECT_CALL(*session_, SubscribeCurrentObject)
      .WillOnce(testing::Return(true));
  publisher_.AddObjectListener(&listener_);
  EXPECT_CALL(*session_, SubscribeCurrentObject).Times(0);
  publisher_.AddObjectListener(&listener_);
}

TEST_F(MoqtRelayTrackPublisherTest, OnMalformedObject) {
  EXPECT_CALL(*session_, SubscribeCurrentObject)
      .WillOnce(testing::Return(true));
  publisher_.AddObjectListener(&listener_);
  EXPECT_CALL(listener_, OnTrackPublisherGone);
  publisher_.OnMalformedTrack(kTrackName);
  EXPECT_TRUE(track_deleted_);
}

TEST_F(MoqtRelayTrackPublisherTest, Fin) {
  SubscribeAndOk();

  // No stream to FIN.
  EXPECT_CALL(listener_, OnNewFinAvailable).Times(0);
  publisher_.OnStreamFin(kTrackName, DataStreamIndex{2, 0});

  ObjectArrives(Location(4, 0), 0, MoqtObjectStatus::kNormal, "object", false);
  std::optional<PublishedObject> object = publisher_.GetCachedObject(4, 0, 0);
  EXPECT_FALSE(object.has_value() && object->fin_after_this);

  EXPECT_CALL(listener_, OnNewFinAvailable(Location(4, 0), 0));
  publisher_.OnStreamFin(kTrackName, DataStreamIndex{4, 0});
  // Object now has fin_after_this set.
  object = publisher_.GetCachedObject(4, 0, 0);
  EXPECT_TRUE(object.has_value() && object->fin_after_this);
}

TEST_F(MoqtRelayTrackPublisherTest, Reset) {
  SubscribeAndOk();

  EXPECT_CALL(listener_, OnSubgroupAbandoned(2, 0, kResetCodeCanceled));
  publisher_.OnStreamReset(kTrackName, DataStreamIndex{2, 0});
}

}  // namespace

}  // namespace moqt::test
