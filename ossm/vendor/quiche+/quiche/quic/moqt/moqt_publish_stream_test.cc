// Copyright (c) 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_publish_stream.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/moqt/moqt_error.h"
#include "quiche/quic/moqt/moqt_fetch_task.h"
#include "quiche/quic/moqt/moqt_framer.h"
#include "quiche/quic/moqt/moqt_key_value_pair.h"
#include "quiche/quic/moqt/moqt_live_publisher.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_names.h"
#include "quiche/quic/moqt/moqt_object_subscriber.h"
#include "quiche/quic/moqt/moqt_parser.h"
#include "quiche/quic/moqt/moqt_priority.h"
#include "quiche/quic/moqt/moqt_session_callbacks.h"
#include "quiche/quic/moqt/moqt_session_interface.h"
#include "quiche/quic/moqt/moqt_trace_recorder.h"
#include "quiche/quic/moqt/moqt_types.h"
#include "quiche/quic/moqt/test_tools/mock_moqt_session.h"
#include "quiche/quic/moqt/test_tools/moqt_framer_utils.h"
#include "quiche/quic/moqt/test_tools/moqt_mock_visitor.h"
#include "quiche/quic/test_tools/mock_clock.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_mem_slice.h"
#include "quiche/common/test_tools/quiche_test_utils.h"
#include "quiche/web_transport/test_tools/mock_web_transport.h"
#include "quiche/web_transport/web_transport.h"

namespace moqt::test {

class LivePublisherPeer {
 public:
  static const MessageParameters& parameters(const LivePublisher& publisher) {
    return publisher.parameters_;
  }
};

class LiveSubscriberPeer {
 public:
  static const MessageParameters& parameters(const LiveSubscriber& track) {
    return track.const_parameters();
  }
};

namespace {

using ::testing::_;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;

constexpr uint64_t kRequestId = 1;
constexpr uint64_t kTrackAlias = 10;
const FullTrackName kTrackName("foo", "bar");

class MoqtPublishRequestStreamTest : public quiche::test::QuicheTest {
 public:
  MoqtPublishRequestStreamTest()
      : framer_(/*using_webtrans=*/true, quic::Perspective::IS_CLIENT),
        message_parser_(kDefaultMoqtVersion, /*uses_web_transport=*/true,
                        quic::Perspective::IS_CLIENT),
        track_publisher_(std::make_shared<TestTrackPublisher>(kTrackName)) {
    ON_CALL(validate_request_id_, Call).WillByDefault(Return(absl::OkStatus()));
    // Construct the stream visitor.
    stream_ = std::make_unique<MoqtPublishRequestStream>(
        &framer_, message_parser_, deleted_callback_.AsStdFunction(),
        error_callback_.AsStdFunction(), validate_request_id_.AsStdFunction(),
        [this](std::variant<MessageParameters, MoqtRequestErrorInfo> response) {
          response_ = response;
        });

    // Construct the LivePublisher.
    parameters_.set_forward(true);
    parameters_.object_delivery_timeout = quic::QuicTimeDelta::FromSeconds(1);
    parameters_.group_order = MoqtGroupOrder::kAscending;

    EXPECT_CALL(visitor_, session).WillRepeatedly(Return(&webtrans_));
    auto publisher = std::make_unique<LivePublisher>(
        framer_, track_publisher_, stream_.get(), kRequestId, kTrackAlias,
        parameters_, visitor_.weak_ptr_factory_.Create(), /*is_publish=*/true);

    publisher_ = publisher.get();  // Keep raw pointer for testing
    stream_->SetPublisher(std::move(publisher));
    EXPECT_CALL(mock_stream_, CanWrite).WillRepeatedly(Return(true));
  }

  MoqtFramer framer_;
  MoqtControlMessageParser message_parser_;
  webtransport::test::MockStream mock_stream_;
  std::shared_ptr<TestTrackPublisher> track_publisher_;
  testing::MockFunction<void(LivePublisher*)> deleted_callback_;
  testing::StrictMock<testing::MockFunction<void(MoqtError, absl::string_view)>>
      error_callback_;
  testing::MockFunction<absl::Status(uint64_t)> validate_request_id_;
  MockSessionToPublisherInterface visitor_;
  webtransport::test::MockSession webtrans_;
  quic::MockClock mock_clock_;
  MoqtTraceRecorder trace_recorder_;
  MessageParameters parameters_;

  std::unique_ptr<MoqtPublishRequestStream> stream_;
  LivePublisher* publisher_;  // Raw pointer
  std::optional<std::variant<MessageParameters, MoqtRequestErrorInfo>>
      response_;
};

TEST_F(MoqtPublishRequestStreamTest, OnStreamBoundSendsPublish) {
  EXPECT_CALL(mock_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kPublish), _))
      .WillOnce(Return(absl::OkStatus()));
  stream_->BindStream(&mock_stream_);  // Calls OnStreamBound
}

TEST_F(MoqtPublishRequestStreamTest, ReceiveRequestOk) {
  EXPECT_CALL(mock_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kPublish), _))
      .WillOnce(Return(absl::OkStatus()));
  stream_->BindStream(&mock_stream_);  // Calls OnStreamBound

  MoqtRequestOk request_ok;
  request_ok.parameters.object_delivery_timeout =
      quic::QuicTimeDelta::FromSeconds(2);
  request_ok.parameters.group_order = MoqtGroupOrder::kDescending;
  QUICHE_EXPECT_OK(stream_->OnControlMessage(request_ok));

  // Verify response callback was called.
  ASSERT_TRUE(response_.has_value());
  ASSERT_TRUE(std::holds_alternative<MessageParameters>(*response_));
  MessageParameters resp_params = std::get<MessageParameters>(*response_);
  EXPECT_EQ(resp_params.object_delivery_timeout,
            request_ok.parameters.object_delivery_timeout);
  EXPECT_EQ(resp_params.group_order, request_ok.parameters.group_order);

  // Verify publisher parameters were updated.
  const MessageParameters& pub_params =
      LivePublisherPeer::parameters(*publisher_);
  EXPECT_EQ(pub_params.object_delivery_timeout,
            request_ok.parameters.object_delivery_timeout);
  // Group order cannot be updated.
  EXPECT_EQ(pub_params.group_order, parameters_.group_order);
}

TEST_F(MoqtPublishRequestStreamTest, ReceiveRequestError) {
  EXPECT_CALL(mock_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kPublish), _))
      .WillOnce(Return(absl::OkStatus()));
  stream_->BindStream(&mock_stream_);  // Calls OnStreamBound
  MoqtRequestError request_error(RequestErrorCode::kUnauthorized,
                                 quic::QuicTimeDelta::FromSeconds(5),
                                 "Unauthorized");
  QUICHE_EXPECT_OK(stream_->OnControlMessage(request_error));

  // Verify response callback was called with error.
  ASSERT_TRUE(response_.has_value());
  ASSERT_TRUE(std::holds_alternative<MoqtRequestErrorInfo>(*response_));
  EXPECT_EQ(std::get<MoqtRequestErrorInfo>(*response_), request_error);
}

TEST_F(MoqtPublishRequestStreamTest, ReceiveRequestUpdate) {
  EXPECT_CALL(mock_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kPublish), _))
      .WillOnce(Return(absl::OkStatus()));
  stream_->BindStream(&mock_stream_);

  QUICHE_EXPECT_OK(stream_->OnControlMessage(MoqtRequestOk()));
  // Set largest location on publisher
  track_publisher_->AddObject(Location(1, 2), 0, "payload", true);

  MoqtRequestUpdate request_update;
  request_update.request_id = kRequestId + 2;
  request_update.parameters.object_delivery_timeout =
      quic::QuicTimeDelta::FromSeconds(3);
  request_update.parameters.subscriber_priority = 5;
  request_update.parameters.location_filter =
      LocationFilter::NextObjectFilter();
  EXPECT_CALL(mock_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kRequestOk), _))
      .WillOnce(Return(absl::OkStatus()));
  QUICHE_EXPECT_OK(stream_->OnControlMessage(request_update));

  // Verify publisher parameters were updated.
  const MessageParameters& pub_params =
      LivePublisherPeer::parameters(*publisher_);
  EXPECT_EQ(pub_params.object_delivery_timeout,
            request_update.parameters.object_delivery_timeout);
  EXPECT_EQ(pub_params.subscriber_priority,
            request_update.parameters.subscriber_priority);

  // Verify filter was updated based on largest location (1, 2) -> (1, 3)
  ASSERT_TRUE(pub_params.location_filter.has_value());
  EXPECT_EQ(pub_params.location_filter->start(), Location(1, 3));
}

TEST_F(MoqtPublishRequestStreamTest, ReceiveObjectAck) {
  EXPECT_CALL(mock_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kPublish), _))
      .WillOnce(Return(absl::OkStatus()));
  stream_->BindStream(&mock_stream_);

  MoqtObjectAck ack;
  ack.group_id = 1;
  ack.object_id = 2;
  ack.delta_from_deadline = quic::QuicTimeDelta::FromMilliseconds(100);

  EXPECT_CALL(visitor_, trace_recorder())
      .WillOnce(testing::ReturnRef(trace_recorder_));
  QUICHE_EXPECT_OK(stream_->OnControlMessage(ack));
}

TEST_F(MoqtPublishRequestStreamTest, Detach) {
  EXPECT_CALL(deleted_callback_, Call(publisher_));
  stream_->Detach();
  // Verifying second detach is a no-op
  EXPECT_CALL(deleted_callback_, Call).Times(0);
  stream_->Detach();
}

TEST_F(MoqtPublishRequestStreamTest, ReceivePublishStateNotifyRejected) {
  MoqtPublishStateNotify notify;
  notify.parameters.largest_object = Location(1, 0);
  notify.parameters.set_forward(false);
  EXPECT_THAT(
      stream_->OnRawControlMessage(GenericMessageToRawControlMessage(notify)),
      quiche::test::StatusIs(
          absl::StatusCode::kInvalidArgument,
          "Received an unexpected message of type PUBLISH_STATE_NOTIFY on a "
          "publish publisher stream"));
}

class MoqtPublishResponseStreamTest : public quiche::test::QuicheTest {
 public:
  MoqtPublishResponseStreamTest()
      : framer_(/*using_webtrans=*/true, quic::Perspective::IS_SERVER),
        message_parser_(kDefaultMoqtVersion, /*uses_web_transport=*/true,
                        quic::Perspective::IS_SERVER),
        incoming_publish_callback_(
            incoming_publish_callback_mock_.AsStdFunction()) {
    ON_CALL(validate_request_id_, Call).WillByDefault(Return(absl::OkStatus()));
    stream_ = std::make_unique<MoqtPublishResponseStream>(
        &framer_, message_parser_, &mock_clock_, &mock_alarm_factory_,
        error_callback_.AsStdFunction(), validate_request_id_.AsStdFunction(),
        &incoming_publish_callback_, mock_add_callback_.AsStdFunction(),
        mock_remove_callback_.AsStdFunction());
    stream_->BindStream(&mock_stream_);
    EXPECT_CALL(mock_stream_, CanWrite).WillRepeatedly(Return(true));
  }

  MoqtPublish DefaultPublish() {
    return MoqtPublish{kRequestId, kTrackName, kTrackAlias, MessageParameters(),
                       TrackProperties()};
  }

  MoqtFramer framer_;
  MoqtControlMessageParser message_parser_;
  quic::MockClock mock_clock_;
  quic::test::MockAlarmFactory mock_alarm_factory_;
  testing::StrictMock<testing::MockFunction<void(MoqtError, absl::string_view)>>
      error_callback_;
  testing::MockFunction<absl::Status(uint64_t)> validate_request_id_;

  testing::MockFunction<SubscribeVisitor*(
      const FullTrackName&, const MessageParameters&, const TrackProperties&,
      MoqtResponseCallback)>
      incoming_publish_callback_mock_;
  MoqtIncomingPublishCallback incoming_publish_callback_;

  testing::MockFunction<bool(LiveSubscriber*)> mock_add_callback_;
  testing::MockFunction<void(LiveSubscriber*)> mock_remove_callback_;

  StrictMock<MockLiveSubscriberVisitor> mock_subscribe_visitor_;
  MoqtResponseCallback captured_response_callback_;
  webtransport::test::MockStream mock_stream_;
  std::unique_ptr<MoqtPublishResponseStream> stream_;
};

TEST_F(MoqtPublishResponseStreamTest, ReceivePublishAndAccept) {
  EXPECT_CALL(mock_subscribe_visitor_, OnReply(kTrackName, _))
      .WillOnce(
          [](const FullTrackName&,
             const std::variant<SubscribeOkData, MoqtRequestErrorInfo>& reply) {
            EXPECT_TRUE(std::holds_alternative<SubscribeOkData>(reply));
          });
  EXPECT_CALL(incoming_publish_callback_mock_, Call(kTrackName, _, _, _))
      .WillOnce([this](const FullTrackName&, const MessageParameters&,
                       const TrackProperties&, MoqtResponseCallback callback) {
        captured_response_callback_ = std::move(callback);
        return &mock_subscribe_visitor_;
      });
  LiveSubscriber* captured_subscriber = nullptr;
  EXPECT_CALL(mock_add_callback_, Call(NotNull()))
      .WillOnce([&](LiveSubscriber* subscriber) {
        captured_subscriber = subscriber;
        return true;
      });
  MoqtPublish publish = DefaultPublish();
  publish.parameters.object_delivery_timeout =
      quic::QuicTimeDelta::FromSeconds(1);
  QUICHE_EXPECT_OK(stream_->OnControlMessage(publish));

  // Verify subscriber is created.
  ASSERT_NE(captured_subscriber, nullptr);
  EXPECT_EQ(captured_subscriber->track_alias(), kTrackAlias);
  EXPECT_EQ(captured_subscriber->visitor(), &mock_subscribe_visitor_);

  // Verify REQUEST_OK response was sent with new_group_request filtered out
  // because DYNAMIC_GROUPS is absent/false.
  MoqtRequestOk expected_ok;
  expected_ok.parameters.object_delivery_timeout =
      quic::QuicTimeDelta::FromSeconds(2);
  EXPECT_CALL(mock_stream_, Writev(SerializedControlMessage(expected_ok), _))
      .WillOnce(Return(absl::OkStatus()));
  MessageParameters response_parameters = expected_ok.parameters;
  response_parameters.new_group_request = 0;
  std::move(captured_response_callback_)(response_parameters);

  // Verify subscriber parameters were updated and new_group_request was
  // filtered out.
  const MessageParameters& sub_params =
      LiveSubscriberPeer::parameters(*captured_subscriber);
  EXPECT_EQ(sub_params.object_delivery_timeout,
            response_parameters.object_delivery_timeout);
  EXPECT_EQ(sub_params.new_group_request, std::nullopt);
  EXPECT_CALL(mock_subscribe_visitor_, OnPublishDone);
}

TEST_F(MoqtPublishResponseStreamTest, ReceivePublishWithDynamicGroups) {
  EXPECT_CALL(mock_subscribe_visitor_, OnReply(kTrackName, _))
      .WillOnce(
          [](const FullTrackName&,
             const std::variant<SubscribeOkData, MoqtRequestErrorInfo>& reply) {
            EXPECT_TRUE(std::holds_alternative<SubscribeOkData>(reply));
          });
  EXPECT_CALL(incoming_publish_callback_mock_, Call(kTrackName, _, _, _))
      .WillOnce([this](const FullTrackName&, const MessageParameters&,
                       const TrackProperties&, MoqtResponseCallback callback) {
        captured_response_callback_ = std::move(callback);
        return &mock_subscribe_visitor_;
      });
  LiveSubscriber* captured_subscriber = nullptr;
  EXPECT_CALL(mock_add_callback_, Call(NotNull()))
      .WillOnce([&](LiveSubscriber* subscriber) {
        captured_subscriber = subscriber;
        return true;
      });
  MoqtPublish publish = DefaultPublish();
  publish.properties = TrackProperties(
      /*object_delivery_timeout=*/std::nullopt,
      /*max_cache_duration=*/std::nullopt,
      /*subgroup_delivery_timeout=*/std::nullopt,
      /*publisher_priority=*/std::nullopt,
      /*group_order=*/std::nullopt,
      /*dynamic_groups=*/true,
      /*immutable_extensions=*/std::nullopt);
  QUICHE_EXPECT_OK(stream_->OnControlMessage(publish));
  ASSERT_NE(captured_subscriber, nullptr);

  // Verify REQUEST_OK response preserves new_group_request when DYNAMIC_GROUPS
  // is true.
  MoqtRequestOk expected_ok;
  expected_ok.parameters.new_group_request = 0;
  EXPECT_CALL(mock_stream_, Writev(SerializedControlMessage(expected_ok), _))
      .WillOnce(Return(absl::OkStatus()));
  std::move(captured_response_callback_)(expected_ok.parameters);
  const MessageParameters& sub_params =
      LiveSubscriberPeer::parameters(*captured_subscriber);
  EXPECT_EQ(sub_params.new_group_request, 0);
  EXPECT_CALL(mock_subscribe_visitor_, OnPublishDone);
}

TEST_F(MoqtPublishResponseStreamTest,
       ReceivePublishWithUnknownMandatoryProperty) {
  MoqtPublish publish = DefaultPublish();
  publish.properties.insert(kMinMandatoryTrackProperty, 1ULL);
  EXPECT_CALL(incoming_publish_callback_mock_, Call).Times(0);
  EXPECT_CALL(mock_add_callback_, Call).Times(0);
  MoqtRequestError expected_error{RequestErrorCode::kUnsupportedExtension,
                                  /*retry_interval=*/std::nullopt,
                                  "Unknown mandatory property: 0x4000"};
  EXPECT_CALL(mock_stream_, Writev(SerializedControlMessage(expected_error), _))
      .WillOnce(Return(absl::OkStatus()));
  QUICHE_EXPECT_OK(stream_->OnControlMessage(publish));
}

TEST_F(MoqtPublishResponseStreamTest, ReceivePublishReservedTrackName) {
  MoqtPublish publish = DefaultPublish();
  publish.full_track_name = FullTrackName(TrackNamespace({"."}), "track");
  EXPECT_CALL(incoming_publish_callback_mock_, Call).Times(0);
  EXPECT_CALL(mock_add_callback_, Call).Times(0);
  MoqtRequestError expected_error{RequestErrorCode::kDoesNotExist,
                                  /*retry_interval=*/std::nullopt,
                                  "Reserved track name"};
  EXPECT_CALL(mock_stream_, Writev(SerializedControlMessage(expected_error), _))
      .WillOnce(Return(absl::OkStatus()));
  QUICHE_EXPECT_OK(stream_->OnControlMessage(publish));
}

TEST_F(MoqtPublishResponseStreamTest, ReceivePublishAndReject) {
  MoqtPublish publish = DefaultPublish();
  // Callback returns nullptr (rejection).
  EXPECT_CALL(mock_add_callback_, Call(NotNull())).WillOnce(Return(true));
  EXPECT_CALL(incoming_publish_callback_mock_, Call(kTrackName, _, _, _))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(mock_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kRequestError), _))
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(mock_remove_callback_, Call);
  QUICHE_EXPECT_OK(stream_->OnControlMessage(publish));
}

TEST_F(MoqtPublishResponseStreamTest, ReceiveTwoPublishOnStream) {
  MoqtPublish publish = DefaultPublish();
  EXPECT_CALL(mock_add_callback_, Call(NotNull())).WillOnce(Return(true));
  EXPECT_CALL(incoming_publish_callback_mock_, Call(kTrackName, _, _, _))
      .WillOnce(Return(&mock_subscribe_visitor_));
  EXPECT_CALL(mock_subscribe_visitor_, OnReply(kTrackName, _))
      .WillOnce(
          [](const FullTrackName&,
             const std::variant<SubscribeOkData, MoqtRequestErrorInfo>& reply) {
            EXPECT_TRUE(std::holds_alternative<SubscribeOkData>(reply));
          });
  QUICHE_EXPECT_OK(stream_->OnControlMessage(publish));

  // Receive second PUBLISH on same stream.
  publish.request_id = kRequestId + 2;
  publish.full_track_name = FullTrackName("dead", "beef");
  publish.track_alias = kTrackAlias + 1;
  EXPECT_FALSE(stream_->OnControlMessage(publish).ok());
  EXPECT_CALL(mock_subscribe_visitor_, OnPublishDone(kTrackName));
}

TEST_F(MoqtPublishResponseStreamTest, ReceivePublishDuplicate) {
  MoqtPublish publish = DefaultPublish();
  publish.request_id = kRequestId + 2;
  publish.full_track_name = kTrackName;
  publish.track_alias = kTrackAlias;

  EXPECT_CALL(mock_add_callback_, Call).WillOnce(Return(false));
  EXPECT_CALL(mock_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kRequestError), _))
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(mock_remove_callback_, Call);
  QUICHE_EXPECT_OK(stream_->OnControlMessage(publish));
}

TEST_F(MoqtPublishResponseStreamTest, ReceiveRequestUpdate) {
  MoqtPublish publish = DefaultPublish();
  publish.parameters.largest_object = Location(1, 2);
  EXPECT_CALL(incoming_publish_callback_mock_, Call(kTrackName, _, _, _))
      .WillOnce(Return(&mock_subscribe_visitor_));
  LiveSubscriber* captured_subscriber = nullptr;
  EXPECT_CALL(mock_add_callback_, Call(NotNull()))
      .WillOnce([&](LiveSubscriber* track) {
        captured_subscriber = track;
        return true;
      });
  EXPECT_CALL(mock_subscribe_visitor_, OnReply(kTrackName, _))
      .WillOnce(
          [](const FullTrackName&,
             const std::variant<SubscribeOkData, MoqtRequestErrorInfo>& reply) {
            EXPECT_TRUE(std::holds_alternative<SubscribeOkData>(reply));
          });
  QUICHE_EXPECT_OK(stream_->OnControlMessage(publish));

  // Now receive REQUEST_UPDATE (which cannot contain LARGEST_OBJECT even though
  // PUBLISH had LARGEST_OBJECT set).
  MoqtRequestUpdate request_update;
  request_update.request_id = kRequestId + 2;
  request_update.parameters.authorization_tokens.emplace_back(
      AuthTokenType::kOutOfBand, "token");
  EXPECT_CALL(mock_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kRequestOk), _))
      .WillOnce(Return(absl::OkStatus()));
  QUICHE_EXPECT_OK(stream_->OnControlMessage(request_update));

  // Verify subscriber parameters were updated and largest_object was preserved.
  ASSERT_NE(captured_subscriber, nullptr);
  const MessageParameters& sub_params =
      LiveSubscriberPeer::parameters(*captured_subscriber);
  EXPECT_EQ(sub_params.authorization_tokens,
            request_update.parameters.authorization_tokens);
  EXPECT_EQ(sub_params.largest_object, Location(1, 2));
  EXPECT_CALL(mock_subscribe_visitor_, OnPublishDone(kTrackName));
}

TEST_F(MoqtPublishResponseStreamTest, ReceivePublishStateNotify) {
  MoqtPublish publish = DefaultPublish();
  EXPECT_CALL(incoming_publish_callback_mock_, Call(kTrackName, _, _, _))
      .WillOnce(Return(&mock_subscribe_visitor_));
  LiveSubscriber* captured_subscriber = nullptr;
  EXPECT_CALL(mock_add_callback_, Call(NotNull()))
      .WillOnce([&](LiveSubscriber* track) {
        captured_subscriber = track;
        return true;
      });
  EXPECT_CALL(mock_subscribe_visitor_, OnReply(kTrackName, _))
      .WillOnce(
          [](const FullTrackName&,
             const std::variant<SubscribeOkData, MoqtRequestErrorInfo>& reply) {
            EXPECT_TRUE(std::holds_alternative<SubscribeOkData>(reply));
          });
  QUICHE_EXPECT_OK(stream_->OnControlMessage(publish));

  MoqtPublishStateNotify notify;
  notify.parameters.largest_object = Location(2, 5);
  notify.parameters.set_forward(false);
  notify.parameters.location_filter =
      LocationFilter::AbsoluteStartGroupEndFilter(Location(3, 0), 8);
  QUICHE_EXPECT_OK(
      stream_->OnRawControlMessage(GenericMessageToRawControlMessage(notify)));

  ASSERT_NE(captured_subscriber, nullptr);
  const MessageParameters& sub_params =
      LiveSubscriberPeer::parameters(*captured_subscriber);
  EXPECT_EQ(sub_params.largest_object, Location(2, 5));
  EXPECT_FALSE(sub_params.forward());
  EXPECT_EQ(sub_params.location_filter,
            LocationFilter::AbsoluteStartGroupEndFilter(Location(3, 0), 8));

  // A subsequent PUBLISH_STATE_NOTIFY decreasing LARGEST_OBJECT is rejected.
  MoqtPublishStateNotify bad_notify;
  bad_notify.parameters.largest_object = Location(2, 4);
  bad_notify.parameters.set_forward(true);
  EXPECT_THAT(stream_->OnRawControlMessage(
                  GenericMessageToRawControlMessage(bad_notify)),
              quiche::test::StatusIs(absl::StatusCode::kInvalidArgument,
                                     "LARGEST_OBJECT cannot be decreased"));
  EXPECT_CALL(mock_subscribe_visitor_, OnPublishDone(kTrackName));
}

TEST_F(MoqtPublishResponseStreamTest, ReceivePublishDone) {
  MoqtPublish publish = DefaultPublish();
  EXPECT_CALL(incoming_publish_callback_mock_, Call(kTrackName, _, _, _))
      .WillOnce(Return(&mock_subscribe_visitor_));
  EXPECT_CALL(mock_add_callback_, Call(NotNull())).WillOnce(Return(true));
  EXPECT_CALL(mock_subscribe_visitor_, OnReply(kTrackName, _))
      .WillOnce(
          [](const FullTrackName&,
             const std::variant<SubscribeOkData, MoqtRequestErrorInfo>& reply) {
            EXPECT_TRUE(std::holds_alternative<SubscribeOkData>(reply));
          });
  QUICHE_EXPECT_OK(stream_->OnControlMessage(publish));

  // Now receive PUBLISH_DONE.
  MoqtPublishDone publish_done;
  publish_done.status_code = PublishDoneCode::kTrackEnded;
  publish_done.stream_count = 0;  // Trigger immediate Destroy
  EXPECT_CALL(mock_stream_, Writev)
      .WillOnce([](absl::Span<quiche::QuicheMemSlice> data,
                   const webtransport::StreamWriteOptions& options) {
        EXPECT_TRUE(data.empty());
        EXPECT_TRUE(options.send_fin());
        return absl::OkStatus();
      });
  EXPECT_CALL(mock_remove_callback_, Call);
  EXPECT_CALL(mock_subscribe_visitor_, OnPublishDone(kTrackName));
  QUICHE_EXPECT_OK(stream_->OnControlMessage(publish_done));
}

TEST_F(MoqtPublishResponseStreamTest, ReceivePublishAndRejectCallback) {
  MoqtPublish publish = DefaultPublish();
  EXPECT_CALL(incoming_publish_callback_mock_, Call(kTrackName, _, _, _))
      .WillOnce([this](const FullTrackName&, const MessageParameters&,
                       const TrackProperties&, MoqtResponseCallback callback) {
        captured_response_callback_ = std::move(callback);
        return &mock_subscribe_visitor_;
      });
  EXPECT_CALL(mock_add_callback_, Call(NotNull())).WillOnce(Return(true));
  EXPECT_CALL(mock_subscribe_visitor_, OnReply(kTrackName, _))
      .WillOnce(
          [](const FullTrackName&,
             const std::variant<SubscribeOkData, MoqtRequestErrorInfo>& reply) {
            EXPECT_TRUE(std::holds_alternative<SubscribeOkData>(reply));
          });
  QUICHE_EXPECT_OK(stream_->OnControlMessage(publish));

  // Now call the response callback with error (reject).
  EXPECT_CALL(mock_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kRequestError), _))
      .WillOnce(Return(absl::OkStatus()));
  MoqtRequestErrorInfo error_info{RequestErrorCode::kUninterested, std::nullopt,
                                  "rejected by app"};
  EXPECT_CALL(mock_remove_callback_, Call);
  EXPECT_CALL(mock_subscribe_visitor_, OnPublishDone(kTrackName));
  std::move(captured_response_callback_)(error_info);
}

TEST_F(MoqtPublishResponseStreamTest, ReceivePublishDoneOnRejectedStream) {
  MoqtPublish publish = DefaultPublish();
  // Callback returns nullptr (rejection).
  EXPECT_CALL(mock_add_callback_, Call(NotNull())).WillOnce(Return(true));
  EXPECT_CALL(incoming_publish_callback_mock_, Call(kTrackName, _, _, _))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(mock_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kRequestError), _))
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(mock_remove_callback_, Call);
  QUICHE_EXPECT_OK(stream_->OnControlMessage(publish));

  // Now receive PUBLISH_DONE.
  MoqtPublishDone publish_done;
  publish_done.status_code = PublishDoneCode::kTrackEnded;
  publish_done.stream_count = 0;
  QUICHE_EXPECT_OK(stream_->OnControlMessage(publish_done));
}

TEST_F(MoqtPublishResponseStreamTest, DuplicatePublishOnSameStream) {
  MoqtPublish publish = DefaultPublish();
  EXPECT_CALL(mock_add_callback_, Call(NotNull())).WillOnce(Return(true));
  EXPECT_CALL(incoming_publish_callback_mock_, Call(kTrackName, _, _, _))
      .WillOnce(Return(&mock_subscribe_visitor_));
  EXPECT_CALL(mock_subscribe_visitor_, OnReply(kTrackName, _))
      .WillOnce(
          [](const FullTrackName&,
             const std::variant<SubscribeOkData, MoqtRequestErrorInfo>& reply) {
            EXPECT_TRUE(std::holds_alternative<SubscribeOkData>(reply));
          });
  QUICHE_EXPECT_OK(stream_->OnControlMessage(publish));

  // Receive second PUBLISH on same stream. Should cause a session error.
  EXPECT_CALL(error_callback_, Call(MoqtError::kProtocolViolation,
                                    "Multiple PUBLISH on the same stream"));
  MoqtPublish publish2 = DefaultPublish();
  publish2.request_id = kRequestId + 2;
  publish2.full_track_name = FullTrackName("dead", "beef");
  publish2.track_alias = kTrackAlias + 1;
  stream_->CheckStatus(stream_->OnControlMessage(publish2));
  EXPECT_CALL(mock_subscribe_visitor_, OnPublishDone(kTrackName));
}

TEST_F(MoqtPublishResponseStreamTest, DuplicatePublishOnDifferentStreams) {
  MoqtPublish publish1 = DefaultPublish();
  EXPECT_CALL(mock_add_callback_, Call(NotNull())).WillOnce(Return(true));
  EXPECT_CALL(incoming_publish_callback_mock_, Call(kTrackName, _, _, _))
      .WillOnce(Return(&mock_subscribe_visitor_));
  EXPECT_CALL(mock_subscribe_visitor_, OnReply(kTrackName, _))
      .WillOnce(
          [](const FullTrackName&,
             const std::variant<SubscribeOkData, MoqtRequestErrorInfo>& reply) {
            EXPECT_TRUE(std::holds_alternative<SubscribeOkData>(reply));
          });
  QUICHE_EXPECT_OK(stream_->OnControlMessage(publish1));

  // Second stream
  testing::MockFunction<void(MoqtError, absl::string_view)> error_callback2;
  testing::MockFunction<bool(LiveSubscriber*)> mock_add_callback2;
  testing::MockFunction<void(LiveSubscriber*)> mock_remove_callback2;
  testing::MockFunction<SubscribeVisitor*(
      const FullTrackName&, const MessageParameters&, const TrackProperties&,
      MoqtResponseCallback)>
      incoming_publish_callback_mock2;
  MoqtIncomingPublishCallback incoming_publish_callback2 =
      incoming_publish_callback_mock2.AsStdFunction();

  webtransport::test::MockStream mock_stream2;
  MoqtPublishResponseStream stream2(
      &framer_, message_parser_, &mock_clock_, &mock_alarm_factory_,
      error_callback2.AsStdFunction(), validate_request_id_.AsStdFunction(),
      &incoming_publish_callback2, mock_add_callback2.AsStdFunction(),
      mock_remove_callback2.AsStdFunction());
  stream2.BindStream(&mock_stream2);
  EXPECT_CALL(mock_stream2, CanWrite).WillRepeatedly(Return(true));

  // Duplicate PUBLISH (same track name) on stream2 is rejected.
  MoqtPublish publish2 = DefaultPublish();
  publish2.request_id = kRequestId + 2;
  publish2.full_track_name = kTrackName;
  EXPECT_CALL(mock_add_callback2, Call(NotNull())).WillOnce(Return(false));
  EXPECT_CALL(mock_stream2,
              Writev(ControlMessageOfType(MoqtMessageType::kRequestError), _))
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(mock_remove_callback2, Call);

  QUICHE_EXPECT_OK(stream2.OnControlMessage(publish2));

  // Finally, a PUBLISH on the second stream triggers a session error.
  EXPECT_CALL(error_callback2, Call(MoqtError::kProtocolViolation,
                                    "Multiple PUBLISH on the same stream"));
  MoqtPublish publish3 = DefaultPublish();
  publish3.request_id = kRequestId + 4;
  publish3.full_track_name = kTrackName;
  stream2.CheckStatus(stream2.OnControlMessage(publish3));

  // Test teardown expectations
  EXPECT_CALL(mock_subscribe_visitor_, OnPublishDone(kTrackName));
}

TEST_F(MoqtPublishResponseStreamTest, TrackAndDetach) {
  MoqtPublish publish = DefaultPublish();
  EXPECT_CALL(incoming_publish_callback_mock_, Call(kTrackName, _, _, _))
      .WillOnce(Return(&mock_subscribe_visitor_));
  EXPECT_CALL(mock_add_callback_, Call(NotNull())).WillOnce(Return(true));
  EXPECT_CALL(mock_subscribe_visitor_, OnReply(kTrackName, _))
      .WillOnce(
          [](const FullTrackName&,
             const std::variant<SubscribeOkData, MoqtRequestErrorInfo>& reply) {
            EXPECT_TRUE(std::holds_alternative<SubscribeOkData>(reply));
          });
  QUICHE_EXPECT_OK(stream_->OnControlMessage(publish));
  EXPECT_NE(stream_->track(), nullptr);
  EXPECT_EQ(stream_->track()->track_alias(), kTrackAlias);

  EXPECT_CALL(mock_remove_callback_, Call(stream_->track()));
  EXPECT_CALL(mock_subscribe_visitor_, OnPublishDone(kTrackName));
  stream_->Detach();
  EXPECT_EQ(stream_->track(), nullptr);
}

}  // namespace
}  // namespace moqt::test
