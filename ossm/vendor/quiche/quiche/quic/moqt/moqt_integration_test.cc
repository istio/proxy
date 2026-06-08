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
#include "quiche/quic/moqt/moqt_session.h"
#include "quiche/quic/moqt/moqt_session_callbacks.h"
#include "quiche/quic/moqt/moqt_session_interface.h"
#include "quiche/quic/moqt/moqt_track.h"
#include "quiche/quic/moqt/moqt_types.h"
#include "quiche/quic/moqt/test_tools/moqt_mock_visitor.h"
#include "quiche/quic/moqt/test_tools/moqt_session_peer.h"
#include "quiche/quic/moqt/test_tools/moqt_simulator_harness.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
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
                             MockSubscribeRemoteTrackVisitor* visitor) {
    bool received_ok = false;
    EXPECT_CALL(*visitor, OnReply)
        .WillOnce(
            [&](const FullTrackName&,
                std::variant<SubscribeOkData, MoqtRequestErrorInfo> response) {
              received_ok = std::holds_alternative<SubscribeOkData>(response);
            });
    MessageParameters parameters(MoqtFilterType::kLargestObject);
    client_->session()->Subscribe(track_name, visitor, parameters);

    bool success =
        test_harness_.RunUntilWithDefaultTimeout([&]() { return received_ok; });
    EXPECT_TRUE(success);
  }

 protected:
  quic::simulator::TestHarness test_harness_;

  MockSessionCallbacks client_callbacks_;
  MockSessionCallbacks server_callbacks_;
  MockSubscribeRemoteTrackVisitor subscribe_visitor_;
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
              Call(TrackNamespace{"foo"}, std::make_optional(parameters), _))
      .WillOnce([](const TrackNamespace&,
                   const std::optional<MessageParameters>&,
                   MoqtResponseCallback callback) {
        std::move(callback)(std::nullopt);
      });
  testing::MockFunction<void(std::optional<MoqtRequestErrorInfo>)>
      response_callback;
  client_->session()->PublishNamespace(TrackNamespace{"foo"}, parameters,
                                       response_callback.AsStdFunction(),
                                       [](MoqtRequestErrorInfo) {});
  bool matches = false;
  EXPECT_CALL(response_callback, Call)
      .WillOnce([&](std::optional<MoqtRequestErrorInfo> error) {
        matches = true;
        EXPECT_FALSE(error.has_value());
      });
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return matches; });
  EXPECT_TRUE(success);
  matches = false;
  EXPECT_CALL(
      server_callbacks_.incoming_publish_namespace_callback,
      Call(TrackNamespace{"foo"}, std::optional<MessageParameters>(), IsNull()))
      .WillOnce([&](const TrackNamespace&,
                    const std::optional<MessageParameters>&,
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
              Call(TrackNamespace{"foo"}, std::make_optional(parameters), _))
      .WillOnce([](const TrackNamespace&,
                   const std::optional<MessageParameters>&,
                   MoqtResponseCallback callback) {
        std::move(callback)(std::nullopt);
      });
  testing::MockFunction<void(std::optional<MoqtRequestErrorInfo>)>
      response_callback;
  testing::MockFunction<void(MoqtRequestErrorInfo)> cancel_callback;
  client_->session()->PublishNamespace(TrackNamespace{"foo"}, parameters,
                                       response_callback.AsStdFunction(),
                                       cancel_callback.AsStdFunction());
  bool matches = false;
  EXPECT_CALL(response_callback, Call(std::optional<MoqtRequestErrorInfo>()))
      .WillOnce(
          [&](std::optional<MoqtRequestErrorInfo> error) { matches = true; });
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return matches; });
  EXPECT_TRUE(success);
  matches = false;
  EXPECT_CALL(cancel_callback,
              Call(MoqtRequestErrorInfo{RequestErrorCode::kInternalError,
                                        std::nullopt, "internal error"}))
      .WillOnce([&](std::optional<MoqtRequestErrorInfo>) { matches = true; });
  server_->session()->PublishNamespaceCancel(TrackNamespace{"foo"},
                                             RequestErrorCode::kInternalError,
                                             "internal error");
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
              Call(TrackNamespace{"foo"}, std::make_optional(parameters), _))
      .WillOnce([&](const TrackNamespace& track_namespace,
                    const std::optional<MessageParameters>&,
                    MoqtResponseCallback callback) {
        std::move(callback)(std::nullopt);
        absl::StatusOr<FullTrackName> track_name =
            FullTrackName::Create(track_namespace, "/catalog");
        QUICHE_ASSERT_OK(track_name.status());
        MessageParameters parameters(MoqtFilterType::kLargestObject);
        server_->session()->Subscribe(*track_name, &subscribe_visitor_,
                                      parameters);
      });
  testing::MockFunction<void(std::optional<MoqtRequestErrorInfo>)>
      response_callback;
  client_->session()->PublishNamespace(prefix, parameters,
                                       response_callback.AsStdFunction(),
                                       [](MoqtRequestErrorInfo) {});
  bool matches = false;
  EXPECT_CALL(response_callback, Call)
      .WillOnce([&](std::optional<MoqtRequestErrorInfo> error) {
        EXPECT_FALSE(error.has_value());
      });
  EXPECT_CALL(subscribe_visitor_, OnReply).WillOnce([&]() { matches = true; });
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return matches; });
  EXPECT_TRUE(success);
  // Session tears down PUBLISH_NAMESPACE.
  EXPECT_CALL(server_callbacks_.incoming_publish_namespace_callback,
              Call(prefix, std::optional<MessageParameters>(), IsNull()));
}

TEST_F(MoqtIntegrationTest, PublishNamespaceSuccessSendDataInResponse) {
  EstablishSession();

  // Set up the server to subscribe to "data" track for the namespace
  // publish_namespace it receives.
  MessageParameters parameters;
  parameters.authorization_tokens.emplace_back(AuthTokenType::kOutOfBand,
                                               "foo");
  EXPECT_CALL(server_callbacks_.incoming_publish_namespace_callback,
              Call(TrackNamespace{"test"}, std::make_optional(parameters), _))
      .WillOnce([&](const TrackNamespace& track_namespace,
                    const std::optional<MessageParameters>&,
                    MoqtResponseCallback callback) {
        absl::StatusOr<FullTrackName> track_name =
            FullTrackName::Create(track_namespace, "data");
        QUICHE_ASSERT_OK(track_name.status());
        std::move(callback)(std::nullopt);
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
      [](std::optional<MoqtRequestErrorInfo>) {}, [](MoqtRequestErrorInfo) {});
  bool success = test_harness_.RunUntilWithDefaultTimeout(
      [&]() { return received_subscribe_ok; });
  EXPECT_TRUE(success);
  success = false;

  queue->AddObject(MemSliceFromString("object data"), /*key=*/true);
  bool received_object = false;
  EXPECT_CALL(subscribe_visitor_, OnObjectFragment)
      .WillOnce([&](const FullTrackName& full_track_name,
                    const PublishedObjectMetadata& metadata,
                    absl::string_view object, bool end_of_message) {
        EXPECT_EQ(full_track_name, FullTrackName("test", "data"));
        EXPECT_EQ(metadata.location.group, 0u);
        EXPECT_EQ(metadata.location.object, 0u);
        EXPECT_EQ(metadata.status, MoqtObjectStatus::kNormal);
        EXPECT_EQ(object, "object data");
        EXPECT_TRUE(end_of_message);
        received_object = true;
      });
  success = test_harness_.RunUntilWithDefaultTimeout(
      [&]() { return received_object; });
  EXPECT_TRUE(success);
  // Session tears down PUBLISH_NAMESPACE.
  EXPECT_CALL(server_callbacks_.incoming_publish_namespace_callback,
              Call(TrackNamespace{"test"}, std::optional<MessageParameters>(),
                   IsNull()));
}

TEST_F(MoqtIntegrationTest, SendMultipleGroups) {
  EstablishSession();
  MoqtKnownTrackPublisher publisher;
  server_->session()->set_publisher(&publisher);
  MessageParameters parameters(MoqtFilterType::kLargestObject);

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
                         "", true))
        .WillOnce([&] { ++received; });
    EXPECT_CALL(subscribe_visitor_,
                OnObjectFragment(_,
                                 MetadataLocationAndStatus(
                                     Location{1, 0}, MoqtObjectStatus::kNormal),
                                 "object 4", true))
        .WillOnce([&] { ++received; });
    queue->AddObject(MemSliceFromString("object 4"), /*key=*/true);
    EXPECT_CALL(subscribe_visitor_,
                OnObjectFragment(_,
                                 MetadataLocationAndStatus(
                                     Location{1, 1}, MoqtObjectStatus::kNormal),
                                 "object 5", true))
        .WillOnce([&] { ++received; });
    queue->AddObject(MemSliceFromString("object 5"), /*key=*/false);

    success = test_harness_.RunUntilWithDefaultTimeout(
        [&]() { return received >= 3; });
    EXPECT_TRUE(success);

    EXPECT_CALL(subscribe_visitor_,
                OnObjectFragment(_,
                                 MetadataLocationAndStatus(
                                     Location{1, 2}, MoqtObjectStatus::kNormal),
                                 "object 6", true))
        .WillOnce([&] { ++received; });
    queue->AddObject(MemSliceFromString("object 6"), /*key=*/false);
    EXPECT_CALL(
        subscribe_visitor_,
        OnObjectFragment(_,
                         MetadataLocationAndStatus(
                             Location{1, 3}, MoqtObjectStatus::kEndOfGroup),
                         "", true))
        .WillOnce([&] { ++received; });
    EXPECT_CALL(subscribe_visitor_,
                OnObjectFragment(_,
                                 MetadataLocationAndStatus(
                                     Location{2, 0}, MoqtObjectStatus::kNormal),
                                 "object 7", true))
        .WillOnce([&] { ++received; });
    queue->AddObject(MemSliceFromString("object 7"), /*key=*/true);
    EXPECT_CALL(subscribe_visitor_,
                OnObjectFragment(_,
                                 MetadataLocationAndStatus(
                                     Location{2, 1}, MoqtObjectStatus::kNormal),
                                 "object 8", true))
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
                         "", true))
        .WillOnce([&] { ++received; });
    EXPECT_CALL(
        subscribe_visitor_,
        OnObjectFragment(_,
                         MetadataLocationAndStatus(
                             Location{3, 0}, MoqtObjectStatus::kEndOfTrack),
                         "", true))
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
  std::unique_ptr<MoqtFetchTask> fetch;
  EXPECT_TRUE(client_->session()->Fetch(
      full_track_name,
      [&](std::unique_ptr<MoqtFetchTask> task) { fetch = std::move(task); },
      Location{0, 0}, 99, std::nullopt, MessageParameters()));
  // Run until we get FETCH_OK.
  bool success = test_harness_.RunUntilWithDefaultTimeout(
      [&]() { return fetch != nullptr; });
  EXPECT_TRUE(success);

  EXPECT_TRUE(fetch->GetStatus().ok());
  MoqtFetchTask::GetNextObjectResult result;
  PublishedObject object;
  Location expected{97, 0};
  do {
    result = fetch->GetNextObject(object);
    if (result == MoqtFetchTask::GetNextObjectResult::kEof) {
      break;
    }
    EXPECT_EQ(result, MoqtFetchTask::GetNextObjectResult::kSuccess);
    EXPECT_EQ(object.metadata.location, expected);
    EXPECT_EQ(object.metadata.status, MoqtObjectStatus::kNormal);
    EXPECT_EQ(object.payload.AsStringView(), "object");
    ++expected.group;
  } while (result == MoqtFetchTask::GetNextObjectResult::kSuccess);
  EXPECT_EQ(result, MoqtFetchTask::GetNextObjectResult::kEof);
  EXPECT_EQ(expected, Location(100, 0));
}

TEST_F(MoqtIntegrationTest, PublishNamespaceFailure) {
  EstablishSession();
  testing::MockFunction<void(std::optional<MoqtRequestErrorInfo>)>
      response_callback;
  client_->session()->PublishNamespace(TrackNamespace{"foo"},
                                       MessageParameters(),
                                       response_callback.AsStdFunction(),
                                       [](MoqtRequestErrorInfo error_info) {});
  bool matches = false;
  EXPECT_CALL(response_callback, Call)
      .WillOnce([&](std::optional<MoqtRequestErrorInfo> error) {
        matches = true;
        ASSERT_TRUE(error.has_value());
        EXPECT_EQ(error->error_code, RequestErrorCode::kNotSupported);
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
      .WillOnce([&](MoqtObjectListener* listener) {
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
      .WillOnce([&](MoqtObjectListener* listener) {
        listener->OnSubscribeAccepted();
      });
  EXPECT_CALL(subscribe_visitor_, OnReply)
      .WillOnce(
          [&](const FullTrackName&,
              std::variant<SubscribeOkData, MoqtRequestErrorInfo> response) {
            received_ok = std::holds_alternative<SubscribeOkData>(response);
          });
  MessageParameters parameters(MoqtFilterType::kLargestObject);
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
      .WillOnce([&](MoqtObjectListener* listener) {
        listener->OnSubscribeAccepted();
      });
  EXPECT_CALL(subscribe_visitor_, OnReply)
      .WillOnce(
          [&](const FullTrackName&,
              std::variant<SubscribeOkData, MoqtRequestErrorInfo> response) {
            received_ok = std::holds_alternative<SubscribeOkData>(response);
          });
  MessageParameters parameters(MoqtFilterType::kNextGroupStart);
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
  MessageParameters parameters(MoqtFilterType::kLargestObject);
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
  MessageParameters parameters(MoqtFilterType::kLargestObject);
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
  EXPECT_CALL(subscribe_visitor_, OnReply)
      .WillOnce(
          [](const FullTrackName&,
             std::variant<SubscribeOkData, MoqtRequestErrorInfo> response) {
            EXPECT_TRUE(std::holds_alternative<MoqtRequestErrorInfo>(response));
          });  // Teardown
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

  MessageParameters parameters(MoqtFilterType::kLargestObject);
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

TEST_F(MoqtIntegrationTest, DeliveryTimeout) {
  CreateDefaultEndpoints();
  WireUpEndpointsWithLoss(/*lose_every_n=*/4);
  ConnectEndpoints();
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
  MessageParameters parameters(MoqtFilterType::kLargestObject);
  // Set delivery timeout to ~ 1 RTT: any loss is fatal.
  parameters.delivery_timeout = quic::QuicTimeDelta::FromMilliseconds(100);
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
              bool end_of_message) { bytes_received += object.size(); });
  queue->AddObject(Location{0, 0}, 0, data, false);
  queue->AddObject(Location{0, 1}, 0, data, false);
  queue->AddObject(Location{0, 2}, 0, data, false);
  queue->AddObject(Location{0, 3}, 0, data, true);
  success = test_harness_.RunUntilWithDefaultTimeout([&]() {
    return MoqtSessionPeer::SubgroupHasBeenReset(
        MoqtSessionPeer::GetSubscription(server_->session(), 0),
        DataStreamIndex{0, 0});
  });
  EXPECT_TRUE(success);
  // Stream was reset before all the bytes arrived.
  EXPECT_LT(bytes_received, 4000);
}

TEST_F(MoqtIntegrationTest, AlternateDeliveryTimeout) {
  EstablishSession();
  FullTrackName full_track_name("foo", "bar");

  MoqtKnownTrackPublisher publisher;
  server_->session()->set_publisher(&publisher);
  server_->session()->UseAlternateDeliveryTimeout();
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
  MessageParameters parameters(MoqtFilterType::kLargestObject);
  // Set delivery timeout to ~ 1 RTT: any loss is fatal.
  parameters.delivery_timeout = quic::QuicTimeDelta::FromMilliseconds(100);
  ON_CALL(*track_publisher, expiration)
      .WillByDefault(Return(quic::QuicTimeDelta::Zero()));
  client_->session()->Subscribe(full_track_name, &subscribe_visitor_,
                                parameters);
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return received_ok; });
  EXPECT_TRUE(success);
  success = false;

  std::string data(1000, '\0');
  size_t bytes_received = 0;
  EXPECT_CALL(subscribe_visitor_, OnObjectFragment)
      .WillRepeatedly(
          [&](const FullTrackName&, const PublishedObjectMetadata& metadata,
              absl::string_view object,
              bool end_of_message) { bytes_received += object.size(); });
  queue->AddObject(Location{0, 0}, 0, data, false);
  queue->AddObject(Location{1, 0}, 0, data, false);
  success = test_harness_.RunUntilWithDefaultTimeout([&]() {
    return MoqtSessionPeer::SubgroupHasBeenReset(
        MoqtSessionPeer::GetSubscription(server_->session(), 0),
        DataStreamIndex{0, 0});
  });
  EXPECT_TRUE(success);
  EXPECT_EQ(bytes_received, 2000);
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

  MessageParameters parameters(MoqtFilterType::kLargestObject);
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
                               kObjectPayload, true))
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
  EXPECT_EQ(control_streams, 1);
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

}  // namespace

}  // namespace moqt::test
