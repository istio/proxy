// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/quic_bandwidth.h"
#include "quiche/quic/core/quic_generic_session.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/moqt/moqt_error.h"
#include "quiche/quic/moqt/moqt_fetch_task.h"
#include "quiche/quic/moqt/moqt_key_value_pair.h"
#include "quiche/quic/moqt/moqt_known_track_publisher.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_names.h"
#include "quiche/quic/moqt/moqt_object.h"
#include "quiche/quic/moqt/moqt_outgoing_queue.h"
#include "quiche/quic/moqt/moqt_probe_manager.h"
#include "quiche/quic/moqt/moqt_publisher.h"
#include "quiche/quic/moqt/moqt_relay_publisher.h"
#include "quiche/quic/moqt/moqt_session.h"
#include "quiche/quic/moqt/moqt_session_callbacks.h"
#include "quiche/quic/moqt/moqt_session_interface.h"
#include "quiche/quic/moqt/moqt_types.h"
#include "quiche/quic/moqt/test_tools/moqt_mock_visitor.h"
#include "quiche/quic/moqt/test_tools/moqt_session_peer.h"
#include "quiche/quic/moqt/test_tools/moqt_simulator_harness.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/quic/test_tools/simulator/link.h"
#include "quiche/quic/test_tools/simulator/simulator.h"
#include "quiche/quic/test_tools/simulator/test_harness.h"
#include "quic_trace/quic_trace.pb.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_mem_slice.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

namespace moqt::test {

namespace {

using ::quic::test::MemSliceFromString;
using ::quiche::QuicheMemSlice;
using ::testing::_;
using ::testing::Assign;
using ::testing::ElementsAre;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Return;

class MoqtIntegrationTest : public quiche::test::QuicheTest {
 public:
  void CreateDefaultEndpoints() {
    client_ = std::make_unique<MoqtClientEndpoint>(
        &test_harness_.simulator(), "Client", "Server", kDefaultMoqtVersion);
    server_ = std::make_unique<MoqtServerEndpoint>(
        &test_harness_.simulator(), "Server", "Client", kDefaultMoqtVersion);
    SetupCallbacks();
    test_harness_.set_client(client_.get());
    test_harness_.set_server(server_.get());
  }
  void SetupCallbacks() {
    client_->session()->callbacks() = client_callbacks_.AsSessionCallbacks();
    client_->session()->callbacks().clock =
        test_harness_.simulator().GetClock();
    server_->session()->callbacks() = server_callbacks_.AsSessionCallbacks();
    server_->session()->callbacks().clock =
        test_harness_.simulator().GetClock();

    client_->RecordTrace();
    client_->session()->trace_recorder().SetParentRecorder(
        client_->trace_visitor());
    server_->RecordTrace();
    server_->session()->trace_recorder().SetParentRecorder(
        server_->trace_visitor());
  }

  void WireUpEndpoints() { test_harness_.WireUpEndpoints(); }
  void WireUpEndpointsWithLoss(int lose_every_n) {
    test_harness_.WireUpEndpointsWithLoss(lose_every_n);
  }
  void ConnectEndpoints() {
    RunHandshakeOrDie(test_harness_.simulator(), *client_, *server_);
  }

  void EstablishSession() {
    CreateDefaultEndpoints();
    WireUpEndpoints();
    ConnectEndpoints();
  }

  // Client subscribes to the latest object in |track_name|.
  void SubscribeLatestObject(FullTrackName track_name,
                             MockLiveSubscriberVisitor* visitor) {
    bool received_ok = false;
    EXPECT_CALL(*visitor, OnReply)
        .WillOnce(
            [&](const FullTrackName&,
                std::variant<SubscribeOkData, MoqtRequestErrorInfo> response) {
              received_ok = std::holds_alternative<SubscribeOkData>(response);
            });
    MessageParameters parameters(LocationFilter::NextObjectFilter());
    client_->session()->Subscribe(track_name, visitor, parameters);

    bool success =
        test_harness_.RunUntilWithDefaultTimeout([&]() { return received_ok; });
    EXPECT_TRUE(success);
  }

 protected:
  quic::simulator::TestHarness test_harness_;

  MockSessionCallbacks client_callbacks_;
  MockSessionCallbacks server_callbacks_;
  MockLiveSubscriberVisitor subscribe_visitor_;
  testing::MockFunction<void(TrackNamespace track_namespace,
                             std::optional<MoqtRequestErrorInfo> error_message)>
      outgoing_publish_namespace_callback_;
  std::unique_ptr<MoqtClientEndpoint> client_;
  std::unique_ptr<MoqtServerEndpoint> server_;
};

MATCHER_P2(
    MetadataLocationAndStatus, location, status,
    "Matches a PublishedObjectMetadata against Location and ObjectStatus") {
  return arg.location == location && status == arg.status;
}

TEST_F(MoqtIntegrationTest, Handshake) {
  CreateDefaultEndpoints();
  WireUpEndpoints();

  client_->quic_session()->CryptoConnect();
  bool client_established = false;
  bool server_established = false;
  EXPECT_CALL(client_callbacks_.session_established_callback, Call())
      .WillOnce(Assign(&client_established, true));
  EXPECT_CALL(server_callbacks_.session_established_callback, Call())
      .WillOnce(Assign(&server_established, true));
  bool success = test_harness_.RunUntilWithDefaultTimeout(
      [&]() { return client_established && server_established; });
  EXPECT_TRUE(success);
}

TEST_F(MoqtIntegrationTest, VersionMismatch) {
  client_ = std::make_unique<MoqtClientEndpoint>(&test_harness_.simulator(),
                                                 "Client", "Server",
                                                 kUnrecognizedVersionForTests);
  server_ = std::make_unique<MoqtServerEndpoint>(
      &test_harness_.simulator(), "Server", "Client", kDefaultMoqtVersion);
  SetupCallbacks();
  test_harness_.set_client(client_.get());
  test_harness_.set_server(server_.get());
  WireUpEndpoints();

  client_->quic_session()->CryptoConnect();
  bool client_terminated = false;
  bool server_terminated = false;
  EXPECT_CALL(client_callbacks_.session_established_callback, Call()).Times(0);
  EXPECT_CALL(server_callbacks_.session_established_callback, Call()).Times(0);
  EXPECT_CALL(client_callbacks_.session_terminated_callback, Call(_))
      .WillOnce(Assign(&client_terminated, true));
  EXPECT_CALL(server_callbacks_.session_terminated_callback, Call(_))
      .WillOnce(Assign(&server_terminated, true));
  bool success = test_harness_.RunUntilWithDefaultTimeout(
      [&]() { return client_terminated && server_terminated; });
  EXPECT_TRUE(success);
}

TEST_F(MoqtIntegrationTest, PublishNamespaceSuccessThenPublishNamespaceDone) {
  EstablishSession();
  MessageParameters parameters;
  parameters.authorization_tokens.emplace_back(AuthTokenType::kOutOfBand,
                                               "foo");
  EXPECT_CALL(server_callbacks_.incoming_publish_namespace_callback,
              Call(TrackNamespace{"foo"}, NotNull(), _))
      .WillOnce([&](const TrackNamespace&, const MessageParameters* params,
                    MoqtResponseCallback callback) {
        EXPECT_TRUE(params != nullptr && *params == parameters);
        std::move(callback)(MessageParameters());
      });
  testing::MockFunction<void(
      std::variant<MessageParameters, MoqtRequestErrorInfo>)>
      response_callback;
  client_->session()->PublishNamespace(TrackNamespace{"foo"}, parameters,
                                       response_callback.AsStdFunction(),
                                       []() {});
  bool matches = false;
  EXPECT_CALL(response_callback, Call)
      .WillOnce(
          [&](std::variant<MessageParameters, MoqtRequestErrorInfo> response) {
            matches = true;
            EXPECT_TRUE(std::holds_alternative<MessageParameters>(response));
          });
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return matches; });
  EXPECT_TRUE(success);
  matches = false;
  EXPECT_CALL(server_callbacks_.incoming_publish_namespace_callback,
              Call(TrackNamespace{"foo"}, IsNull(), _))
      .WillOnce([&](const TrackNamespace&, const MessageParameters*,
                    MoqtResponseCallback) { matches = true; });
  EXPECT_TRUE(client_->session()->PublishNamespaceDone(TrackNamespace{"foo"}));
  success = test_harness_.RunUntilWithDefaultTimeout([&]() { return matches; });
  EXPECT_TRUE(success);
}

TEST_F(MoqtIntegrationTest, PublishNamespaceSuccessThenCancel) {
  EstablishSession();
  MessageParameters parameters;
  parameters.authorization_tokens.emplace_back(AuthTokenType::kOutOfBand,
                                               "foo");
  EXPECT_CALL(server_callbacks_.incoming_publish_namespace_callback,
              Call(TrackNamespace{"foo"}, NotNull(), _))
      .WillOnce([&](const TrackNamespace&, const MessageParameters* params,
                    MoqtResponseCallback callback) {
        EXPECT_TRUE(params != nullptr && *params == parameters);
        std::move(callback)(MessageParameters());
      });
  testing::MockFunction<void(
      std::variant<MessageParameters, MoqtRequestErrorInfo>)>
      response_callback;
  testing::MockFunction<void()> cancel_callback;
  client_->session()->PublishNamespace(TrackNamespace{"foo"}, parameters,
                                       response_callback.AsStdFunction(),
                                       cancel_callback.AsStdFunction());
  bool matches = false;
  EXPECT_CALL(response_callback, Call(testing::VariantWith<MessageParameters>(
                                     testing::Eq(MessageParameters()))))
      .WillOnce([&](std::variant<MessageParameters, MoqtRequestErrorInfo>) {
        matches = true;
      });
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return matches; });
  EXPECT_TRUE(success);
  matches = false;
  EXPECT_CALL(cancel_callback, Call).WillOnce([&]() { matches = true; });
  // Resetting the stream will trigger the removal callback.
  EXPECT_CALL(server_callbacks_.incoming_publish_namespace_callback,
              Call(TrackNamespace{"foo"}, IsNull(), _));
  server_->session()->PublishNamespaceCancel(TrackNamespace{"foo"},
                                             kResetCodeCancelled);
  success = test_harness_.RunUntilWithDefaultTimeout([&]() { return matches; });
  EXPECT_TRUE(success);
}

TEST_F(MoqtIntegrationTest, PublishNamespaceSuccessSubscribeInResponse) {
  EstablishSession();
  TrackNamespace prefix{"foo"};
  MessageParameters parameters;
  parameters.authorization_tokens.emplace_back(AuthTokenType::kOutOfBand,
                                               "foo");
  EXPECT_CALL(server_callbacks_.incoming_publish_namespace_callback,
              Call(TrackNamespace{"foo"}, NotNull(), _))
      .WillOnce([&](const TrackNamespace& track_namespace,
                    const MessageParameters* params,
                    MoqtResponseCallback callback) {
        EXPECT_TRUE(params != nullptr && *params == parameters);
        std::move(callback)(MessageParameters());
        absl::StatusOr<FullTrackName> track_name =
            FullTrackName::Create(track_namespace, "/catalog");
        QUICHE_ASSERT_OK(track_name.status());
        MessageParameters parameters(LocationFilter::NextObjectFilter());
        server_->session()->Subscribe(*track_name, &subscribe_visitor_,
                                      parameters);
      });
  testing::MockFunction<void(
      std::variant<MessageParameters, MoqtRequestErrorInfo>)>
      response_callback;
  client_->session()->PublishNamespace(
      prefix, parameters, response_callback.AsStdFunction(), []() {});
  bool matches = false;
  EXPECT_CALL(response_callback, Call)
      .WillOnce(
          [&](std::variant<MessageParameters, MoqtRequestErrorInfo> response) {
            EXPECT_TRUE(std::holds_alternative<MessageParameters>(response));
          });
  EXPECT_CALL(subscribe_visitor_, OnReply).WillOnce([&]() { matches = true; });
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return matches; });
  EXPECT_TRUE(success);
  // Teardown will invoke the close callbacks.
  EXPECT_CALL(server_callbacks_.incoming_publish_namespace_callback,
              Call(TrackNamespace{"foo"}, IsNull(), _));
}

TEST_F(MoqtIntegrationTest, PublishNamespaceSuccessSendDataInResponse) {
  EstablishSession();

  // Set up the server to subscribe to "data" track for the namespace
  // publish_namespace it receives.
  MessageParameters parameters;
  parameters.authorization_tokens.emplace_back(AuthTokenType::kOutOfBand,
                                               "foo");
  EXPECT_CALL(server_callbacks_.incoming_publish_namespace_callback,
              Call(TrackNamespace{"test"}, NotNull(), _))
      .WillOnce([&](const TrackNamespace& track_namespace,
                    const MessageParameters* params,
                    MoqtResponseCallback callback) {
        absl::StatusOr<FullTrackName> track_name =
            FullTrackName::Create(track_namespace, "data");
        QUICHE_ASSERT_OK(track_name.status());
        std::move(callback)(MessageParameters());
        MessageParameters parameters;
        server_->session()->Subscribe(*track_name, &subscribe_visitor_,
                                      parameters);
      });

  auto queue =
      std::make_shared<MoqtOutgoingQueue>(FullTrackName{"test", "data"});
  MoqtKnownTrackPublisher known_track_publisher;
  known_track_publisher.Add(queue);
  client_->session()->set_publisher(&known_track_publisher);
  bool received_subscribe_ok = false;
  EXPECT_CALL(subscribe_visitor_, OnReply).WillOnce([&]() {
    received_subscribe_ok = true;
  });
  client_->session()->PublishNamespace(
      TrackNamespace{"test"}, parameters,
      [](std::variant<MessageParameters, MoqtRequestErrorInfo>) {}, []() {});
  bool success = test_harness_.RunUntilWithDefaultTimeout(
      [&]() { return received_subscribe_ok; });
  EXPECT_TRUE(success);
  success = false;

  queue->AddObject(MemSliceFromString("object data"), /*key=*/true);
  bool received_object = false;
  EXPECT_CALL(subscribe_visitor_, OnObjectFragment)
      .WillOnce([&](const FullTrackName& full_track_name,
                    const PublishedObjectMetadata& metadata,
                    absl::string_view object, uint64_t offset) {
        EXPECT_EQ(full_track_name, FullTrackName("test", "data"));
        EXPECT_EQ(metadata.location.group, 0u);
        EXPECT_EQ(metadata.location.object, 0u);
        EXPECT_EQ(metadata.status, MoqtObjectStatus::kNormal);
        EXPECT_EQ(object, "object data");
        EXPECT_EQ(offset, 0u);
        received_object = true;
      });
  success = test_harness_.RunUntilWithDefaultTimeout(
      [&]() { return received_object; });
  EXPECT_TRUE(success);
  // Teardown will invoke the close callbacks.
  EXPECT_CALL(server_callbacks_.incoming_publish_namespace_callback,
              Call(TrackNamespace{"test"}, IsNull(), _));
}

TEST_F(MoqtIntegrationTest, SendMultipleGroups) {
  EstablishSession();
  MoqtKnownTrackPublisher publisher;
  server_->session()->set_publisher(&publisher);
  MessageParameters parameters(LocationFilter::NextObjectFilter());

  for (MoqtForwardingPreference forwarding_preference :
       {MoqtForwardingPreference::kSubgroup,
        MoqtForwardingPreference::kDatagram}) {
    SCOPED_TRACE(MoqtForwardingPreferenceToString(forwarding_preference));
    std::string name =
        absl::StrCat("pref_", static_cast<int>(forwarding_preference));
    auto queue =
        std::make_shared<MoqtOutgoingQueue>(FullTrackName{"test", name});
    publisher.Add(queue);

    // These will not be delivered.
    queue->AddObject(MemSliceFromString("object 1"), /*key=*/true);
    queue->AddObject(MemSliceFromString("object 2"), /*key=*/false);
    queue->AddObject(MemSliceFromString("object 3"), /*key=*/false);
    client_->session()->Subscribe(FullTrackName("test", name),
                                  &subscribe_visitor_, parameters);
    std::optional<Location> largest_id;
    EXPECT_CALL(subscribe_visitor_, OnReply)
        .WillOnce(
            [&](const FullTrackName&,
                std::variant<SubscribeOkData, MoqtRequestErrorInfo> response) {
              EXPECT_TRUE(std::holds_alternative<SubscribeOkData>(response));
              largest_id =
                  std::get<SubscribeOkData>(response).parameters.largest_object;
            });
    bool success = test_harness_.RunUntilWithDefaultTimeout([&]() {
      return largest_id.has_value() && *largest_id == Location(0, 2);
    });
    EXPECT_TRUE(success);

    int received = 0;
    EXPECT_CALL(
        subscribe_visitor_,
        OnObjectFragment(_,
                         MetadataLocationAndStatus(
                             Location{0, 3}, MoqtObjectStatus::kEndOfGroup),
                         "", /*offset=*/0))
        .WillOnce([&] { ++received; });
    EXPECT_CALL(subscribe_visitor_,
                OnObjectFragment(_,
                                 MetadataLocationAndStatus(
                                     Location{1, 0}, MoqtObjectStatus::kNormal),
                                 "object 4", /*offset=*/0))
        .WillOnce([&] { ++received; });
    queue->AddObject(MemSliceFromString("object 4"), /*key=*/true);
    EXPECT_CALL(subscribe_visitor_,
                OnObjectFragment(_,
                                 MetadataLocationAndStatus(
                                     Location{1, 1}, MoqtObjectStatus::kNormal),
                                 "object 5", /*offset=*/0))
        .WillOnce([&] { ++received; });
    queue->AddObject(MemSliceFromString("object 5"), /*key=*/false);

    success = test_harness_.RunUntilWithDefaultTimeout(
        [&]() { return received >= 3; });
    EXPECT_TRUE(success);

    EXPECT_CALL(subscribe_visitor_,
                OnObjectFragment(_,
                                 MetadataLocationAndStatus(
                                     Location{1, 2}, MoqtObjectStatus::kNormal),
                                 "object 6", /*offset=*/0))
        .WillOnce([&] { ++received; });
    queue->AddObject(MemSliceFromString("object 6"), /*key=*/false);
    EXPECT_CALL(
        subscribe_visitor_,
        OnObjectFragment(_,
                         MetadataLocationAndStatus(
                             Location{1, 3}, MoqtObjectStatus::kEndOfGroup),
                         "", /*offset=*/0))
        .WillOnce([&] { ++received; });
    EXPECT_CALL(subscribe_visitor_,
                OnObjectFragment(_,
                                 MetadataLocationAndStatus(
                                     Location{2, 0}, MoqtObjectStatus::kNormal),
                                 "object 7", /*offset=*/0))
        .WillOnce([&] { ++received; });
    queue->AddObject(MemSliceFromString("object 7"), /*key=*/true);
    EXPECT_CALL(subscribe_visitor_,
                OnObjectFragment(_,
                                 MetadataLocationAndStatus(
                                     Location{2, 1}, MoqtObjectStatus::kNormal),
                                 "object 8", /*offset=*/0))
        .WillOnce([&] { ++received; });
    queue->AddObject(MemSliceFromString("object 8"), /*key=*/false);

    success = test_harness_.RunUntilWithDefaultTimeout(
        [&]() { return received >= 7; });
    EXPECT_TRUE(success);

    EXPECT_CALL(
        subscribe_visitor_,
        OnObjectFragment(_,
                         MetadataLocationAndStatus(
                             Location{2, 2}, MoqtObjectStatus::kEndOfGroup),
                         "", /*offset=*/0))
        .WillOnce([&] { ++received; });
    EXPECT_CALL(
        subscribe_visitor_,
        OnObjectFragment(_,
                         MetadataLocationAndStatus(
                             Location{3, 0}, MoqtObjectStatus::kEndOfTrack),
                         "", /*offset=*/0))
        .WillOnce([&] { ++received; });
    queue->Close();
    success = test_harness_.RunUntilWithDefaultTimeout(
        [&]() { return received >= 9; });
    EXPECT_TRUE(success);
  }
}

TEST_F(MoqtIntegrationTest, FetchItemsFromPast) {
  EstablishSession();
  MoqtKnownTrackPublisher publisher;
  server_->session()->set_publisher(&publisher);

  FullTrackName full_track_name("test", "fetch");
  auto queue = std::make_shared<MoqtOutgoingQueue>(full_track_name);
  publisher.Add(queue);
  for (int i = 0; i < 100; ++i) {
    queue->AddObject(MemSliceFromString("object"), /*key=*/true);
  }
  bool fetch_ok = false;
  std::unique_ptr<MoqtFetchTask> fetch = client_->session()->Fetch(
      full_track_name,
      [&](std::variant<FetchOkData, MoqtRequestErrorInfo> response) {
        fetch_ok = std::holds_alternative<FetchOkData>(response);
      },
      Location{0, 0}, 99, std::nullopt, MessageParameters());
  ASSERT_NE(fetch, nullptr);
  bool eof = false;
  std::vector<PublishedObject> objects;
  fetch->SetObjectAvailableCallback([&]() {
    PublishedObject object;
    while (true) {
      MoqtFetchTask::GetNextObjectResult result = fetch->GetNextObject(object);
      if (result == MoqtFetchTask::GetNextObjectResult::kSuccess) {
        objects.push_back(std::move(object));
      } else if (result == MoqtFetchTask::GetNextObjectResult::kEof) {
        eof = true;
        break;
      } else {
        break;
      }
    }
  });
  // Run until we get FETCH_OK and all objects until EOF.
  bool success = test_harness_.RunUntilWithDefaultTimeout(
      [&]() { return fetch_ok && eof; });
  EXPECT_TRUE(success);

  EXPECT_TRUE(fetch->GetStatus().ok());
  EXPECT_EQ(objects.size(), 3);
  Location expected{97, 0};
  for (const PublishedObject& object : objects) {
    EXPECT_EQ(object.metadata.location, expected);
    EXPECT_EQ(object.metadata.status, MoqtObjectStatus::kNormal);
    EXPECT_EQ(object.payload[0].AsStringView(), "object");
    ++expected.group;
  }
  EXPECT_EQ(expected, Location(100, 0));
}

TEST_F(MoqtIntegrationTest, PublishNamespaceFailure) {
  EstablishSession();
  testing::MockFunction<void(
      std::variant<MessageParameters, MoqtRequestErrorInfo>)>
      response_callback;
  client_->session()->PublishNamespace(
      TrackNamespace{"foo"}, MessageParameters(),
      response_callback.AsStdFunction(), []() {});
  bool matches = false;
  EXPECT_CALL(response_callback, Call)
      .WillOnce(
          [&](std::variant<MessageParameters, MoqtRequestErrorInfo> response) {
            matches = true;
            ASSERT_TRUE(std::holds_alternative<MoqtRequestErrorInfo>(response));
            const MoqtRequestErrorInfo& error =
                std::get<MoqtRequestErrorInfo>(response);
            EXPECT_EQ(error.error_code, RequestErrorCode::kNotSupported);
          });
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return matches; });
  EXPECT_TRUE(success);
}

TEST_F(MoqtIntegrationTest, SubscribeAbsoluteOk) {
  EstablishSession();
  FullTrackName full_track_name("foo", "bar");

  MoqtKnownTrackPublisher publisher;
  server_->session()->set_publisher(&publisher);
  // TODO(martinduke): Unmock this.
  auto track_publisher = std::make_shared<MockTrackPublisher>(full_track_name);
  publisher.Add(track_publisher);

  bool received_ok = false;
  ON_CALL(*track_publisher, expiration).WillByDefault(Return(std::nullopt));
  EXPECT_CALL(*track_publisher, AddObjectListener)
      .WillOnce([&](MoqtObjectListener* listener, const MessageParameters&) {
        listener->OnSubscribeAccepted();
      });
  EXPECT_CALL(subscribe_visitor_, OnReply)
      .WillOnce(
          [&](const FullTrackName&,
              std::variant<SubscribeOkData, MoqtRequestErrorInfo> response) {
            received_ok = std::holds_alternative<SubscribeOkData>(response);
          });
  MessageParameters parameters;
  client_->session()->Subscribe(full_track_name, &subscribe_visitor_,
                                parameters);
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return received_ok; });
  EXPECT_TRUE(success);
}

TEST_F(MoqtIntegrationTest, SubscribeCurrentObjectOk) {
  EstablishSession();
  FullTrackName full_track_name("foo", "bar");

  MoqtKnownTrackPublisher publisher;
  server_->session()->set_publisher(&publisher);
  // TODO(martinduke): Unmock this.
  auto track_publisher = std::make_shared<MockTrackPublisher>(full_track_name);
  publisher.Add(track_publisher);

  bool received_ok = false;
  ON_CALL(*track_publisher, expiration)
      .WillByDefault(Return(quic::QuicTimeDelta::Zero()));
  EXPECT_CALL(*track_publisher, AddObjectListener)
      .WillOnce([&](MoqtObjectListener* listener, const MessageParameters&) {
        listener->OnSubscribeAccepted();
      });
  EXPECT_CALL(subscribe_visitor_, OnReply)
      .WillOnce(
          [&](const FullTrackName&,
              std::variant<SubscribeOkData, MoqtRequestErrorInfo> response) {
            received_ok = std::holds_alternative<SubscribeOkData>(response);
          });
  MessageParameters parameters(LocationFilter::NextObjectFilter());
  client_->session()->Subscribe(full_track_name, &subscribe_visitor_,
                                parameters);
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return received_ok; });
  EXPECT_TRUE(success);
}

TEST_F(MoqtIntegrationTest, SubscribeNextGroupOk) {
  EstablishSession();
  FullTrackName full_track_name("foo", "bar");

  MoqtKnownTrackPublisher publisher;
  server_->session()->set_publisher(&publisher);
  // TODO(martinduke): Unmock this.
  auto track_publisher = std::make_shared<MockTrackPublisher>(full_track_name);
  publisher.Add(track_publisher);

  bool received_ok = false;
  ON_CALL(*track_publisher, expiration)
      .WillByDefault(Return(quic::QuicTimeDelta::Zero()));
  EXPECT_CALL(*track_publisher, AddObjectListener)
      .WillOnce([&](MoqtObjectListener* listener, const MessageParameters&) {
        listener->OnSubscribeAccepted();
      });
  EXPECT_CALL(subscribe_visitor_, OnReply)
      .WillOnce(
          [&](const FullTrackName&,
              std::variant<SubscribeOkData, MoqtRequestErrorInfo> response) {
            received_ok = std::holds_alternative<SubscribeOkData>(response);
          });
  MessageParameters parameters(LocationFilter::RelativeStartFilter(0));
  client_->session()->Subscribe(full_track_name, &subscribe_visitor_,
                                parameters);
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return received_ok; });
  EXPECT_TRUE(success);
}

TEST_F(MoqtIntegrationTest, SubscribeError) {
  EstablishSession();
  FullTrackName full_track_name("foo", "bar");
  bool received_ok = false;
  EXPECT_CALL(subscribe_visitor_, OnReply)
      .WillOnce(
          [&](const FullTrackName&,
              std::variant<SubscribeOkData, MoqtRequestErrorInfo> response) {
            received_ok =
                std::holds_alternative<MoqtRequestErrorInfo>(response);
          });
  MessageParameters parameters(LocationFilter::NextObjectFilter());
  client_->session()->Subscribe(full_track_name, &subscribe_visitor_,
                                parameters);
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return received_ok; });
  EXPECT_TRUE(success);
}

TEST_F(MoqtIntegrationTest, CleanPublishDone) {
  EstablishSession();
  FullTrackName full_track_name("foo", "bar");

  MoqtKnownTrackPublisher publisher;
  server_->session()->set_publisher(&publisher);
  auto queue = std::make_shared<TestTrackPublisher>(full_track_name);
  publisher.Add(queue);

  SubscribeLatestObject(full_track_name, &subscribe_visitor_);

  // Deliver 3 objects on 2 streams.
  queue->AddObject(Location(0, 0), 0, "object,0,0", false);
  queue->AddObject(Location(0, 1), 0, "object,0,1", true);
  queue->AddObject(Location(1, 0), 0, "object,1,0", true);
  int received = 0;
  EXPECT_CALL(subscribe_visitor_, OnObjectFragment).WillRepeatedly([&]() {
    ++received;
  });
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return received == 3; });
  EXPECT_TRUE(success);

  // Reject this subscribe because there already is one.
  MessageParameters parameters(LocationFilter::NextObjectFilter());
  EXPECT_FALSE(client_->session()->Subscribe(full_track_name,
                                             &subscribe_visitor_, parameters));
  queue->RemoveAllSubscriptions();  // Induce a PUBLISH_DONE.
  bool publish_done = false;
  EXPECT_CALL(subscribe_visitor_, OnPublishDone).WillOnce([&]() {
    publish_done = true;
  });
  success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return publish_done; });
  EXPECT_TRUE(success);
  // Subscription is deleted; the client session should not immediately reject
  // a new attempt.
  EXPECT_TRUE(client_->session()->Subscribe(full_track_name,
                                            &subscribe_visitor_, parameters));
  EXPECT_CALL(subscribe_visitor_, OnPublishDone);  // Test teardown
}

TEST_F(MoqtIntegrationTest, ObjectAcks) {
  CreateDefaultEndpoints();
  WireUpEndpoints();
  client_->session()->set_support_object_acks(true);
  server_->session()->set_support_object_acks(true);
  ConnectEndpoints();

  FullTrackName full_track_name("foo", "bar");

  MoqtKnownTrackPublisher publisher;
  server_->session()->set_publisher(&publisher);
  auto track_publisher = std::make_shared<MoqtOutgoingQueue>(
      full_track_name, test_harness_.simulator().GetClock());
  publisher.Add(track_publisher);

  testing::StrictMock<MockPublishingMonitorInterface> monitoring;
  server_->session()->SetMonitoringInterfaceForTrack(full_track_name,
                                                     &monitoring);

  MoqtObjectAckFunction ack_function = nullptr;
  EXPECT_CALL(subscribe_visitor_, OnCanAckObjects(_))
      .WillOnce([&](MoqtObjectAckFunction new_ack_function) {
        ack_function = std::move(new_ack_function);
      });
  EXPECT_CALL(subscribe_visitor_, OnReply)
      .WillOnce([&](const FullTrackName&,
                    std::variant<SubscribeOkData, MoqtRequestErrorInfo>) {
        ack_function(10, 20, quic::QuicTimeDelta::FromMicroseconds(-123));
        ack_function(100, 200, quic::QuicTimeDelta::FromMicroseconds(456));
      });

  MessageParameters parameters(LocationFilter::NextObjectFilter());
  parameters.oack_window_size = quic::QuicTimeDelta::FromMilliseconds(100);
  client_->session()->Subscribe(full_track_name, &subscribe_visitor_,
                                parameters);
  EXPECT_CALL(monitoring, OnObjectAckSupportKnown(parameters.oack_window_size));
  EXPECT_CALL(monitoring,
              OnObjectAckReceived(Location(10, 20),
                                  quic::QuicTimeDelta::FromMicroseconds(-123)));
  bool done = false;
  EXPECT_CALL(monitoring,
              OnObjectAckReceived(Location(100, 200),
                                  quic::QuicTimeDelta::FromMicroseconds(456)))
      .WillOnce([&] { done = true; });
  bool success = test_harness_.RunUntilWithDefaultTimeout([&] { return done; });
  EXPECT_TRUE(success);

  EXPECT_CALL(monitoring, OnNewObjectEnqueued(Location(0, 0)));
  track_publisher->AddObject(QuicheMemSlice::Copy("test"), true);

  const quic_trace::Trace& trace = *server_->trace_visitor()->trace();
  std::vector<int64_t> ack_deltas;
  for (const quic_trace::Event& event : trace.events()) {
    if (event.event_type() == quic_trace::EventType::MOQT_OBJECT_ACKNOWLEDGED) {
      ack_deltas.push_back(event.moq_object_ack_time_delta_us());
    }
  }
  EXPECT_THAT(ack_deltas, ElementsAre(-123, 456));
}

TEST_F(MoqtIntegrationTest, SubgroupDeliveryTimeout) {
  EstablishSession();
  // The loss is added after the handshake, to ensure that the connection has
  // enough congestion control window to work with for the further tests.
  WireUpEndpointsWithLoss(/*lose_every_n=*/4);
  FullTrackName full_track_name("foo", "bar");

  MoqtKnownTrackPublisher publisher;
  server_->session()->set_publisher(&publisher);
  auto queue = std::make_shared<TestTrackPublisher>(full_track_name);
  auto track_publisher = std::make_shared<MockTrackPublisher>(full_track_name);
  publisher.Add(queue);

  bool received_ok = false;
  EXPECT_CALL(subscribe_visitor_, OnReply)
      .WillOnce(
          [&](const FullTrackName&,
              std::variant<SubscribeOkData, MoqtRequestErrorInfo> response) {
            received_ok = std::holds_alternative<SubscribeOkData>(response);
          });
  bool stream_reset = false;
  EXPECT_CALL(subscribe_visitor_, OnStreamReset).WillOnce([&]() {
    stream_reset = true;
  });
  MessageParameters parameters(LocationFilter::NextObjectFilter());
  // Set delivery timeout to ~ 1 RTT: any loss is fatal.
  parameters.subgroup_delivery_timeout =
      quic::QuicTimeDelta::FromMilliseconds(100);
  client_->session()->Subscribe(full_track_name, &subscribe_visitor_,
                                parameters);
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return received_ok; });
  EXPECT_TRUE(success);

  // Publish 4 large objects with a FIN. One of them will be lost.
  std::string data(1000, '\0');
  size_t bytes_received = 0;
  EXPECT_CALL(subscribe_visitor_, OnObjectFragment)
      .WillRepeatedly(
          [&](const FullTrackName&, const PublishedObjectMetadata& metadata,
              absl::string_view object,
              uint64_t offset) { bytes_received += object.size(); });
  quic::QuicTime now = test_harness_.simulator().GetClock()->Now();
  queue->AddObject(Location{0, 0}, 0, data, false, now);
  queue->AddObject(Location{0, 1}, 0, data, false, now);
  queue->AddObject(Location{0, 2}, 0, data, false, now);
  queue->AddObject(Location{0, 3}, 0, data, true, now);
  success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return stream_reset; });
  EXPECT_TRUE(success);
  // Stream was reset before all the bytes arrived.
  EXPECT_LT(bytes_received, 4000);
}

// An object arrives, but the session doesn't grant stream credit until it's
// too late.
TEST_F(MoqtIntegrationTest, ObjectDeliveryTimeout) {
  EstablishSession();
  FullTrackName full_track_name("foo", "bar");

  MoqtKnownTrackPublisher publisher;
  server_->session()->set_publisher(&publisher);
  auto queue = std::make_shared<TestTrackPublisher>(full_track_name);
  auto track_publisher = std::make_shared<MockTrackPublisher>(full_track_name);
  publisher.Add(queue);

  bool received_ok = false;
  EXPECT_CALL(subscribe_visitor_, OnReply)
      .WillOnce(
          [&](const FullTrackName&,
              std::variant<SubscribeOkData, MoqtRequestErrorInfo> response) {
            received_ok = std::holds_alternative<SubscribeOkData>(response);
          });
  bool stream_reset = false;
  EXPECT_CALL(subscribe_visitor_, OnStreamReset).WillOnce([&]() {
    stream_reset = true;
  });
  MessageParameters parameters(LocationFilter::NextObjectFilter());
  // Set delivery timeout to ~ 1 RTT.
  parameters.object_delivery_timeout =
      quic::QuicTimeDelta::FromMilliseconds(100);
  ON_CALL(*track_publisher, expiration)
      .WillByDefault(Return(quic::QuicTimeDelta::Zero()));
  client_->session()->Subscribe(full_track_name, &subscribe_visitor_,
                                parameters);
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return received_ok; });
  EXPECT_TRUE(success);
  success = false;

  std::string data(65535, '\0');
  size_t bytes_received = 0;
  EXPECT_CALL(subscribe_visitor_, OnObjectFragment)
      .WillRepeatedly(
          [&](const FullTrackName&, const PublishedObjectMetadata& metadata,
              absl::string_view object,
              uint64_t offset) { bytes_received += object.size(); });
  quic::QuicTime now = test_harness_.simulator().GetClock()->Now();
  // Send a small object that allows the subscriber to register the stream with
  // this subgroup.
  queue->AddObject(Location{0, 0}, 0, absl::string_view(data.data(), 100),
                   false, now);
  // Send a huge object that will take more than 1 RTT to deliver. This clogs up
  // the QUIC buffer and forces the next object to remain at the MOQT layer.
  queue->AddObject(Location{0, 1}, 0, data, false, now);
  // The third object will have to wait for an RTT, causing a timeout and reset.
  queue->AddObject(Location{0, 2}, 0, absl::string_view(data.data(), 100), true,
                   now);
  success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return stream_reset; });
  EXPECT_TRUE(success);
  // Partial object delivery is disabled, so only the first object is received.
  EXPECT_EQ(bytes_received, 100);
  // On teardown, streams are destroyed in arbitrary order. If the uni stream
  // is destroyed before the bidi stream, there will be a second stream reset
  // notification to the visitor. If not, there won't be.
  EXPECT_CALL(subscribe_visitor_, OnStreamReset).Times(testing::AnyNumber());
}

TEST_F(MoqtIntegrationTest, BandwidthProbe) {
  EstablishSession();
  MoqtProbeManager probe_manager(client_->session()->session(),
                                 test_harness_.simulator().GetClock(),
                                 *test_harness_.simulator().GetAlarmFactory(),
                                 &client_->session()->trace_recorder());

  constexpr quic::QuicBandwidth kModelBandwidth =
      quic::simulator::TestHarness::kServerBandwidth;
  constexpr quic::QuicByteCount kProbeSize = 1024 * 1024;
  constexpr quic::QuicTimeDelta kProbeTimeout =
      kModelBandwidth.TransferTime(kProbeSize) * 10;
  bool probe_done = false;
  std::optional<ProbeId> probe_id = probe_manager.StartProbe(
      kProbeSize, kProbeTimeout, [&probe_done](const ProbeResult& result) {
        probe_done = true;
        EXPECT_EQ(result.status, ProbeStatus::kSuccess);
      });
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return probe_done; });
  EXPECT_TRUE(success);

  int probe_streams = 0;
  for (const quic_trace::StreamAnnotation& annotation :
       client_->trace_visitor()->trace()->stream_annotations()) {
    if (annotation.has_moqt_probe_stream()) {
      ++probe_streams;
      EXPECT_EQ(probe_id, annotation.moqt_probe_stream().probe_id());
    }
  }
  EXPECT_EQ(probe_streams, 1);
}

TEST_F(MoqtIntegrationTest, RecordTrace) {
  constexpr absl::string_view kObjectPayload = "object";
  EstablishSession();
  MoqtKnownTrackPublisher publisher;
  client_->session()->set_publisher(&publisher);

  auto queue =
      std::make_shared<MoqtOutgoingQueue>(FullTrackName{"test", "subgroup"});
  publisher.Add(queue);

  MessageParameters parameters(LocationFilter::NextObjectFilter());
  server_->session()->Subscribe(FullTrackName("test", "subgroup"),
                                &subscribe_visitor_, parameters);
  bool subscribed = false;
  EXPECT_CALL(subscribe_visitor_, OnReply)
      .WillOnce([&](const FullTrackName&,
                    std::variant<SubscribeOkData, MoqtRequestErrorInfo>) {
        subscribed = true;
      });
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return subscribed; });
  EXPECT_TRUE(success);

  queue->AddObject(QuicheMemSlice::Copy(kObjectPayload), /*key=*/true);
  int received = 0;
  EXPECT_CALL(subscribe_visitor_,
              OnObjectFragment(_,
                               MetadataLocationAndStatus(
                                   Location{0, 0}, MoqtObjectStatus::kNormal),
                               kObjectPayload, /*offset=*/0))
      .WillOnce([&] { ++received; });

  success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return received >= 1; });
  EXPECT_TRUE(success);
  const quic_trace::Trace& trace = *client_->trace_visitor()->trace();

  int control_streams = 0;
  int subgroup_streams = 0;
  for (const quic_trace::StreamAnnotation& annotation :
       trace.stream_annotations()) {
    if (annotation.moqt_control_stream()) {
      ++control_streams;
    }
    if (annotation.has_moqt_subgroup_stream()) {
      ++subgroup_streams;
      EXPECT_EQ(annotation.moqt_subgroup_stream().group_id(), 0);
      EXPECT_EQ(annotation.moqt_subgroup_stream().subgroup_id(), 0);
    }
  }
  EXPECT_EQ(control_streams, 2);
  EXPECT_EQ(subgroup_streams, 1);

  int objects_enqueued = 0;
  for (const quic_trace::Event& event : trace.events()) {
    if (event.event_type() == quic_trace::EventType::MOQT_OBJECT_ENQUEUED) {
      ++objects_enqueued;
      ASSERT_TRUE(event.has_moqt_object());
      ASSERT_TRUE(event.moqt_object().has_group_id());
      ASSERT_TRUE(event.moqt_object().has_object_id());
      EXPECT_EQ(event.moqt_object().group_id(), 0);
      EXPECT_EQ(event.moqt_object().object_id(), 0);
      EXPECT_EQ(event.moqt_object().payload_size(), kObjectPayload.size());
      EXPECT_TRUE(event.has_transport_state());
    }
  }
  EXPECT_EQ(objects_enqueued, 1);
}

TEST_F(MoqtIntegrationTest, ClientPublishServerSubscribe) {
  EstablishSession();
  FullTrackName full_track_name("foo", "bar");

  // Server registers incoming publish callback.
  bool server_received_publish = false;
  MoqtResponseCallback server_response_callback;
  server_->session()->callbacks().incoming_publish_callback =
      [&](const FullTrackName& name, const MessageParameters& parameters,
          const TrackProperties& properties, MoqtResponseCallback callback) {
        EXPECT_EQ(name, full_track_name);
        server_response_callback = std::move(callback);
        server_received_publish = true;
        return &subscribe_visitor_;
      };

  // Client publishes.
  auto queue = std::make_shared<TestTrackPublisher>(full_track_name);
  bool client_publish_completed = false;
  bool client_publish_success = false;
  MoqtResponseCallback client_publish_callback =
      [&](std::variant<MessageParameters, MoqtRequestErrorInfo> response) {
        client_publish_completed = true;
        client_publish_success =
            std::holds_alternative<MessageParameters>(response);
      };

  MessageParameters publish_parameters;
  TrackProperties publish_properties;
  bool publish_submitted =
      client_->session()->Publish(queue, publish_parameters, publish_properties,
                                  std::move(client_publish_callback));
  ASSERT_TRUE(publish_submitted);

  // Run until server receives PUBLISH.
  bool success = test_harness_.RunUntilWithDefaultTimeout(
      [&]() { return server_received_publish; });
  ASSERT_TRUE(success);

  // Server responds with REQUEST_OK.
  std::move(server_response_callback)(MessageParameters());

  // Run until client receives REQUEST_OK.
  success = test_harness_.RunUntilWithDefaultTimeout(
      [&]() { return client_publish_completed; });
  ASSERT_TRUE(success);
  EXPECT_TRUE(client_publish_success);

  // Deliver objects.
  queue->AddObject(Location(0, 0), 0, "object0", false);
  queue->AddObject(Location(0, 1), 0, "object1", true);

  int received_objects = 0;
  EXPECT_CALL(subscribe_visitor_, OnObjectFragment)
      .Times(2)
      .WillRepeatedly([&](const FullTrackName& name,
                          const PublishedObjectMetadata& metadata,
                          absl::string_view object, uint64_t offset) {
        EXPECT_EQ(name, full_track_name);
        if (received_objects == 0) {
          EXPECT_EQ(metadata.location, Location(0, 0));
          EXPECT_EQ(object, "object0");
        } else if (received_objects == 1) {
          EXPECT_EQ(metadata.location, Location(0, 1));
          EXPECT_EQ(object, "object1");
        }
        ++received_objects;
      });

  success = test_harness_.RunUntilWithDefaultTimeout(
      [&]() { return received_objects == 2; });
  EXPECT_TRUE(success);

  // Destroy the publisher to induce PUBLISH_DONE.
  queue->RemoveAllSubscriptions();
  bool publish_done = false;
  EXPECT_CALL(subscribe_visitor_, OnPublishDone).WillOnce([&]() {
    publish_done = true;
  });
  success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return publish_done; });
  EXPECT_TRUE(success);
}

// Repro for b/537862681
TEST_F(MoqtIntegrationTest, RelayTwoClientsQueueClose) {
  quic::simulator::Simulator simulator;
  MockSessionCallbacks client1_callbacks;
  MockSessionCallbacks client2_callbacks;
  MockSessionCallbacks relay1_callbacks;
  MockSessionCallbacks relay2_callbacks;
  MoqtRelayPublisher relay_publisher;
  MoqtKnownTrackPublisher client1_publisher;
  MockLiveSubscriberVisitor subscriber_visitor;
  std::optional<quic::simulator::SymmetricLink> link1;
  std::optional<quic::simulator::SymmetricLink> link2;

  // Client 1 (Publisher Client)
  MoqtClientEndpoint client1(&simulator, "Client1", "Relay1",
                             kDefaultMoqtVersion);
  // Relay's server endpoint facing Client 1
  MoqtServerEndpoint relay_endpoint1(&simulator, "Relay1", "Client1",
                                     kDefaultMoqtVersion);
  // Client 2 (Subscriber Client)
  MoqtClientEndpoint client2(&simulator, "Client2", "Relay2",
                             kDefaultMoqtVersion);
  // Relay's server endpoint facing Client 2
  MoqtServerEndpoint relay_endpoint2(&simulator, "Relay2", "Client2",
                                     kDefaultMoqtVersion);

  // Wire up endpoints directly using SymmetricLink (no switch needed)
  link1.emplace(&client1, &relay_endpoint1,
                quic::simulator::TestHarness::kClientBandwidth,
                quic::simulator::TestHarness::kClientPropagationDelay);
  link2.emplace(&client2, &relay_endpoint2,
                quic::simulator::TestHarness::kClientBandwidth,
                quic::simulator::TestHarness::kClientPropagationDelay);

  client1.session()->callbacks() = client1_callbacks.AsSessionCallbacks();
  client1.session()->callbacks().clock = simulator.GetClock();
  client2.session()->callbacks() = client2_callbacks.AsSessionCallbacks();
  client2.session()->callbacks().clock = simulator.GetClock();
  relay_endpoint1.session()->callbacks() =
      relay1_callbacks.AsSessionCallbacks();
  relay_endpoint1.session()->callbacks().clock = simulator.GetClock();
  relay_endpoint2.session()->callbacks() =
      relay2_callbacks.AsSessionCallbacks();
  relay_endpoint2.session()->callbacks().clock = simulator.GetClock();

  // Configure MoqtRelayPublisher at the relay
  relay_endpoint1.session()->set_publisher(&relay_publisher);
  relay_endpoint2.session()->set_publisher(&relay_publisher);

  // Set up publish namespace callbacks to advertise namespace to the relay
  // publisher
  EXPECT_CALL(relay1_callbacks.incoming_publish_namespace_callback, Call)
      .WillRepeatedly([&relay_publisher, &relay_endpoint1](
                          const TrackNamespace& track_namespace,
                          const MessageParameters* parameters,
                          MoqtResponseCallback callback) {
        if (parameters != nullptr) {
          relay_publisher.OnPublishNamespace(track_namespace, *parameters,
                                             relay_endpoint1.session(),
                                             std::move(callback));
        } else {
          relay_publisher.OnPublishNamespaceDone(track_namespace,
                                                 relay_endpoint1.session());
        }
      });

  // Perform handshakes for both client/relay connections
  client1.quic_session()->CryptoConnect();
  client2.quic_session()->CryptoConnect();
  bool client1_established = false;
  bool client2_established = false;
  bool relay1_established = false;
  bool relay2_established = false;
  EXPECT_CALL(client1_callbacks.session_established_callback, Call())
      .WillOnce(Assign(&client1_established, true));
  EXPECT_CALL(client2_callbacks.session_established_callback, Call())
      .WillOnce(Assign(&client2_established, true));
  EXPECT_CALL(relay1_callbacks.session_established_callback, Call())
      .WillOnce(Assign(&relay1_established, true));
  EXPECT_CALL(relay2_callbacks.session_established_callback, Call())
      .WillOnce(Assign(&relay2_established, true));
  bool success = simulator.RunUntilOrTimeout(
      [&]() {
        return client1_established && client2_established &&
               relay1_established && relay2_established;
      },
      quic::simulator::TestHarness::kDefaultTimeout);
  ASSERT_TRUE(success);

  // Client 1 (Publisher) registers its local track publisher
  client1.session()->set_publisher(&client1_publisher);
  FullTrackName track_name("test", "track");
  auto queue =
      std::make_shared<MoqtOutgoingQueue>(track_name, simulator.GetClock());
  client1_publisher.Add(queue);
  // Client 1 publishes namespace "test" to the relay
  bool publish_namespace_ok = false;
  testing::MockFunction<void(
      std::variant<MessageParameters, MoqtRequestErrorInfo>)>
      publish_response_callback;
  EXPECT_CALL(publish_response_callback, Call)
      .WillOnce(
          [&](std::variant<MessageParameters, MoqtRequestErrorInfo> response) {
            publish_namespace_ok =
                std::holds_alternative<MessageParameters>(response);
          });
  client1.session()->PublishNamespace(
      TrackNamespace{"test"}, MessageParameters(),
      publish_response_callback.AsStdFunction(), []() {});
  success = simulator.RunUntilOrTimeout(
      [&]() { return publish_namespace_ok; },
      quic::simulator::TestHarness::kDefaultTimeout);
  ASSERT_TRUE(success);

  // Client 2 subscribes to the track from the relay
  bool subscribe_acknowledged = false;
  EXPECT_CALL(subscriber_visitor, OnReply)
      .WillOnce(
          [&](const FullTrackName&,
              std::variant<SubscribeOkData, MoqtRequestErrorInfo> response) {
            subscribe_acknowledged =
                std::holds_alternative<SubscribeOkData>(response);
          });

  client2.session()->Subscribe(track_name, &subscriber_visitor,
                               MessageParameters());
  success = simulator.RunUntilOrTimeout(
      [&]() { return subscribe_acknowledged; },
      quic::simulator::TestHarness::kDefaultTimeout);
  ASSERT_TRUE(success);

  // Publisher (Client 1) pushes an initial object
  queue->AddObject(quiche::QuicheMemSlice::Copy("object content"), true);
  bool received_object = false;
  EXPECT_CALL(subscriber_visitor, OnObjectFragment)
      .WillOnce([&](const FullTrackName& full_track_name,
                    const PublishedObjectMetadata& metadata,
                    absl::string_view object, uint64_t offset) {
        EXPECT_EQ(full_track_name, track_name);
        EXPECT_EQ(metadata.location.group, 0u);
        EXPECT_EQ(metadata.location.object, 0u);
        EXPECT_EQ(metadata.status, MoqtObjectStatus::kNormal);
        EXPECT_EQ(object, "object content");
        received_object = true;
      });
  success = simulator.RunUntilOrTimeout(
      [&]() { return received_object; },
      quic::simulator::TestHarness::kDefaultTimeout);
  ASSERT_TRUE(success);

  // Now, Client 1 closes the queue
  queue->Close();
  bool received_eog = false;
  bool received_eof = false;
  EXPECT_CALL(subscriber_visitor, OnObjectFragment)
      .WillOnce([&](const FullTrackName& full_track_name,
                    const PublishedObjectMetadata& metadata,
                    absl::string_view object, uint64_t offset) {
        EXPECT_EQ(full_track_name, track_name);
        EXPECT_EQ(metadata.location.group, 0u);
        EXPECT_EQ(metadata.location.object, 1u);
        EXPECT_EQ(metadata.status, MoqtObjectStatus::kEndOfGroup);
        received_eog = true;
      })
      .WillOnce([&](const FullTrackName& full_track_name,
                    const PublishedObjectMetadata& metadata,
                    absl::string_view object, uint64_t offset) {
        EXPECT_EQ(full_track_name, track_name);
        EXPECT_EQ(metadata.location.group, 1u);
        EXPECT_EQ(metadata.location.object, 0u);
        EXPECT_EQ(metadata.status, MoqtObjectStatus::kEndOfTrack);
        received_eof = true;
      });
  success = simulator.RunUntilOrTimeout(
      [&]() { return /*received_eog &&*/ received_eof; },
      quic::simulator::TestHarness::kDefaultTimeout);
  ASSERT_TRUE(success);
}

TEST_F(MoqtIntegrationTest, RelayForwardsObjectAcks) {
  quic::simulator::Simulator simulator;
  MockSessionCallbacks client1_callbacks;
  MockSessionCallbacks client2_callbacks;
  MockSessionCallbacks relay1_callbacks;
  MockSessionCallbacks relay2_callbacks;
  MoqtRelayPublisher relay_publisher;
  MoqtKnownTrackPublisher client1_publisher;
  MockLiveSubscriberVisitor subscriber_visitor;
  std::optional<quic::simulator::SymmetricLink> link1;
  std::optional<quic::simulator::SymmetricLink> link2;

  // Client 1 (original publisher)
  MoqtClientEndpoint client1(&simulator, "Client1", "Relay1",
                             kDefaultMoqtVersion);
  // Relay's server endpoint facing Client 1
  MoqtServerEndpoint relay_endpoint1(&simulator, "Relay1", "Client1",
                                     kDefaultMoqtVersion);
  // Client 2 (end subscriber)
  MoqtClientEndpoint client2(&simulator, "Client2", "Relay2",
                             kDefaultMoqtVersion);
  // Relay's server endpoint facing Client 2
  MoqtServerEndpoint relay_endpoint2(&simulator, "Relay2", "Client2",
                                     kDefaultMoqtVersion);

  link1.emplace(&client1, &relay_endpoint1,
                quic::simulator::TestHarness::kClientBandwidth,
                quic::simulator::TestHarness::kClientPropagationDelay);
  link2.emplace(&client2, &relay_endpoint2,
                quic::simulator::TestHarness::kClientBandwidth,
                quic::simulator::TestHarness::kClientPropagationDelay);

  client1.session()->set_support_object_acks(true);
  client2.session()->set_support_object_acks(true);
  relay_endpoint1.session()->set_support_object_acks(true);
  relay_endpoint2.session()->set_support_object_acks(true);

  client1.session()->callbacks() = client1_callbacks.AsSessionCallbacks();
  client1.session()->callbacks().clock = simulator.GetClock();
  client2.session()->callbacks() = client2_callbacks.AsSessionCallbacks();
  client2.session()->callbacks().clock = simulator.GetClock();
  relay_endpoint1.session()->callbacks() =
      relay1_callbacks.AsSessionCallbacks();
  relay_endpoint1.session()->callbacks().clock = simulator.GetClock();
  relay_endpoint2.session()->callbacks() =
      relay2_callbacks.AsSessionCallbacks();
  relay_endpoint2.session()->callbacks().clock = simulator.GetClock();

  relay_endpoint1.session()->set_publisher(&relay_publisher);
  relay_endpoint2.session()->set_publisher(&relay_publisher);
  relay_publisher.set_oack_window_size(
      quic::QuicTimeDelta::FromMilliseconds(50));
  relay_publisher.SetDefaultUpstreamSession(relay_endpoint1.session());

  client1.quic_session()->CryptoConnect();
  client2.quic_session()->CryptoConnect();
  bool client1_established = false;
  bool client2_established = false;
  bool relay1_established = false;
  bool relay2_established = false;
  EXPECT_CALL(client1_callbacks.session_established_callback, Call())
      .WillOnce(Assign(&client1_established, true));
  EXPECT_CALL(client2_callbacks.session_established_callback, Call())
      .WillOnce(Assign(&client2_established, true));
  EXPECT_CALL(relay1_callbacks.session_established_callback, Call())
      .WillOnce(Assign(&relay1_established, true));
  EXPECT_CALL(relay2_callbacks.session_established_callback, Call())
      .WillOnce(Assign(&relay2_established, true));
  bool success = simulator.RunUntilOrTimeout(
      [&]() {
        return client1_established && client2_established &&
               relay1_established && relay2_established;
      },
      quic::simulator::TestHarness::kDefaultTimeout);
  ASSERT_TRUE(success);

  client1.session()->set_publisher(&client1_publisher);
  FullTrackName track_name("test", "track");
  auto queue =
      std::make_shared<MoqtOutgoingQueue>(track_name, simulator.GetClock());
  client1_publisher.Add(queue);

  testing::StrictMock<MockPublishingMonitorInterface> monitoring;
  client1.session()->SetMonitoringInterfaceForTrack(track_name, &monitoring);

  MoqtObjectAckFunction client2_ack_function = nullptr;
  EXPECT_CALL(subscriber_visitor, OnCanAckObjects(_))
      .WillOnce([&](MoqtObjectAckFunction new_ack_function) {
        client2_ack_function = std::move(new_ack_function);
      });

  bool subscribe_acknowledged = false;
  EXPECT_CALL(subscriber_visitor, OnReply)
      .WillOnce(
          [&](const FullTrackName&,
              std::variant<SubscribeOkData, MoqtRequestErrorInfo> response) {
            subscribe_acknowledged =
                std::holds_alternative<SubscribeOkData>(response);
          });

  MessageParameters parameters(LocationFilter::NextObjectFilter());
  parameters.oack_window_size = quic::QuicTimeDelta::FromMilliseconds(100);
  EXPECT_CALL(monitoring,
              OnObjectAckSupportKnown(std::optional<quic::QuicTimeDelta>(
                  quic::QuicTimeDelta::FromMilliseconds(50))));
  client2.session()->Subscribe(track_name, &subscriber_visitor, parameters);
  success = simulator.RunUntilOrTimeout(
      [&]() {
        return subscribe_acknowledged && client2_ack_function != nullptr;
      },
      quic::simulator::TestHarness::kDefaultTimeout);
  ASSERT_TRUE(success);

  int acks_received = 0;
  EXPECT_CALL(monitoring,
              OnObjectAckReceived(Location(1, 2),
                                  quic::QuicTimeDelta::FromMicroseconds(12345)))
      .WillOnce([&] { ++acks_received; });

  client2_ack_function(1, 2, quic::QuicTimeDelta::FromMicroseconds(12345));

  EXPECT_CALL(monitoring,
              OnObjectAckReceived(Location(3, 4),
                                  quic::QuicTimeDelta::FromMicroseconds(67890)))
      .WillOnce([&] { ++acks_received; });

  client2_ack_function(3, 4, quic::QuicTimeDelta::FromMicroseconds(67890));

  success = simulator.RunUntilOrTimeout(
      [&]() { return acks_received == 2; },
      quic::simulator::TestHarness::kDefaultTimeout);
  EXPECT_TRUE(success);
}

TEST_F(MoqtIntegrationTest, TrackStatusSuccess) {
  EstablishSession();
  FullTrackName track_name("test", "data");
  auto queue = std::make_shared<MoqtOutgoingQueue>(track_name);
  queue->AddObject(quiche::QuicheMemSlice::Copy("object 1"), /*key=*/true);
  queue->AddObject(quiche::QuicheMemSlice::Copy("object 2"), /*key=*/true);
  MoqtKnownTrackPublisher known_track_publisher;
  known_track_publisher.Add(queue);
  server_->session()->set_publisher(&known_track_publisher);

  bool received_response = false;
  MessageParameters received_parameters;
  client_->session()->TrackStatus(
      track_name, MessageParameters(),
      [&](std::variant<TrackStatusOkData, MoqtRequestErrorInfo> response) {
        received_response = true;
        ASSERT_TRUE(std::holds_alternative<TrackStatusOkData>(response));
        received_parameters = std::get<TrackStatusOkData>(response).parameters;
      });

  bool success = test_harness_.RunUntilWithDefaultTimeout(
      [&]() { return received_response; });
  EXPECT_TRUE(success);
  EXPECT_TRUE(received_parameters.largest_object.has_value());
  EXPECT_EQ(received_parameters.largest_object->group, 1u);
  EXPECT_EQ(received_parameters.largest_object->object, 0u);
}

TEST_F(MoqtIntegrationTest, TrackStatusDoesNotExist) {
  EstablishSession();
  FullTrackName track_name("test", "nonexistent");
  MoqtKnownTrackPublisher known_track_publisher;
  server_->session()->set_publisher(&known_track_publisher);

  bool received_response = false;
  MoqtRequestErrorInfo received_error;
  client_->session()->TrackStatus(
      track_name, MessageParameters(),
      [&](std::variant<TrackStatusOkData, MoqtRequestErrorInfo> response) {
        received_response = true;
        ASSERT_TRUE(std::holds_alternative<MoqtRequestErrorInfo>(response));
        received_error = std::get<MoqtRequestErrorInfo>(response);
      });

  bool success = test_harness_.RunUntilWithDefaultTimeout(
      [&]() { return received_response; });
  EXPECT_TRUE(success);
  EXPECT_EQ(received_error.error_code, RequestErrorCode::kDoesNotExist);
}

TEST_F(MoqtIntegrationTest, SubscribeWithNewGroupRequest) {
  EstablishSession();
  MoqtKnownTrackPublisher publisher;
  server_->session()->set_publisher(&publisher);

  FullTrackName track_name("test", "data");
  std::shared_ptr<MoqtOutgoingQueue> queue;
  queue = std::make_shared<MoqtOutgoingQueue>(
      track_name, test_harness_.simulator().GetClock(), [&]() {
        queue->AddObject(quiche::QuicheMemSlice::Copy("object 3"), true);
      });
  publisher.Add(queue);

  // Publish some objects before having any subscribers.
  queue->AddObject(quiche::QuicheMemSlice::Copy("object 1"), /*key=*/true);
  queue->AddObject(quiche::QuicheMemSlice::Copy("object 2"), /*key=*/false);

  MessageParameters parameters(LocationFilter::RelativeStartFilter(0));
  parameters.new_group_request = 0;
  client_->session()->Subscribe(track_name, &subscribe_visitor_, parameters);

  std::optional<Location> largest_id;
  bool dynamic_groups = false;
  EXPECT_CALL(subscribe_visitor_, OnReply)
      .WillOnce(
          [&](const FullTrackName&,
              std::variant<SubscribeOkData, MoqtRequestErrorInfo> response) {
            ASSERT_TRUE(std::holds_alternative<SubscribeOkData>(response));
            const auto& ok_data = std::get<SubscribeOkData>(response);
            largest_id = ok_data.parameters.largest_object;
            dynamic_groups = ok_data.properties.dynamic_groups();
          });
  int received = 0;
  // EndOfGroup object not sent.
  EXPECT_CALL(subscribe_visitor_,
              OnObjectFragment(track_name,
                               MetadataLocationAndStatus(
                                   Location{1, 0}, MoqtObjectStatus::kNormal),
                               "object 3", /*offset=*/0))
      .WillOnce([&] { ++received; });
  bool success = test_harness_.RunUntilWithDefaultTimeout(
      [&]() { return largest_id.has_value() && received == 1; });
  EXPECT_TRUE(success);
  EXPECT_EQ(largest_id, Location(0, 1));
  EXPECT_TRUE(dynamic_groups);
}

TEST_F(MoqtIntegrationTest, PublishWithNewGroupRequest) {
  EstablishSession();
  FullTrackName track_name("test", "data");
  std::shared_ptr<MoqtOutgoingQueue> queue;
  queue = std::make_shared<MoqtOutgoingQueue>(
      track_name, test_harness_.simulator().GetClock(), [&]() {
        queue->AddObject(quiche::QuicheMemSlice::Copy("object 3"), true);
      });

  // Publish some objects before having any subscribers.
  queue->AddObject(quiche::QuicheMemSlice::Copy("object 1"), true);
  queue->AddObject(quiche::QuicheMemSlice::Copy("object 2"), false);

  std::optional<Location> largest_id;
  bool dynamic_groups = false;
  client_->session()->callbacks().incoming_publish_callback =
      [&](const FullTrackName& name, const MessageParameters& parameters,
          const TrackProperties& properties, MoqtResponseCallback callback) {
        EXPECT_EQ(name, track_name);
        largest_id = parameters.largest_object;
        dynamic_groups = properties.dynamic_groups();
        MessageParameters ok_parameters;
        ok_parameters.new_group_request = 0;
        ok_parameters.location_filter = LocationFilter::RelativeStartFilter(0);
        std::move(callback)(ok_parameters);
        return &subscribe_visitor_;
      };
  EXPECT_CALL(subscribe_visitor_, OnReply).Times(1);

  bool publish_ok_received = false;
  ASSERT_TRUE(server_->session()->Publish(
      queue, MessageParameters(), queue->properties(),
      [&](std::variant<MessageParameters, MoqtRequestErrorInfo> response) {
        ASSERT_TRUE(std::holds_alternative<MessageParameters>(response));
        EXPECT_EQ(std::get<MessageParameters>(response).new_group_request, 0u);
        publish_ok_received = true;
      }));

  int received = 0;
  // EndOfGroup object not sent.
  EXPECT_CALL(subscribe_visitor_,
              OnObjectFragment(track_name,
                               MetadataLocationAndStatus(
                                   Location{1, 0}, MoqtObjectStatus::kNormal),
                               "object 3", /*offset=*/0))
      .WillOnce([&] { ++received; });

  bool success = test_harness_.RunUntilWithDefaultTimeout([&]() {
    return publish_ok_received && largest_id.has_value() && received == 1;
  });
  EXPECT_TRUE(success);
  EXPECT_EQ(largest_id, Location(0, 1));
  EXPECT_TRUE(dynamic_groups);
}

TEST_F(MoqtIntegrationTest, MaxUndeliveredObjectExceeded) {
  EstablishSession();
  MoqtSessionPeer::SetMaxUndeliveredObject(client_->session(), 500);
  FullTrackName full_track_name("foo", "bar");

  MoqtKnownTrackPublisher publisher;
  server_->session()->set_publisher(&publisher);
  auto queue = std::make_shared<TestTrackPublisher>(full_track_name);
  publisher.Add(queue);

  SubscribeLatestObject(full_track_name, &subscribe_visitor_);

  EXPECT_CALL(client_callbacks_.session_terminated_callback, Call(_)).Times(0);
  EXPECT_CALL(server_callbacks_.session_terminated_callback, Call(_)).Times(0);

  // Send a small first object on group 0 so the stream index is registered,
  // followed by a 5000-byte object (> 1 MTU and > max_undelivered_object = 500)
  // on group 0, and a small object on group 1 to verify the session stays alive
  // and delivers subsequent streams.
  EXPECT_CALL(subscribe_visitor_,
              OnObjectFragment(full_track_name,
                               MetadataLocationAndStatus(
                                   Location{0, 0}, MoqtObjectStatus::kNormal),
                               "first", /*offset=*/0));
  bool stream_reset = false;
  EXPECT_CALL(subscribe_visitor_,
              OnStreamReset(full_track_name, DataStreamIndex{0, 0}))
      .WillOnce([&]() { stream_reset = true; });
  bool small_received = false;
  EXPECT_CALL(subscribe_visitor_,
              OnObjectFragment(full_track_name,
                               MetadataLocationAndStatus(
                                   Location{1, 0}, MoqtObjectStatus::kNormal),
                               "small", /*offset=*/0))
      .WillOnce([&]() { small_received = true; });
  EXPECT_CALL(subscribe_visitor_,
              OnStreamFin(full_track_name, DataStreamIndex{1, 0}));

  queue->AddObject(Location{0, 0}, 0, "first", false);
  std::string large_data(5000, 'a');
  queue->AddObject(Location{0, 1}, 0, large_data, true);
  queue->AddObject(Location{1, 0}, 0, "small", true);

  bool success = test_harness_.RunUntilWithDefaultTimeout(
      [&]() { return stream_reset && small_received; });
  EXPECT_TRUE(success);
}

}  // namespace

}  // namespace moqt::test
