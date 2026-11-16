// Copyright (c) 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_live_publisher.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "absl/base/casts.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/moqt/moqt_bidi_stream.h"
#include "quiche/quic/moqt/moqt_error.h"
#include "quiche/quic/moqt/moqt_framer.h"
#include "quiche/quic/moqt/moqt_key_value_pair.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_names.h"
#include "quiche/quic/moqt/moqt_object.h"
#include "quiche/quic/moqt/moqt_parser.h"
#include "quiche/quic/moqt/moqt_priority.h"
#include "quiche/quic/moqt/moqt_session_callbacks.h"
#include "quiche/quic/moqt/moqt_session_interface.h"
#include "quiche/quic/moqt/moqt_trace_recorder.h"
#include "quiche/quic/moqt/moqt_types.h"
#include "quiche/quic/moqt/moqt_uni_stream.h"
#include "quiche/quic/moqt/test_tools/mock_moqt_session.h"
#include "quiche/quic/moqt/test_tools/moqt_framer_utils.h"
#include "quiche/quic/moqt/test_tools/moqt_mock_visitor.h"
#include "quiche/quic/moqt/test_tools/moqt_session_peer.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/mock_clock.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/platform/api/quiche_expect_bug.h"
#include "quiche/common/quiche_mem_slice.h"
#include "quiche/web_transport/test_tools/mock_web_transport.h"
#include "quiche/web_transport/web_transport.h"

namespace moqt::test {

class LivePublisherPeer {
 public:
  static size_t num_open_streams(LivePublisher* publisher) {
    return publisher->stream_map_.GetAllStreams().size();
  }
  static std::optional<Location> largest_sent(const LivePublisher* publisher) {
    return publisher->largest_sent_;
  }
  static const absl::flat_hash_set<DataStreamIndex>& reset_subgroups(
      const LivePublisher* publisher) {
    return publisher->reset_subgroups_;
  }
};

namespace {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;
using ::webtransport::DatagramStatus;
using ::webtransport::DatagramStatusCode;

class TestMoqtBidiStream : public MoqtBidiStreamBase {
 public:
  TestMoqtBidiStream(MoqtFramer* absl_nonnull framer,
                     const MoqtControlMessageParser& message_parser,
                     SessionErrorCallback session_error_callback)
      : MoqtBidiStreamBase(framer, message_parser,
                           std::move(session_error_callback)) {}
  ~TestMoqtBidiStream() override = default;
  void OnStreamBound() override {};
  absl::Status OnRawControlMessage(
      const MoqtRawControlMessage& message) override {
    return absl::OkStatus();
  }
  void Detach() override { detached_ = true; }
  using MoqtBidiStreamBase::QueueIncomingUpdate;
  bool detached_ = false;
};

std::optional<PublishedObject> DefaultPublishedObject(
    Location location, std::optional<uint64_t> subgroup,
    MoqtPriority publisher_priority) {
  PublishedObject object;
  object.metadata.location = location;
  object.metadata.subgroup = subgroup;
  object.metadata.status = MoqtObjectStatus::kNormal;
  object.metadata.publisher_priority = publisher_priority;
  object.metadata.properties = "properties";
  object.metadata.first_object_in_subgroup =
      subgroup.has_value() ? std::optional<bool>(location.object == 0)
                           : std::nullopt;
  object.metadata.payload_length = 8;
  object.payload.push_back(quiche::QuicheMemSlice::Copy("deadbeef"));
  return object;
}

class LivePublisherTest : public quic::test::QuicTest {
 public:
  LivePublisherTest()
      : track_publisher_(
            std::make_shared<MockTrackPublisher>(FullTrackName("foo", "bar"))),
        bidi_stream_(&framer_, message_parser_,
                     [](MoqtError, absl::string_view) {}),
        trace_recorder_(nullptr) {
    bidi_stream_.BindStream(&mock_bidi_stream_);
    parameters_.set_forward(true);
    parameters_.object_delivery_timeout = quic::QuicTimeDelta::FromSeconds(1);
    parameters_.subgroup_delivery_timeout = quic::QuicTimeDelta::FromSeconds(2);
    parameters_.group_order = MoqtGroupOrder::kAscending;
    EXPECT_CALL(monitoring_interface_, OnObjectAckSupportKnown)
        .Times(AtLeast(0));
    EXPECT_CALL(visitor_, session).WillRepeatedly(Return(&webtrans_));
    ON_CALL(visitor_, ReleaseMonitoringInterface)
        .WillByDefault(Return(&monitoring_interface_));
    publisher_ = std::make_unique<LivePublisher>(
        framer_, track_publisher_, &bidi_stream_, kRequestId, kTrackAlias,
        parameters_, visitor_.weak_ptr_factory_.Create(),
        /*is_publish=*/false);
    ON_CALL(webtrans_, GetStreamById(kStreamId))
        .WillByDefault(Return(&mock_uni_stream_));
    ON_CALL(visitor_, alarm_factory).WillByDefault(Return(&alarm_factory_));
    ON_CALL(visitor_, clock).WillByDefault(Return(&mock_clock_));
    ON_CALL(visitor_, trace_recorder).WillByDefault(ReturnRef(trace_recorder_));
  }

  ~LivePublisherTest() override {
    if (track_publisher_ == nullptr) {
      return;
    }
    EXPECT_CALL(*track_publisher_, RemoveObjectListener(publisher_.get()));
  }

  MoqtPriority subscriber_priority() const {
    return parameters_.subscriber_priority.value_or(kDefaultSubscriberPriority);
  }

  // Create a stream with the given parameters and send the first object. Will
  // check that the first bytes written to the stream are equal to
  // |opening_bytes|.
  void CreateStream(Location location, uint64_t subgroup,
                    MoqtPriority publisher_priority,
                    std::string opening_bytes = "") {
    EXPECT_CALL(
        *track_publisher_,
        GetCachedObject(location.group, std::make_optional<uint64_t>(subgroup),
                        location.object, 0))
        .WillOnce(Return(  // Once for monitoring interface.
            DefaultPublishedObject(location, subgroup, publisher_priority)))
        .WillOnce(Return(  // To actually deliver the object.
            DefaultPublishedObject(location, subgroup, publisher_priority)));
    EXPECT_CALL(
        *track_publisher_,
        GetCachedObject(location.group, std::make_optional<uint64_t>(subgroup),
                        location.object + 1, 0))
        .WillOnce(Return(std::nullopt));
    // Additional object retrievals will return nullopt.
    EXPECT_CALL(monitoring_interface_, OnNewObjectEnqueued(location));
    EXPECT_CALL(mock_uni_stream_, GetStreamId())
        .WillRepeatedly(Return(kStreamId));
    EXPECT_CALL(webtrans_, CanOpenNextOutgoingUnidirectionalStream())
        .WillOnce(Return(true));
    EXPECT_CALL(webtrans_, OpenOutgoingUnidirectionalStream)
        .WillOnce(Return(&mock_uni_stream_));
    EXPECT_CALL(mock_uni_stream_, SetVisitor)
        .WillOnce([&](std::unique_ptr<webtransport::StreamVisitor> visitor) {
          uni_stream_ = std::move(visitor);
        });
    EXPECT_CALL(mock_uni_stream_, SetPriority);
    ON_CALL(mock_uni_stream_, visitor()).WillByDefault([&]() {
      return uni_stream_.get();
    });
    EXPECT_CALL(mock_uni_stream_, CanWrite()).WillRepeatedly(Return(true));
    EXPECT_CALL(mock_uni_stream_, Writev)
        .WillOnce([&](absl::Span<quiche::QuicheMemSlice> data,
                      const webtransport::StreamWriteOptions& options) {
          EXPECT_TRUE(absl::StartsWith(data[0].AsStringView(), opening_bytes));
          EXPECT_FALSE(options.send_fin());
          return absl::OkStatus();
        });
    publisher_->OnNewObjectAvailable(location, subgroup, publisher_priority);
    ++open_streams_;
  }

  void CreatePendingStream(Location location, uint64_t subgroup,
                           MoqtPriority publisher_priority) {
    EXPECT_CALL(
        *track_publisher_,
        GetCachedObject(location.group, std::make_optional<uint64_t>(subgroup),
                        location.object, 0))
        .WillOnce(Return(  // Once for monitoring interface.
            DefaultPublishedObject(location, subgroup, publisher_priority)));
    // Additional object retrievals will return nullopt.
    EXPECT_CALL(monitoring_interface_, OnNewObjectEnqueued(location));
    EXPECT_CALL(webtrans_, CanOpenNextOutgoingUnidirectionalStream())
        .WillOnce(Return(false));
    EXPECT_CALL(visitor_,
                UpdateTrackPriority(track_publisher_->GetTrackName(), _,
                                    MoqtTrackPriority{subscriber_priority(),
                                                      publisher_priority}));
    publisher_->OnNewObjectAvailable(location, subgroup, publisher_priority);
  }

  static constexpr webtransport::StreamId kStreamId = 100;
  static constexpr uint64_t kTrackAlias = 10;
  static constexpr uint64_t kRequestId = 1;

  MoqtFramer framer_{true, quic::Perspective::IS_CLIENT};
  MoqtControlMessageParser message_parser_{kDefaultMoqtVersion, true,
                                           quic::Perspective::IS_CLIENT};
  webtransport::test::MockSession webtrans_;
  StrictMock<webtransport::test::MockStream> mock_bidi_stream_;
  webtransport::test::MockStream mock_uni_stream_;
  std::shared_ptr<MockTrackPublisher> track_publisher_;
  TestMoqtBidiStream bidi_stream_;
  std::unique_ptr<webtransport::StreamVisitor> uni_stream_;
  MessageParameters parameters_;
  MockSessionToPublisherInterface visitor_;
  StrictMock<MockPublishingMonitorInterface> monitoring_interface_;
  MoqtTraceRecorder trace_recorder_;
  std::unique_ptr<LivePublisher> publisher_;
  const TrackProperties properties_;
  quic::MockClock mock_clock_;
  MoqtSessionCallbacks callbacks_;
  quic::test::TestAlarmFactory alarm_factory_;
  int open_streams_ = 0;
};

TEST_F(LivePublisherTest, OnSubscribeAcceptedNoFilter) {
  EXPECT_CALL(mock_bidi_stream_, CanWrite()).WillRepeatedly(Return(true));
  EXPECT_CALL(*track_publisher_, largest_location())
      .WillOnce(Return(Location(1, 2)));
  EXPECT_CALL(*track_publisher_, expiration)
      .WillOnce(Return(quic::QuicTimeDelta::FromSeconds(10)));
  EXPECT_CALL(*track_publisher_, properties)
      .WillRepeatedly(ReturnRef(properties_));
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kSubscribeOk), _))
      .WillOnce(Return(absl::OkStatus()));
  publisher_->OnSubscribeAccepted();
  EXPECT_TRUE(publisher_->largest_object_known());
  EXPECT_EQ(publisher_->parameters().largest_object, Location(1, 2));
  EXPECT_FALSE(publisher_->parameters().location_filter.has_value());
}

TEST_F(LivePublisherTest, OnSubscribeAcceptedWithFilter) {
  publisher_->parameters().location_filter = LocationFilter::NextObjectFilter();
  const TrackProperties properties(std::nullopt, std::nullopt, std::nullopt,
                                   /*default_publisher_priority=*/64,
                                   std::nullopt, std::nullopt, std::nullopt);
  EXPECT_CALL(mock_bidi_stream_, CanWrite()).WillRepeatedly(Return(true));
  EXPECT_CALL(*track_publisher_, largest_location())
      .WillOnce(Return(Location(1, 2)));
  EXPECT_CALL(*track_publisher_, expiration)
      .WillOnce(Return(quic::QuicTimeDelta::FromSeconds(10)));
  EXPECT_CALL(*track_publisher_, properties)
      .WillRepeatedly(ReturnRef(properties));
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kSubscribeOk), _))
      .WillOnce(Return(absl::OkStatus()));
  publisher_->OnSubscribeAccepted();
  ASSERT_TRUE(publisher_->parameters().location_filter.has_value());
  EXPECT_EQ(publisher_->parameters().location_filter->start(), Location(1, 3));
  // Check that default_publisher_priority is set. A datagram set at priority
  // 64 should not explicitly encode that.
  EXPECT_CALL(*track_publisher_,
              GetCachedObject(1, std::optional<uint64_t>(), 3, 0))
      .WillOnce(
          Return(DefaultPublishedObject(Location(1, 3), std::nullopt, 64)))
      .WillOnce(
          Return(DefaultPublishedObject(Location(1, 3), std::nullopt, 64)));
  EXPECT_CALL(monitoring_interface_, OnNewObjectEnqueued(Location(1, 3)));
  EXPECT_CALL(webtrans_, SendOrQueueDatagram)
      .WillOnce([](absl::string_view datagram) {
        EXPECT_FALSE(datagram.empty());
        std::optional<MoqtDatagramType> type =
            MoqtDatagramType::FromValue(static_cast<uint64_t>(datagram[0]));
        EXPECT_TRUE(type.has_value() && type->has_default_priority());
        return DatagramStatus(DatagramStatusCode::kSuccess, "");
      });
  publisher_->OnNewObjectAvailable(Location(1, 3), std::nullopt, 64);
}

TEST_F(LivePublisherTest, OnSubscribeAcceptedWithQueuedUpdates) {
  MessageParameters update1;
  update1.object_delivery_timeout = quic::QuicTimeDelta::FromSeconds(5);
  update1.set_forward(false);
  bidi_stream_.QueueIncomingUpdate(update1);

  MessageParameters update2;
  update2.set_forward(true);
  update2.location_filter = LocationFilter::NextObjectFilter();
  bidi_stream_.QueueIncomingUpdate(update2);

  EXPECT_CALL(mock_bidi_stream_, CanWrite()).WillRepeatedly(Return(true));
  EXPECT_CALL(*track_publisher_, largest_location())
      .WillOnce(Return(std::nullopt))
      .WillOnce(Return(Location(1, 2)))
      .WillOnce(Return(Location(2, 5)));
  EXPECT_CALL(*track_publisher_, expiration)
      .WillOnce(Return(quic::QuicTimeDelta::FromSeconds(10)));
  EXPECT_CALL(*track_publisher_, properties)
      .WillRepeatedly(ReturnRef(properties_));
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kSubscribeOk), _))
      .WillOnce(Return(absl::OkStatus()));
  MessageParameters expected_ok;
  expected_ok.largest_object = Location(1, 2);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(MoqtRequestOk(expected_ok)), _))
      .WillOnce(Return(absl::OkStatus()));
  expected_ok.largest_object = Location(2, 5);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(MoqtRequestOk(expected_ok)), _))
      .WillOnce(Return(absl::OkStatus()));

  publisher_->OnSubscribeAccepted();
  EXPECT_TRUE(publisher_->largest_object_known());
  EXPECT_EQ(publisher_->parameters().object_delivery_timeout,
            quic::QuicTimeDelta::FromSeconds(5));
  EXPECT_TRUE(publisher_->parameters().forward());
  EXPECT_EQ(publisher_->parameters().largest_object, Location(2, 5));
  ASSERT_TRUE(publisher_->parameters().location_filter.has_value());
  EXPECT_EQ(publisher_->parameters().location_filter->start(), Location(2, 6));
}

TEST_F(LivePublisherTest, OnSubscribeRejected) {
  EXPECT_CALL(mock_bidi_stream_, CanWrite()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kRequestError), _))
      .WillOnce(Return(absl::OkStatus()));
  publisher_->OnSubscribeRejected(MoqtRequestErrorInfo(
      RequestErrorCode::kDoesNotExist, std::nullopt, "reason"));
}

TEST_F(LivePublisherTest, Update) {
  MessageParameters new_params;
  new_params.object_delivery_timeout = quic::QuicTimeDelta::FromSeconds(5);
  EXPECT_CALL(mock_bidi_stream_, CanWrite()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(MoqtRequestOk()), _))
      .WillOnce(Return(absl::OkStatus()));
  publisher_->Update(new_params, false);
  EXPECT_EQ(publisher_->parameters().object_delivery_timeout,
            quic::QuicTimeDelta::FromSeconds(5));

  // Changing forward preference updates can_have_joining_fetch_
  new_params.set_forward(false);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(MoqtRequestOk()), _))
      .WillOnce(Return(absl::OkStatus()));
  publisher_->Update(new_params, false);
  EXPECT_FALSE(publisher_->parameters().forward());
  EXPECT_FALSE(publisher_->can_have_joining_fetch());

  // Setting a NextObject filter while forward=false, then turning forward=
  // true initializes the filter using the latest largest_location.
  publisher_->parameters().location_filter = LocationFilter::NextObjectFilter();
  MessageParameters forward_on_params;
  forward_on_params.set_forward(true);
  EXPECT_CALL(*track_publisher_, largest_location())
      .WillOnce(Return(Location(2, 4)));
  MessageParameters expected_ok;
  expected_ok.largest_object = Location(2, 4);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(MoqtRequestOk{expected_ok}), _))
      .WillOnce(Return(absl::OkStatus()));
  publisher_->Update(forward_on_params, false);
  EXPECT_TRUE(publisher_->parameters().forward());
  EXPECT_TRUE(publisher_->can_have_joining_fetch());
  EXPECT_EQ(publisher_->parameters().largest_object, Location(2, 4));
  ASSERT_TRUE(publisher_->parameters().location_filter.has_value());
  EXPECT_EQ(publisher_->parameters().location_filter->start(), Location(2, 5));
}

TEST_F(LivePublisherTest, UpdateLocationFilterAndEndGroup) {
  EXPECT_CALL(mock_bidi_stream_, CanWrite()).WillRepeatedly(Return(true));

  // Adding a new kNextObject filter when forward is already true initializes
  // the filter and sends LARGEST_OBJECT in REQUEST_OK.
  MessageParameters filter_params;
  filter_params.location_filter = LocationFilter::NextObjectFilter();
  EXPECT_CALL(*track_publisher_, largest_location())
      .WillOnce(Return(Location(3, 1)));
  MessageParameters expected_ok1;
  expected_ok1.largest_object = Location(3, 1);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(MoqtRequestOk{expected_ok1}), _))
      .WillOnce(Return(absl::OkStatus()));
  publisher_->Update(filter_params, false);
  ASSERT_TRUE(publisher_->parameters().location_filter.has_value());
  EXPECT_EQ(publisher_->parameters().location_filter->start(), Location(3, 2));

  // Set an AbsoluteStartGroupEnd filter and then extend end_group.
  publisher_->parameters().location_filter =
      LocationFilter::AbsoluteStartGroupEndFilter(Location(1, 0), 5);
  MessageParameters extend_params;
  extend_params.location_filter =
      LocationFilter::AbsoluteStartGroupEndFilter(Location(1, 0), 8);
  EXPECT_CALL(*track_publisher_, largest_location())
      .WillOnce(Return(Location(4, 2)));
  MessageParameters expected_ok2;
  expected_ok2.largest_object = Location(4, 2);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(MoqtRequestOk{expected_ok2}), _))
      .WillOnce(Return(absl::OkStatus()));
  publisher_->Update(extend_params, false);
  EXPECT_EQ(publisher_->parameters().location_filter->end_group(), 8);
  EXPECT_EQ(publisher_->parameters().largest_object, Location(4, 2));
}

TEST_F(LivePublisherTest, UpdateRejectedByPublisher) {
  MessageParameters new_params;
  new_params.object_delivery_timeout = quic::QuicTimeDelta::FromSeconds(5);
  EXPECT_CALL(mock_bidi_stream_, CanWrite()).WillRepeatedly(Return(true));

  // When from_request_ok is false, sends PUBLISH_DONE (no FIN) followed by
  // REQUEST_ERROR (with FIN).
  EXPECT_CALL(*track_publisher_, UpdateObjectListener(publisher_.get(), _))
      .WillOnce(Return(absl::InvalidArgumentError("invalid update")));
  {
    testing::InSequence seq;
    MoqtPublishDone expected_publish_done = {
        PublishDoneCode::kUpdateFailed,
        /*stream_count=*/0,
        /*error_reason=*/"Update failed",
    };
    EXPECT_CALL(mock_bidi_stream_,
                Writev(SerializedControlMessage(expected_publish_done), _))
        .WillOnce([](absl::Span<quiche::QuicheMemSlice> data,
                     const webtransport::StreamWriteOptions& options) {
          EXPECT_FALSE(options.send_fin());
          return absl::OkStatus();
        });
    EXPECT_CALL(mock_bidi_stream_,
                Writev(ControlMessageOfType(MoqtMessageType::kRequestError), _))
        .WillOnce([](absl::Span<quiche::QuicheMemSlice> data,
                     const webtransport::StreamWriteOptions& options) {
          EXPECT_TRUE(options.send_fin());
          return absl::OkStatus();
        });
  }
  publisher_->Update(new_params, /*from_request_ok=*/false);
  EXPECT_EQ(publisher_->parameters().object_delivery_timeout,
            quic::QuicTimeDelta::FromSeconds(1));

  // When from_request_ok is true, resets the bidi stream.
  EXPECT_CALL(*track_publisher_, UpdateObjectListener(publisher_.get(), _))
      .WillOnce(Return(absl::InvalidArgumentError("invalid update")));
  EXPECT_CALL(mock_bidi_stream_, ResetWithUserCode);
  EXPECT_CALL(mock_bidi_stream_, SendStopSending);
  publisher_->Update(new_params, /*from_request_ok=*/true);
}

TEST_F(LivePublisherTest, UpdatePriorityNoStreams) {
  MessageParameters new_params;
  new_params.subscriber_priority = 20;
  publisher_->Update(new_params, true);
  EXPECT_EQ(publisher_->parameters().subscriber_priority, 20);
}

TEST_F(LivePublisherTest, UpdatePriorityWithPendingStreams) {
  CreatePendingStream(Location(1, 0), 0, 64);
  MessageParameters new_params;
  new_params.subscriber_priority = 20;
  EXPECT_CALL(*track_publisher_, properties())
      .WillRepeatedly(ReturnRef(properties_));
  EXPECT_CALL(visitor_, UpdateTrackPriority(track_publisher_->GetTrackName(),
                                            std::optional<MoqtTrackPriority>(
                                                {subscriber_priority(), 64}),
                                            MoqtTrackPriority{20, 64}));
  EXPECT_CALL(mock_bidi_stream_, CanWrite()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(MoqtRequestOk()), _))
      .WillOnce(Return(absl::OkStatus()));
  publisher_->Update(new_params, false);
}

TEST_F(LivePublisherTest, UpdatePriorityWithActiveStreams) {
  CreateStream(
      Location(1, 0), 0, 127,
      {0x51, static_cast<uint8_t>(kTrackAlias), 0x01, 0x7f, 0x00, 0x0a});
  MessageParameters new_params;
  new_params.subscriber_priority = 20;
  EXPECT_CALL(mock_uni_stream_, SetPriority);
  EXPECT_CALL(mock_bidi_stream_, CanWrite()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(MoqtRequestOk()), _))
      .WillOnce(Return(absl::OkStatus()));
  publisher_->Update(new_params, false);
}

TEST_F(LivePublisherTest, OnNewObjectAvailableNotInWindow) {
  MessageParameters params;
  params.location_filter =
      LocationFilter::AbsoluteStartGroupEndFilter(Location(10, 0), 10);
  publisher_->Update(params, true);
  EXPECT_CALL(*track_publisher_, GetCachedObject).Times(0);
  publisher_->OnNewObjectAvailable(Location(5, 0), 0, 128);
}

TEST_F(LivePublisherTest, OnNewObjectAvailableDatagram) {
  EXPECT_CALL(*track_publisher_,
              GetCachedObject(1, std::optional<uint64_t>(), 0, 0))
      .WillOnce(
          Return(DefaultPublishedObject(Location(1, 0), std::nullopt, 128)))
      .WillOnce(
          Return(DefaultPublishedObject(Location(1, 0), std::nullopt, 128)));
  EXPECT_CALL(monitoring_interface_, OnNewObjectEnqueued(Location(1, 0)));
  EXPECT_CALL(webtrans_, SendOrQueueDatagram)
      .WillOnce(Return(DatagramStatus(DatagramStatusCode::kSuccess, "")));
  EXPECT_CALL(*track_publisher_, properties())
      .WillRepeatedly(ReturnRef(properties_));
  publisher_->OnNewObjectAvailable(Location(1, 0), std::nullopt, 128);
}

TEST_F(LivePublisherTest, OnNewObjectAvailableStatusDatagram) {
  PublishedObjectMetadata metadata{Location(1, 0),
                                   /*subgroup=*/std::nullopt,
                                   /*properties=*/"",
                                   MoqtObjectStatus::kEndOfGroup,
                                   /*publisher_priority=*/128,
                                   /*first_object_in_subgroup=*/std::nullopt,
                                   /*payload_length=*/0};
  EXPECT_CALL(*track_publisher_,
              GetCachedObject(1, std::optional<uint64_t>(), 0, 0))
      .WillOnce(Return(PublishedObject{metadata, {}, false}))
      .WillOnce(Return(PublishedObject{metadata, {}, false}));
  EXPECT_CALL(webtrans_, SendOrQueueDatagram)
      .WillOnce([](absl::string_view datagram) {
        MoqtObject message;
        bool use_default_priority;
        absl::StatusOr<absl::string_view> payload =
            ParseDatagram(datagram, message, use_default_priority);
        EXPECT_TRUE(payload.ok() && payload->empty());
        EXPECT_EQ(message.object_status, MoqtObjectStatus::kEndOfGroup);
        return DatagramStatus(DatagramStatusCode::kSuccess, "");
      });
  publisher_->OnNewObjectAvailable(Location(1, 0), std::nullopt, 128);
}

TEST_F(LivePublisherTest, OnNewObjectAvailableStreamCreationBlocked) {
  CreatePendingStream(Location(1, 0), 0, 128);
}

TEST_F(LivePublisherTest, OnNewFinAvailableNoops) {
  // Not in window
  MessageParameters params;
  params.location_filter =
      LocationFilter::AbsoluteStartGroupEndFilter(Location(10, 0), 10);
  publisher_->Update(params, true);
  EXPECT_CALL(webtrans_, GetStreamById).Times(0);
  EXPECT_CALL(mock_uni_stream_, Writev).Times(0);
  publisher_->OnNewFinAvailable(Location(0, 0), 0);

  // In window but no stream
  EXPECT_CALL(mock_bidi_stream_, CanWrite()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(MoqtRequestOk()), _))
      .WillOnce(Return(absl::OkStatus()));
  publisher_->Update(parameters_, false);
  EXPECT_CALL(webtrans_, GetStreamById).Times(0);
  EXPECT_CALL(mock_uni_stream_, Writev).Times(0);
  publisher_->OnNewFinAvailable(Location(10, 10), 0);

  EXPECT_CALL(webtrans_, GetStreamById)
      .WillRepeatedly(Return(&mock_uni_stream_));
  // Stream hasn't gotten there yet. The cache will tell us when to send FIN.
  CreateStream(Location(10, 0), 0, 128);
  EXPECT_CALL(mock_uni_stream_, Writev).Times(0);
  publisher_->OnNewFinAvailable(Location(10, 1), 0);
}

TEST_F(LivePublisherTest, OnNewFinAvailableWithStream) {
  CreateStream(Location(1, 0), 0, 128);
  EXPECT_CALL(mock_uni_stream_, Writev)
      .WillOnce([](absl::Span<quiche::QuicheMemSlice> data,
                   const webtransport::StreamWriteOptions& options) {
        EXPECT_TRUE(data.empty());
        EXPECT_TRUE(options.send_fin());
        return absl::OkStatus();
      });
  quic::test::MockAlarmFactory alarm_factory;
  EXPECT_CALL(visitor_, alarm_factory).WillOnce(Return(&alarm_factory));
  publisher_->OnNewFinAvailable(Location(1, 0), 0);
}

TEST_F(LivePublisherTest, OnSubgroupAbandonedNoEffect) {
  // Not in window
  MessageParameters params;
  params.location_filter =
      LocationFilter::AbsoluteStartGroupEndFilter(Location(10, 0), 10);
  publisher_->Update(params, true);
  EXPECT_CALL(webtrans_, GetStreamById).Times(0);
  publisher_->OnSubgroupAbandoned(1, 0, 17);

  // In window but no stream
  EXPECT_CALL(mock_bidi_stream_, CanWrite()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(MoqtRequestOk()), _))
      .WillOnce(Return(absl::OkStatus()));
  publisher_->Update(parameters_, false);
  EXPECT_CALL(webtrans_, GetStreamById).Times(0);
  publisher_->OnSubgroupAbandoned(1, 0, 17);
}

TEST_F(LivePublisherTest, OnGroupAbandoned) {
  // Not in window
  MessageParameters params;
  params.location_filter =
      LocationFilter::AbsoluteStartGroupEndFilter(Location(10, 0), 10);
  publisher_->Update(params, true);
  EXPECT_CALL(webtrans_, GetStreamById).Times(0);
  publisher_->OnGroupAbandoned(1);

  // In window
  EXPECT_CALL(mock_bidi_stream_, CanWrite()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(MoqtRequestOk()), _))
      .WillOnce(Return(absl::OkStatus()));
  publisher_->Update(parameters_, false);
  EXPECT_CALL(webtrans_, GetStreamById).Times(0);
  publisher_->OnGroupAbandoned(1);
  EXPECT_CALL(*track_publisher_, GetCachedObject).Times(0);
  publisher_->OnNewObjectAvailable(Location(1, 0), 0, 128);
}

TEST_F(LivePublisherTest, OnGroupAbandonedWithStreams) {
  // The delivery timeout is not infinite, so it will not send a PUBLISH_DONE
  // with kTooFarBehind.
  CreateStream(Location(1, 0), 0, 128);
  EXPECT_CALL(mock_uni_stream_, ResetWithUserCode);
  EXPECT_CALL(mock_bidi_stream_, Writev).Times(0);  // No PUBLISH_DONE.
  publisher_->OnGroupAbandoned(1);
}

TEST_F(LivePublisherTest, OnGroupAbandonedTooFarBehind) {
  // Set the delivery timeout to infinite so that TooFarBehind is possible.
  parameters_.object_delivery_timeout = quic::QuicTimeDelta::Infinite();
  parameters_.subgroup_delivery_timeout = quic::QuicTimeDelta::Infinite();
  publisher_->Update(parameters_, true);
  CreateStream(Location(5, 0), 0, 128);
  struct MoqtPublishDone expected_publish_done = {
      PublishDoneCode::kTooFarBehind,
      /*stream_count=*/1,
      /*error_reason=*/"",
  };
  EXPECT_CALL(mock_bidi_stream_, CanWrite()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(expected_publish_done), _))
      .WillOnce([&](absl::Span<quiche::QuicheMemSlice> data,
                    const webtransport::StreamWriteOptions& options) {
        EXPECT_TRUE(options.send_fin());
        return absl::OkStatus();
      });
  EXPECT_CALL(*track_publisher_, RemoveObjectListener);
  publisher_->OnGroupAbandoned(5);
  track_publisher_ = nullptr;
}

TEST_F(LivePublisherTest, OnCanCreateNewUniStreamPendingCleanup) {
  CreatePendingStream(Location(1, 0), 0, 128);
  // Abandon the group.
  publisher_->OnGroupAbandoned(1);
  // OnCanCreateNewUniStream should clean it up; no attempt to create a stream.
  EXPECT_CALL(webtrans_, CanOpenNextOutgoingUnidirectionalStream)
      .WillOnce(Return(true));
  EXPECT_CALL(webtrans_, OpenOutgoingUnidirectionalStream).Times(0);
  publisher_->OnCanCreateNewUniStream();
}

TEST_F(LivePublisherTest, SubgroupDeliveryTimeoutSetAlarm) {
  // Create a stream for group 1.
  CreateStream(Location(1, 0), 0, 128);
  // Create a pending stream for group 2, which should start the timer but does
  // less work than an active stream.
  EXPECT_CALL(visitor_, alarm_factory).WillOnce(Return(&alarm_factory_));
  publisher_->OnNewFinAvailable(Location(1, 7), 0);
}

TEST_F(LivePublisherTest, OnTrackPublisherGone) {
  EXPECT_CALL(mock_bidi_stream_, CanWrite()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kPublishDone), _))
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(*track_publisher_, RemoveObjectListener);
  publisher_->OnTrackPublisherGone();
  track_publisher_ = nullptr;
}

TEST_F(LivePublisherTest, ProcessObjectAck) {
  MoqtObjectAck ack;
  ack.group_id = 1;
  ack.object_id = 2;
  ack.delta_from_deadline = quic::QuicTimeDelta::FromMilliseconds(100);
  EXPECT_CALL(monitoring_interface_,
              OnObjectAckReceived(Location(1, 2), ack.delta_from_deadline));
  publisher_->ProcessObjectAck(ack);
}

TEST_F(LivePublisherTest, OnSubgroupAbandonedWithStream) {
  CreateStream(Location(1, 0), 0, 128);
  EXPECT_CALL(mock_uni_stream_, ResetWithUserCode(17));
  publisher_->OnSubgroupAbandoned(1, 0, 17);
}

TEST_F(LivePublisherTest, OnCanCreateNewUniStreamSuccess) {
  CreatePendingStream(Location(1, 0), 0, 128);
  // Call OnCanCreateNewUniStream and succeed.
  EXPECT_CALL(mock_uni_stream_, GetStreamId())
      .WillRepeatedly(Return(kStreamId));
  EXPECT_CALL(webtrans_, CanOpenNextOutgoingUnidirectionalStream())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(webtrans_, OpenOutgoingUnidirectionalStream)
      .WillOnce(Return(&mock_uni_stream_));
  EXPECT_CALL(mock_uni_stream_, SetVisitor)
      .WillOnce([&](std::unique_ptr<webtransport::StreamVisitor> visitor) {
        uni_stream_ = std::move(visitor);
      });
  EXPECT_CALL(mock_uni_stream_, SetPriority);
  EXPECT_CALL(mock_uni_stream_, visitor()).WillRepeatedly([&]() {
    return uni_stream_.get();
  });
  EXPECT_CALL(mock_uni_stream_, CanWrite()).WillRepeatedly(Return(true));
  EXPECT_CALL(*track_publisher_,
              GetCachedObject(1, std::optional<uint64_t>(0), 0, 0))
      .WillOnce(Return(DefaultPublishedObject(Location(1, 0), 0, 128)));
  EXPECT_CALL(*track_publisher_,
              GetCachedObject(1, std::optional<uint64_t>(0), 1, 0))
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(mock_uni_stream_, Writev).WillOnce(Return(absl::OkStatus()));
  publisher_->OnCanCreateNewUniStream();
}

TEST_F(LivePublisherTest, PendingStreamsInOrder) {
  CreatePendingStream(Location(1, 0), 0, 128);
  CreatePendingStream(Location(0, 0), 0, 128);
  CreatePendingStream(Location(2, 0), 0, 127);
  // Should be opened in the order (2, 0), (0, 0), (1, 0),
  // Open stream and send (2, 0).
  EXPECT_CALL(webtrans_, CanOpenNextOutgoingUnidirectionalStream())
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(webtrans_, OpenOutgoingUnidirectionalStream)
      .WillOnce(Return(&mock_uni_stream_));
  EXPECT_CALL(mock_uni_stream_, GetStreamId).WillRepeatedly(Return(kStreamId));
  std::unique_ptr<webtransport::StreamVisitor> stream_visitor2;
  EXPECT_CALL(mock_uni_stream_, SetVisitor)
      .WillOnce([&](std::unique_ptr<webtransport::StreamVisitor> visitor) {
        stream_visitor2 = std::move(visitor);
      });
  EXPECT_CALL(mock_uni_stream_, visitor()).WillRepeatedly([&]() {
    return stream_visitor2.get();
  });
  EXPECT_CALL(mock_uni_stream_, SetPriority);
  EXPECT_CALL(*track_publisher_,
              GetCachedObject(2, std::optional<uint64_t>(0), 0, 0))
      .WillOnce(Return(DefaultPublishedObject(Location(2, 0), 0, 127)));
  EXPECT_CALL(*track_publisher_,
              GetCachedObject(2, std::optional<uint64_t>(0), 1, 0))
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(mock_uni_stream_, CanWrite).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_uni_stream_, Writev).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(visitor_, UpdateTrackPriority);
  publisher_->OnCanCreateNewUniStream();
  // Open (0, 0)
  EXPECT_CALL(webtrans_, CanOpenNextOutgoingUnidirectionalStream())
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(webtrans_, OpenOutgoingUnidirectionalStream)
      .WillOnce(Return(&mock_uni_stream_));
  EXPECT_CALL(mock_uni_stream_, GetStreamId)
      .WillRepeatedly(Return(kStreamId + 4));
  std::unique_ptr<webtransport::StreamVisitor> stream_visitor0;
  EXPECT_CALL(mock_uni_stream_, SetVisitor)
      .WillOnce([&](std::unique_ptr<webtransport::StreamVisitor> visitor) {
        stream_visitor0 = std::move(visitor);
      });
  EXPECT_CALL(mock_uni_stream_, visitor()).WillRepeatedly([&]() {
    return stream_visitor0.get();
  });
  EXPECT_CALL(mock_uni_stream_, SetPriority);
  EXPECT_CALL(*track_publisher_,
              GetCachedObject(0, std::optional<uint64_t>(0), 0, 0))
      .WillOnce(Return(DefaultPublishedObject(Location(0, 0), 0, 128)));
  EXPECT_CALL(*track_publisher_,
              GetCachedObject(0, std::optional<uint64_t>(0), 1, 0))
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(mock_uni_stream_, CanWrite).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_uni_stream_, Writev).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(visitor_, UpdateTrackPriority);
  publisher_->OnCanCreateNewUniStream();
  // Open (1, 0)
  EXPECT_CALL(webtrans_, CanOpenNextOutgoingUnidirectionalStream())
      .WillRepeatedly(Return(true));  // Unlimited credit but only one stream.
  EXPECT_CALL(webtrans_, OpenOutgoingUnidirectionalStream)
      .WillOnce(Return(&mock_uni_stream_));
  EXPECT_CALL(mock_uni_stream_, GetStreamId)
      .WillRepeatedly(Return(kStreamId + 8));
  std::unique_ptr<webtransport::StreamVisitor> stream_visitor1;
  EXPECT_CALL(mock_uni_stream_, SetVisitor)
      .WillOnce([&](std::unique_ptr<webtransport::StreamVisitor> visitor) {
        stream_visitor1 = std::move(visitor);
      });
  EXPECT_CALL(mock_uni_stream_, visitor()).WillRepeatedly([&]() {
    return stream_visitor1.get();
  });
  EXPECT_CALL(mock_uni_stream_, SetPriority);
  EXPECT_CALL(*track_publisher_,
              GetCachedObject(1, std::optional<uint64_t>(0), 0, 0))
      .WillOnce(Return(DefaultPublishedObject(Location(1, 0), 0, 128)));
  EXPECT_CALL(*track_publisher_,
              GetCachedObject(1, std::optional<uint64_t>(0), 1, 0))
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(mock_uni_stream_, CanWrite).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_uni_stream_, Writev).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(visitor_, UpdateTrackPriority).Times(0);
  publisher_->OnCanCreateNewUniStream();
}

TEST_F(LivePublisherTest, OnDataStreamDestroyed) {
  CreateStream(Location(1, 0), 0, 128);
  DataStreamIndex index(1, 0);
  publisher_->OnDataStreamDestroyed(index);
  // No entries in the stream map.
  EXPECT_CALL(webtrans_, GetStreamById).Times(0);
  parameters_.subscriber_priority = 20;
  EXPECT_CALL(mock_bidi_stream_, CanWrite()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(MoqtRequestOk()), _))
      .WillOnce(Return(absl::OkStatus()));
  publisher_->Update(parameters_, false);
}

TEST_F(LivePublisherTest, OnObjectSentTwice) {
  publisher_->OnObjectSent(Location(1, 0));
  EXPECT_TRUE(LivePublisherPeer::largest_sent(publisher_.get()).has_value() &&
              *LivePublisherPeer::largest_sent(publisher_.get()) ==
                  Location(1, 0));
}

TEST_F(LivePublisherTest, SubgroupDeliveryTimeout) {
  CreateStream(Location(0, 0), 0, 128);
  // Timers aren't running.
  EXPECT_EQ(OutgoingSubgroupStreamPeer::GetAlarm(
                absl::down_cast<OutgoingSubgroupStream*>(uni_stream_.get())),
            nullptr);
  // FIN starts the timer.
  EXPECT_CALL(mock_uni_stream_, visitor)
      .WillOnce(Return(uni_stream_.get()))
      .WillRepeatedly([&]() { return uni_stream_.get(); });
  publisher_->OnNewFinAvailable(Location(0, 7), 0);
  // Group 0 streams now have a timer running.
  EXPECT_NE(OutgoingSubgroupStreamPeer::GetAlarm(
                absl::down_cast<OutgoingSubgroupStream*>(uni_stream_.get())),
            nullptr);
}

TEST_F(LivePublisherTest, IncomingUpdateTruncatesSubscription) {
  // Track gets to Group 5.
  CreateStream(Location(5, 0), 0, 128);
  parameters_.location_filter =
      LocationFilter::AbsoluteStartGroupEndFilter(Location(0, 0), 4);
  EXPECT_CALL(mock_bidi_stream_, CanWrite()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(MoqtRequestOk()), _))
      .WillOnce(Return(absl::OkStatus()));
  publisher_->Update(parameters_, false);
  EXPECT_CALL(*track_publisher_, GetCachedObject).Times(0);
  publisher_->OnNewObjectAvailable(Location(5, 1), 0, 128);
}

TEST_F(LivePublisherTest, OnNewFinAvailable) {
  CreateStream(
      Location(1, 0), 0, 127,
      {0x51, static_cast<uint8_t>(kTrackAlias), 0x01, 0x7f, 0x00, 0x0a});
  EXPECT_CALL(mock_uni_stream_, Writev(testing::IsEmpty(), _))
      .WillOnce([](absl::Span<quiche::QuicheMemSlice> data,
                   const webtransport::StreamWriteOptions& options) {
        EXPECT_TRUE(options.send_fin());
        return absl::OkStatus();
      });
  publisher_->OnNewFinAvailable(Location(1, 0), 0);
}

TEST_F(LivePublisherTest, OnSubgroupAbandoned) {
  CreateStream(
      Location(1, 0), 0, 127,
      {0x51, static_cast<uint8_t>(kTrackAlias), 0x01, 0x7f, 0x00, 0x0a});
  EXPECT_CALL(mock_uni_stream_, ResetWithUserCode(1234));
  publisher_->OnSubgroupAbandoned(1, 0, 1234);
}

TEST_F(LivePublisherTest, OnSubgroupAbandonedOutsideWindow) {
  parameters_.location_filter =
      LocationFilter::AbsoluteStartFilter(Location(20, 0));
  publisher_->Update(parameters_, true);
  EXPECT_CALL(mock_uni_stream_, ResetWithUserCode).Times(0);
  publisher_->OnSubgroupAbandoned(1, 0, 1234);
}

// Repro for b/564069875. A relay could abandon a group and then receive a reset
// for a stream in that group.
TEST_F(LivePublisherTest, OnSubgroupAbandonedAfterGroupAbandoned) {
  CreateStream(Location(1, 0), 0, 128);
  EXPECT_CALL(mock_uni_stream_, ResetWithUserCode(kResetCodeDeliveryTimeout))
      .WillOnce([&](webtransport::StreamErrorCode) { uni_stream_.reset(); });
  publisher_->OnGroupAbandoned(1);
  publisher_->OnSubgroupAbandoned(1, 0, 1234);
}

// Repro for b/565145319. A new object arrives while the session is closing.
TEST_F(LivePublisherTest, OnNewObjectAvailableSessionClosing) {
  CreateStream(Location(1, 0), 0, 128);
  // When MoqtSession::is_closing_ is true, session() returns nullptr.
  EXPECT_CALL(visitor_, session).WillRepeatedly(Return(nullptr));
  EXPECT_CALL(*track_publisher_,
              GetCachedObject(1, std::make_optional<uint64_t>(0), 1, 0))
      .Times(testing::AtMost(1))
      .WillOnce(Return(DefaultPublishedObject(Location(1, 1), 0, 128)));
  EXPECT_CALL(monitoring_interface_, OnNewObjectEnqueued(Location(1, 1)))
      .Times(testing::AtMost(1));
  publisher_->OnNewObjectAvailable(Location(1, 1), 0, 128);
}

TEST_F(LivePublisherTest, PublishPropertiesAndTimeouts) {
  TrackProperties properties(
      /*object_delivery_timeout=*/quic::QuicTimeDelta::FromMilliseconds(250),
      /*max_cache_duration=*/std::nullopt,
      /*subgroup_delivery_timeout=*/quic::QuicTimeDelta::FromMilliseconds(500),
      /*publisher_priority=*/42,
      /*group_order=*/MoqtGroupOrder::kDescending,
      /*dynamic_groups=*/std::nullopt,
      /*immutable_properties=*/std::nullopt);
  EXPECT_CALL(*track_publisher_, properties())
      .WillRepeatedly(ReturnRef(properties));

  MessageParameters publish_params;
  publish_params.set_forward(true);
  LivePublisher publish_publisher(framer_, track_publisher_, &bidi_stream_,
                                  kRequestId, kTrackAlias, publish_params,
                                  visitor_.weak_ptr_factory_.Create(),
                                  /*is_publish=*/true);
  EXPECT_EQ(publish_publisher.parameters().group_order,
            MoqtGroupOrder::kDescending);

  // OnSubscribeAccepted initializes publisher delivery timeouts and default
  // priority without sending SUBSCRIBE_OK since largest_object_known_ is
  // already true.
  EXPECT_CALL(mock_bidi_stream_, Writev).Times(0);
  publish_publisher.OnSubscribeAccepted();
  EXPECT_EQ(publish_publisher.subgroup_delivery_timeout(),
            quic::QuicTimeDelta::FromMilliseconds(500));
  EXPECT_EQ(publish_publisher.object_delivery_timeout(),
            quic::QuicTimeDelta::FromMilliseconds(250));
  EXPECT_CALL(*track_publisher_, RemoveObjectListener(&publish_publisher));
}

TEST_F(LivePublisherTest, PublishUpdateOnPublish) {
  EXPECT_CALL(*track_publisher_, properties())
      .WillRepeatedly(ReturnRef(properties_));
  EXPECT_CALL(mock_bidi_stream_, CanWrite()).WillRepeatedly(Return(true));

  MessageParameters publish_params;
  publish_params.set_forward(false);
  LivePublisher publish_publisher(framer_, track_publisher_, &bidi_stream_,
                                  kRequestId, kTrackAlias, publish_params,
                                  visitor_.weak_ptr_factory_.Create(),
                                  /*is_publish=*/true);

  // Ignored parameters alone return false and send nothing.
  MessageParameters ignored_only;
  ignored_only.subscriber_priority = 99;
  EXPECT_CALL(mock_bidi_stream_, Writev).Times(0);
  EXPECT_FALSE(publish_publisher.PublishUpdate(
      ignored_only,
      [](std::variant<MessageParameters, MoqtRequestErrorInfo>) {}));

  // AUTHORIZATION_TOKEN + LOCATION_FILTER + FORWARD=1 (+ an ignored
  // parameter) sends both REQUEST_UPDATE and PUBLISH_STATE_NOTIFY using
  // track_publisher_->largest_location().
  EXPECT_CALL(*track_publisher_, largest_location())
      .WillOnce(Return(Location(3, 5)));
  MessageParameters update_params;
  update_params.authorization_tokens.emplace_back(AuthTokenType::kOutOfBand,
                                                  "token");
  update_params.set_forward(true);
  update_params.location_filter = LocationFilter::NextObjectFilter();
  update_params.subscriber_priority = 99;

  MoqtRequestUpdate expected_update;
  expected_update.request_id = 2;
  expected_update.parameters.authorization_tokens =
      update_params.authorization_tokens;

  MoqtPublishStateNotify expected_notify;
  expected_notify.parameters.largest_object = Location(3, 5);
  expected_notify.parameters.set_forward(true);
  expected_notify.parameters.location_filter =
      LocationFilter::NextObjectFilter();

  EXPECT_CALL(visitor_, NextRequestId()).WillOnce(Return(2));
  {
    testing::InSequence seq;
    EXPECT_CALL(mock_bidi_stream_,
                Writev(SerializedControlMessage(expected_notify), _))
        .WillOnce(Return(absl::OkStatus()));
    EXPECT_CALL(mock_bidi_stream_,
                Writev(SerializedControlMessage(expected_update), _))
        .WillOnce(Return(absl::OkStatus()));
  }

  bool update_callback_called = false;
  EXPECT_TRUE(publish_publisher.PublishUpdate(
      update_params,
      [&](std::variant<MessageParameters, MoqtRequestErrorInfo>) {
        update_callback_called = true;
      }));
  EXPECT_TRUE(publish_publisher.parameters().forward());
  ASSERT_TRUE(publish_publisher.parameters().location_filter.has_value());
  EXPECT_EQ(publish_publisher.parameters().location_filter->start(),
            Location(3, 6));
  EXPECT_EQ(publish_publisher.parameters().largest_object, Location(3, 5));
  EXPECT_FALSE(update_callback_called);

  // FORWARD alone sends PUBLISH_STATE_NOTIFY, invokes response_callback
  // synchronously with empty MessageParameters, and returns true.
  MessageParameters notify_only;
  notify_only.set_forward(false);
  EXPECT_CALL(*track_publisher_, largest_location())
      .WillOnce(Return(Location(4, 0)));
  MoqtPublishStateNotify expected_notify_only;
  expected_notify_only.parameters.largest_object = Location(4, 0);
  expected_notify_only.parameters.set_forward(false);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(expected_notify_only), _))
      .WillOnce(Return(absl::OkStatus()));
  bool notify_callback_called = false;
  EXPECT_TRUE(publish_publisher.PublishUpdate(
      notify_only,
      [&](std::variant<MessageParameters, MoqtRequestErrorInfo> resp) {
        notify_callback_called = true;
        EXPECT_TRUE(std::holds_alternative<MessageParameters>(resp));
      }));
  EXPECT_FALSE(publish_publisher.parameters().forward());
  EXPECT_EQ(publish_publisher.parameters().largest_object, Location(4, 0));
  EXPECT_TRUE(notify_callback_called);
  EXPECT_CALL(*track_publisher_, RemoveObjectListener(&publish_publisher));
}

TEST_F(LivePublisherTest, PublishUpdateOnSubscribe) {
  EXPECT_CALL(mock_bidi_stream_, CanWrite()).WillRepeatedly(Return(true));

  MessageParameters auth_only;
  auth_only.authorization_tokens.emplace_back(AuthTokenType::kOutOfBand,
                                              "token");
  MessageParameters notify_params = auth_only;
  notify_params.set_forward(false);
  notify_params.subscriber_priority = 42;

  // Before SUBSCRIBE_OK is sent (!largest_object_known()), PublishUpdate hits
  // a QUICHE_BUG and returns false without invoking response_callback.
  EXPECT_CALL(mock_bidi_stream_, Writev).Times(0);
  bool pre_ok_callback_called = false;
  EXPECT_QUICHE_BUG(
      EXPECT_FALSE(publisher_->PublishUpdate(
          notify_params,
          [&](std::variant<MessageParameters, MoqtRequestErrorInfo>) {
            pre_ok_callback_called = true;
          })),
      "PublishUpdate called before largest_object_known_ is set");
  EXPECT_TRUE(publisher_->parameters().forward());
  EXPECT_FALSE(publisher_->parameters().largest_object.has_value());
  EXPECT_FALSE(pre_ok_callback_called);

  EXPECT_CALL(*track_publisher_, properties())
      .WillRepeatedly(ReturnRef(properties_));
  EXPECT_CALL(*track_publisher_, largest_location())
      .WillOnce(Return(Location(1, 0)));
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kSubscribeOk), _))
      .WillOnce(Return(absl::OkStatus()));
  publisher_->OnSubscribeAccepted();

  // AUTHORIZATION_TOKEN alone on a SUBSCRIBE does not send REQUEST_UPDATE and
  // returns false without invoking response_callback.
  EXPECT_CALL(mock_bidi_stream_, Writev).Times(0);
  bool auth_callback_called = false;
  EXPECT_FALSE(publisher_->PublishUpdate(
      auth_only, [&](std::variant<MessageParameters, MoqtRequestErrorInfo>) {
        auth_callback_called = true;
      }));
  EXPECT_FALSE(auth_callback_called);

  // AUTHORIZATION_TOKEN + FORWARD=0 on a SUBSCRIBE ignores AUTHORIZATION_TOKEN,
  // sends PUBLISH_STATE_NOTIFY with track_publisher_->largest_location(),
  // invokes response_callback synchronously with empty MessageParameters, and
  // returns true.
  EXPECT_CALL(*track_publisher_, largest_location())
      .WillOnce(Return(Location(2, 1)));

  MoqtPublishStateNotify expected_notify;
  expected_notify.parameters.largest_object = Location(2, 1);
  expected_notify.parameters.set_forward(false);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(expected_notify), _))
      .WillOnce(Return(absl::OkStatus()));
  bool callback_called = false;
  EXPECT_TRUE(publisher_->PublishUpdate(
      notify_params,
      [&](std::variant<MessageParameters, MoqtRequestErrorInfo> resp) {
        callback_called = true;
        EXPECT_TRUE(std::holds_alternative<MessageParameters>(resp));
      }));
  EXPECT_FALSE(publisher_->parameters().forward());
  EXPECT_EQ(publisher_->parameters().largest_object, Location(2, 1));
  EXPECT_TRUE(callback_called);
}

}  // namespace

}  // namespace moqt::test
