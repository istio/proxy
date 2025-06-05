// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/quic_generic_session.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/moqt/moqt_known_track_publisher.h"
#include "quiche/quic/moqt/moqt_live_relay_queue.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_outgoing_queue.h"
#include "quiche/quic/moqt/moqt_priority.h"
#include "quiche/quic/moqt/moqt_publisher.h"
#include "quiche/quic/moqt/moqt_session.h"
#include "quiche/quic/moqt/moqt_track.h"
#include "quiche/quic/moqt/test_tools/moqt_session_peer.h"
#include "quiche/quic/moqt/test_tools/moqt_simulator_harness.h"
#include "quiche/quic/moqt/tools/moqt_mock_visitor.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/quic/test_tools/simulator/test_harness.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace moqt::test {

namespace {

using ::quic::test::MemSliceFromString;
using ::testing::_;
using ::testing::Assign;
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
    EXPECT_CALL(*visitor, OnReply(track_name, std::optional<FullSequence>(),
                                  std::optional<absl::string_view>()))
        .WillOnce([&]() { received_ok = true; });
    client_->session()->SubscribeCurrentObject(track_name, visitor,
                                               MoqtSubscribeParameters());
    bool success =
        test_harness_.RunUntilWithDefaultTimeout([&]() { return received_ok; });
    EXPECT_TRUE(success);
  }

 protected:
  quic::simulator::TestHarness test_harness_;

  MockSessionCallbacks client_callbacks_;
  MockSessionCallbacks server_callbacks_;
  std::unique_ptr<MoqtClientEndpoint> client_;
  std::unique_ptr<MoqtServerEndpoint> server_;
};

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
  client_ = std::make_unique<MoqtClientEndpoint>(
      &test_harness_.simulator(), "Client", "Server",
      MoqtVersion::kUnrecognizedVersionForTests);
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

TEST_F(MoqtIntegrationTest, AnnounceSuccessThenUnannounce) {
  EstablishSession();
  EXPECT_CALL(server_callbacks_.incoming_announce_callback,
              Call(FullTrackName{"foo"}, AnnounceEvent::kAnnounce))
      .WillOnce(Return(std::nullopt));
  testing::MockFunction<void(
      FullTrackName track_namespace,
      std::optional<MoqtAnnounceErrorReason> error_message)>
      announce_callback;
  client_->session()->Announce(FullTrackName{"foo"},
                               announce_callback.AsStdFunction());
  bool matches = false;
  EXPECT_CALL(announce_callback, Call(_, _))
      .WillOnce([&](FullTrackName track_namespace,
                    std::optional<MoqtAnnounceErrorReason> error) {
        matches = true;
        EXPECT_EQ(track_namespace, FullTrackName{"foo"});
        EXPECT_FALSE(error.has_value());
      });
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return matches; });
  EXPECT_TRUE(success);
  matches = false;
  EXPECT_CALL(server_callbacks_.incoming_announce_callback, Call(_, _))
      .WillOnce([&](FullTrackName name, AnnounceEvent event) {
        matches = true;
        EXPECT_EQ(name, FullTrackName{"foo"});
        EXPECT_EQ(event, AnnounceEvent::kUnannounce);
        return std::nullopt;
      });
  client_->session()->Unannounce(FullTrackName{"foo"});
  success = test_harness_.RunUntilWithDefaultTimeout([&]() { return matches; });
  EXPECT_TRUE(success);
}

TEST_F(MoqtIntegrationTest, AnnounceSuccessThenCancel) {
  EstablishSession();
  EXPECT_CALL(server_callbacks_.incoming_announce_callback,
              Call(FullTrackName{"foo"}, AnnounceEvent::kAnnounce))
      .WillOnce(Return(std::nullopt));
  testing::MockFunction<void(
      FullTrackName track_namespace,
      std::optional<MoqtAnnounceErrorReason> error_message)>
      announce_callback;
  client_->session()->Announce(FullTrackName{"foo"},
                               announce_callback.AsStdFunction());
  bool matches = false;
  EXPECT_CALL(announce_callback, Call(_, _))
      .WillOnce([&](FullTrackName track_namespace,
                    std::optional<MoqtAnnounceErrorReason> error) {
        matches = true;
        EXPECT_EQ(track_namespace, FullTrackName{"foo"});
        EXPECT_FALSE(error.has_value());
      });
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return matches; });
  EXPECT_TRUE(success);
  matches = false;
  EXPECT_CALL(announce_callback, Call(_, _))
      .WillOnce([&](FullTrackName track_namespace,
                    std::optional<MoqtAnnounceErrorReason> error) {
        matches = true;
        EXPECT_EQ(track_namespace, FullTrackName{"foo"});
        ASSERT_TRUE(error.has_value());
        EXPECT_EQ(error->error_code, SubscribeErrorCode::kInternalError);
        EXPECT_EQ(error->reason_phrase, "internal error");
      });
  server_->session()->CancelAnnounce(FullTrackName{"foo"},
                                     SubscribeErrorCode::kInternalError,
                                     "internal error");
  success = test_harness_.RunUntilWithDefaultTimeout([&]() { return matches; });
  EXPECT_TRUE(success);
}

TEST_F(MoqtIntegrationTest, AnnounceSuccessSubscribeInResponse) {
  EstablishSession();
  EXPECT_CALL(server_callbacks_.incoming_announce_callback,
              Call(FullTrackName{"foo"}, AnnounceEvent::kAnnounce))
      .WillOnce(Return(std::nullopt));
  MockSubscribeRemoteTrackVisitor server_visitor;
  testing::MockFunction<void(
      FullTrackName track_namespace,
      std::optional<MoqtAnnounceErrorReason> error_message)>
      announce_callback;
  client_->session()->Announce(FullTrackName{"foo"},
                               announce_callback.AsStdFunction());
  bool matches = false;
  EXPECT_CALL(announce_callback, Call(_, _))
      .WillOnce([&](FullTrackName track_namespace,
                    std::optional<MoqtAnnounceErrorReason> error) {
        EXPECT_EQ(track_namespace, FullTrackName{"foo"});
        FullTrackName track_name = track_namespace;
        track_name.AddElement("/catalog");
        EXPECT_FALSE(error.has_value());
        server_->session()->SubscribeCurrentObject(track_name, &server_visitor,
                                                   MoqtSubscribeParameters());
      });
  EXPECT_CALL(server_visitor, OnReply(_, _, _)).WillOnce([&]() {
    matches = true;
  });
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return matches; });
  EXPECT_TRUE(success);
}

TEST_F(MoqtIntegrationTest, AnnounceSuccessSendDataInResponse) {
  EstablishSession();

  // Set up the server to subscribe to "data" track for the namespace announce
  // it receives.
  MockSubscribeRemoteTrackVisitor server_visitor;
  EXPECT_CALL(server_callbacks_.incoming_announce_callback,
              Call(_, AnnounceEvent::kAnnounce))
      .WillOnce([&](const FullTrackName& track_namespace,
                    AnnounceEvent /*announce*/) {
        FullTrackName track_name = track_namespace;
        track_name.AddElement("data");
        server_->session()->SubscribeAbsolute(
            track_name, /*start_group=*/0, /*start_object=*/0, &server_visitor,
            MoqtSubscribeParameters());
        return std::optional<MoqtAnnounceErrorReason>();
      });

  auto queue = std::make_shared<MoqtOutgoingQueue>(
      FullTrackName{"test", "data"}, MoqtForwardingPreference::kSubgroup);
  MoqtKnownTrackPublisher known_track_publisher;
  known_track_publisher.Add(queue);
  client_->session()->set_publisher(&known_track_publisher);
  bool received_subscribe_ok = false;
  EXPECT_CALL(server_visitor, OnReply(_, _, _)).WillOnce([&]() {
    received_subscribe_ok = true;
  });
  client_->session()->Announce(
      FullTrackName{"test"},
      [](FullTrackName, std::optional<MoqtAnnounceErrorReason>) {});
  bool success = test_harness_.RunUntilWithDefaultTimeout(
      [&]() { return received_subscribe_ok; });
  EXPECT_TRUE(success);
  success = false;

  queue->AddObject(MemSliceFromString("object data"), /*key=*/true);
  bool received_object = false;
  EXPECT_CALL(server_visitor, OnObjectFragment(_, _, _, _, _, _))
      .WillOnce([&](const FullTrackName& full_track_name, FullSequence sequence,
                    MoqtPriority /*publisher_priority*/,
                    MoqtObjectStatus status, absl::string_view object,
                    bool end_of_message) {
        EXPECT_EQ(full_track_name, FullTrackName("test", "data"));
        EXPECT_EQ(sequence.group, 0u);
        EXPECT_EQ(sequence.object, 0u);
        EXPECT_EQ(status, MoqtObjectStatus::kNormal);
        EXPECT_EQ(object, "object data");
        EXPECT_TRUE(end_of_message);
        received_object = true;
      });
  success = test_harness_.RunUntilWithDefaultTimeout(
      [&]() { return received_object; });
  EXPECT_TRUE(success);
}

TEST_F(MoqtIntegrationTest, SendMultipleGroups) {
  EstablishSession();
  MoqtKnownTrackPublisher publisher;
  server_->session()->set_publisher(&publisher);

  for (MoqtForwardingPreference forwarding_preference :
       {MoqtForwardingPreference::kSubgroup,
        MoqtForwardingPreference::kDatagram}) {
    SCOPED_TRACE(MoqtForwardingPreferenceToString(forwarding_preference));
    MockSubscribeRemoteTrackVisitor client_visitor;
    std::string name =
        absl::StrCat("pref_", static_cast<int>(forwarding_preference));
    auto queue = std::make_shared<MoqtOutgoingQueue>(
        FullTrackName{"test", name}, forwarding_preference);
    publisher.Add(queue);

    // These will not be delivered.
    queue->AddObject(MemSliceFromString("object 1"), /*key=*/true);
    queue->AddObject(MemSliceFromString("object 2"), /*key=*/false);
    queue->AddObject(MemSliceFromString("object 3"), /*key=*/false);
    client_->session()->SubscribeCurrentObject(FullTrackName("test", name),
                                               &client_visitor,
                                               MoqtSubscribeParameters());
    std::optional<FullSequence> largest_id;
    EXPECT_CALL(client_visitor, OnReply)
        .WillOnce([&](const FullTrackName& /*name*/,
                      std::optional<FullSequence> id,
                      std::optional<absl::string_view> /*reason*/) {
          largest_id = id;
        });
    bool success = test_harness_.RunUntilWithDefaultTimeout([&]() {
      return largest_id.has_value() && *largest_id == FullSequence(0, 2);
    });
    EXPECT_TRUE(success);

    int received = 0;
    EXPECT_CALL(client_visitor,
                OnObjectFragment(_, FullSequence{0, 3}, _,
                                 MoqtObjectStatus::kEndOfGroup, "", true))
        .WillOnce([&] { ++received; });
    EXPECT_CALL(client_visitor,
                OnObjectFragment(_, FullSequence{1, 0}, _,
                                 MoqtObjectStatus::kNormal, "object 4", true))
        .WillOnce([&] { ++received; });
    queue->AddObject(MemSliceFromString("object 4"), /*key=*/true);
    EXPECT_CALL(client_visitor,
                OnObjectFragment(_, FullSequence{1, 1}, _,
                                 MoqtObjectStatus::kNormal, "object 5", true))
        .WillOnce([&] { ++received; });
    queue->AddObject(MemSliceFromString("object 5"), /*key=*/false);

    success = test_harness_.RunUntilWithDefaultTimeout(
        [&]() { return received >= 3; });
    EXPECT_TRUE(success);

    EXPECT_CALL(client_visitor,
                OnObjectFragment(_, FullSequence{1, 2}, _,
                                 MoqtObjectStatus::kNormal, "object 6", true))
        .WillOnce([&] { ++received; });
    queue->AddObject(MemSliceFromString("object 6"), /*key=*/false);
    EXPECT_CALL(client_visitor,
                OnObjectFragment(_, FullSequence{1, 3}, _,
                                 MoqtObjectStatus::kEndOfGroup, "", true))
        .WillOnce([&] { ++received; });
    EXPECT_CALL(client_visitor,
                OnObjectFragment(_, FullSequence{2, 0}, _,
                                 MoqtObjectStatus::kNormal, "object 7", true))
        .WillOnce([&] { ++received; });
    queue->AddObject(MemSliceFromString("object 7"), /*key=*/true);
    EXPECT_CALL(client_visitor,
                OnObjectFragment(_, FullSequence{2, 1}, _,
                                 MoqtObjectStatus::kNormal, "object 8", true))
        .WillOnce([&] { ++received; });
    queue->AddObject(MemSliceFromString("object 8"), /*key=*/false);

    success = test_harness_.RunUntilWithDefaultTimeout(
        [&]() { return received >= 7; });
    EXPECT_TRUE(success);

    EXPECT_CALL(client_visitor,
                OnObjectFragment(_, FullSequence{2, 2}, _,
                                 MoqtObjectStatus::kEndOfGroup, "", true))
        .WillOnce([&] { ++received; });
    EXPECT_CALL(client_visitor,
                OnObjectFragment(_, FullSequence{3, 0}, _,
                                 MoqtObjectStatus::kEndOfTrack, "", true))
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

  MockSubscribeRemoteTrackVisitor client_visitor;
  FullTrackName full_track_name("test", "fetch");
  auto queue = std::make_shared<MoqtOutgoingQueue>(
      full_track_name, MoqtForwardingPreference::kSubgroup);
  publisher.Add(queue);
  for (int i = 0; i < 100; ++i) {
    queue->AddObject(MemSliceFromString("object"), /*key=*/true);
  }
  std::unique_ptr<MoqtFetchTask> fetch;
  EXPECT_TRUE(client_->session()->Fetch(
      full_track_name,
      [&](std::unique_ptr<MoqtFetchTask> task) { fetch = std::move(task); },
      FullSequence{0, 0}, 99, std::nullopt, 128, std::nullopt,
      MoqtSubscribeParameters()));
  // Run until we get FETCH_OK.
  bool success = test_harness_.RunUntilWithDefaultTimeout(
      [&]() { return fetch != nullptr; });
  EXPECT_TRUE(success);

  EXPECT_TRUE(fetch->GetStatus().ok());
  EXPECT_EQ(fetch->GetLargestId(), FullSequence(99, 0));
  MoqtFetchTask::GetNextObjectResult result;
  PublishedObject object;
  FullSequence expected{97, 0};
  do {
    result = fetch->GetNextObject(object);
    if (result == MoqtFetchTask::GetNextObjectResult::kEof) {
      break;
    }
    EXPECT_EQ(result, MoqtFetchTask::GetNextObjectResult::kSuccess);
    EXPECT_EQ(object.sequence, expected);
    if (object.sequence.object == 1) {
      EXPECT_EQ(object.status, MoqtObjectStatus::kEndOfGroup);
      expected.object = 0;
      ++expected.group;
    } else {
      EXPECT_EQ(object.status, MoqtObjectStatus::kNormal);
      EXPECT_EQ(object.payload.AsStringView(), "object");
      ++expected.object;
    }
  } while (result == MoqtFetchTask::GetNextObjectResult::kSuccess);
  EXPECT_EQ(result, MoqtFetchTask::GetNextObjectResult::kEof);
  EXPECT_EQ(expected, FullSequence(99, 1));
}

TEST_F(MoqtIntegrationTest, AnnounceFailure) {
  EstablishSession();
  testing::MockFunction<void(
      FullTrackName track_namespace,
      std::optional<MoqtAnnounceErrorReason> error_message)>
      announce_callback;
  client_->session()->Announce(FullTrackName{"foo"},
                               announce_callback.AsStdFunction());
  bool matches = false;
  EXPECT_CALL(announce_callback, Call(_, _))
      .WillOnce([&](FullTrackName track_namespace,
                    std::optional<MoqtAnnounceErrorReason> error) {
        matches = true;
        EXPECT_EQ(track_namespace, FullTrackName{"foo"});
        ASSERT_TRUE(error.has_value());
        EXPECT_EQ(error->error_code, SubscribeErrorCode::kNotSupported);
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

  MockSubscribeRemoteTrackVisitor client_visitor;
  std::optional<absl::string_view> expected_reason = std::nullopt;
  bool received_ok = false;
  EXPECT_CALL(*track_publisher, AddObjectListener)
      .WillOnce([&](MoqtObjectListener* listener) {
        listener->OnSubscribeAccepted();
      });
  EXPECT_CALL(client_visitor, OnReply(full_track_name, _, expected_reason))
      .WillOnce([&]() { received_ok = true; });
  client_->session()->SubscribeAbsolute(full_track_name, 0, 0, &client_visitor,
                                        MoqtSubscribeParameters());
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

  MockSubscribeRemoteTrackVisitor client_visitor;
  std::optional<absl::string_view> expected_reason = std::nullopt;
  bool received_ok = false;
  EXPECT_CALL(*track_publisher, AddObjectListener)
      .WillOnce([&](MoqtObjectListener* listener) {
        listener->OnSubscribeAccepted();
      });
  EXPECT_CALL(client_visitor, OnReply(full_track_name, _, expected_reason))
      .WillOnce([&]() { received_ok = true; });
  client_->session()->SubscribeCurrentObject(full_track_name, &client_visitor,
                                             MoqtSubscribeParameters());
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return received_ok; });
  EXPECT_TRUE(success);
}

TEST_F(MoqtIntegrationTest, SubscribeCurrentGroupOk) {
  EstablishSession();
  FullTrackName full_track_name("foo", "bar");

  MoqtKnownTrackPublisher publisher;
  server_->session()->set_publisher(&publisher);
  // TODO(martinduke): Unmock this.
  auto track_publisher = std::make_shared<MockTrackPublisher>(full_track_name);
  publisher.Add(track_publisher);

  MockSubscribeRemoteTrackVisitor client_visitor;
  std::optional<absl::string_view> expected_reason = std::nullopt;
  bool received_ok = false;
  EXPECT_CALL(*track_publisher, AddObjectListener)
      .WillOnce([&](MoqtObjectListener* listener) {
        listener->OnSubscribeAccepted();
      });
  EXPECT_CALL(client_visitor, OnReply(full_track_name, _, expected_reason))
      .WillOnce([&]() { received_ok = true; });
  client_->session()->SubscribeCurrentObject(full_track_name, &client_visitor,
                                             MoqtSubscribeParameters());
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return received_ok; });
  EXPECT_TRUE(success);
}

TEST_F(MoqtIntegrationTest, SubscribeError) {
  EstablishSession();
  FullTrackName full_track_name("foo", "bar");
  MockSubscribeRemoteTrackVisitor client_visitor;
  std::optional<absl::string_view> expected_reason = "No tracks published";
  bool received_ok = false;
  EXPECT_CALL(client_visitor, OnReply(full_track_name, _, expected_reason))
      .WillOnce([&]() { received_ok = true; });
  client_->session()->SubscribeCurrentObject(full_track_name, &client_visitor,
                                             MoqtSubscribeParameters());
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return received_ok; });
  EXPECT_TRUE(success);
}

TEST_F(MoqtIntegrationTest, CleanSubscribeDone) {
  EstablishSession();
  FullTrackName full_track_name("foo", "bar");

  MoqtKnownTrackPublisher publisher;
  server_->session()->set_publisher(&publisher);
  auto queue = std::make_shared<MoqtLiveRelayQueue>(
      full_track_name, MoqtForwardingPreference::kSubgroup);
  publisher.Add(queue);

  MockSubscribeRemoteTrackVisitor client_visitor;
  SubscribeLatestObject(full_track_name, &client_visitor);

  // Deliver 3 objects on 2 streams.
  queue->AddObject(FullSequence(0, 0), "object,0,0", false);
  queue->AddObject(FullSequence(0, 1), "object,0,1", true);
  queue->AddObject(FullSequence(1, 0), "object,1,0", true);
  int received = 0;
  EXPECT_CALL(client_visitor, OnObjectFragment).WillRepeatedly([&]() {
    ++received;
  });
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return received == 3; });
  EXPECT_TRUE(success);

  // Reject this subscribe because there already is one.
  EXPECT_FALSE(client_->session()->SubscribeCurrentObject(
      full_track_name, &client_visitor, MoqtSubscribeParameters()));
  queue->RemoveAllSubscriptions();  // Induce a SUBSCRIBE_DONE.
  bool subscribe_done = false;
  EXPECT_CALL(client_visitor, OnSubscribeDone).WillOnce([&]() {
    subscribe_done = true;
  });
  success = test_harness_.RunUntilWithDefaultTimeout(
      [&]() { return subscribe_done; });
  EXPECT_TRUE(success);
  // Subscription is deleted; the client session should not immediately reject
  // a new attempt.
  EXPECT_TRUE(client_->session()->SubscribeCurrentObject(
      full_track_name, &client_visitor, MoqtSubscribeParameters()));
}

TEST_F(MoqtIntegrationTest, ObjectAcks) {
  CreateDefaultEndpoints();
  WireUpEndpoints();
  client_->session()->set_support_object_acks(true);
  server_->session()->set_support_object_acks(true);
  ConnectEndpoints();

  FullTrackName full_track_name("foo", "bar");
  MockSubscribeRemoteTrackVisitor client_visitor;

  MoqtKnownTrackPublisher publisher;
  server_->session()->set_publisher(&publisher);
  auto track_publisher = std::make_shared<MockTrackPublisher>(full_track_name);
  publisher.Add(track_publisher);

  MockPublishingMonitorInterface monitoring;
  server_->session()->SetMonitoringInterfaceForTrack(full_track_name,
                                                     &monitoring);

  MoqtObjectAckFunction ack_function = nullptr;
  EXPECT_CALL(client_visitor, OnCanAckObjects(_))
      .WillOnce([&](MoqtObjectAckFunction new_ack_function) {
        ack_function = std::move(new_ack_function);
      });
  EXPECT_CALL(*track_publisher, AddObjectListener)
      .WillOnce([&](MoqtObjectListener* listener) {
        listener->OnSubscribeAccepted();
      });
  EXPECT_CALL(client_visitor, OnReply(_, _, _))
      .WillOnce([&](const FullTrackName&, std::optional<FullSequence>,
                    std::optional<absl::string_view>) {
        ack_function(10, 20, quic::QuicTimeDelta::FromMicroseconds(-123));
        ack_function(100, 200, quic::QuicTimeDelta::FromMicroseconds(456));
      });

  MoqtSubscribeParameters parameters;
  parameters.object_ack_window = quic::QuicTimeDelta::FromMilliseconds(100);
  client_->session()->SubscribeCurrentObject(full_track_name, &client_visitor,
                                             parameters);
  EXPECT_CALL(monitoring, OnObjectAckSupportKnown(true));
  EXPECT_CALL(
      monitoring,
      OnObjectAckReceived(10, 20, quic::QuicTimeDelta::FromMicroseconds(-123)));
  bool done = false;
  EXPECT_CALL(
      monitoring,
      OnObjectAckReceived(100, 200, quic::QuicTimeDelta::FromMicroseconds(456)))
      .WillOnce([&] { done = true; });
  bool success = test_harness_.RunUntilWithDefaultTimeout([&] { return done; });
  EXPECT_TRUE(success);
}

TEST_F(MoqtIntegrationTest, DeliveryTimeout) {
  CreateDefaultEndpoints();
  WireUpEndpointsWithLoss(/*lose_every_n=*/4);
  ConnectEndpoints();
  FullTrackName full_track_name("foo", "bar");

  MoqtKnownTrackPublisher publisher;
  server_->session()->set_publisher(&publisher);
  auto queue = std::make_shared<MoqtLiveRelayQueue>(
      full_track_name, MoqtForwardingPreference::kSubgroup,
      test_harness_.simulator().GetClock());
  auto track_publisher = std::make_shared<MockTrackPublisher>(full_track_name);
  publisher.Add(queue);

  MockSubscribeRemoteTrackVisitor client_visitor;
  std::optional<absl::string_view> expected_reason = std::nullopt;
  bool received_ok = false;
  EXPECT_CALL(client_visitor, OnReply(full_track_name, _, expected_reason))
      .WillOnce([&]() { received_ok = true; });
  MoqtSubscribeParameters parameters;
  // Set delivery timeout to ~ 1 RTT: any loss is fatal.
  parameters.delivery_timeout = quic::QuicTimeDelta::FromMilliseconds(100);
  client_->session()->SubscribeCurrentObject(full_track_name, &client_visitor,
                                             parameters);
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return received_ok; });
  EXPECT_TRUE(success);

  // Publish 4 large objects with a FIN. One of them will be lost.
  std::string data(1000, '\0');
  size_t bytes_received = 0;
  EXPECT_CALL(client_visitor, OnObjectFragment)
      .WillRepeatedly(
          [&](const FullTrackName&, FullSequence sequence,
              MoqtPriority /*publisher_priority*/, MoqtObjectStatus status,
              absl::string_view object,
              bool end_of_message) { bytes_received += object.size(); });
  queue->AddObject(FullSequence{0, 0, 0}, data, false);
  queue->AddObject(FullSequence{0, 0, 1}, data, false);
  queue->AddObject(FullSequence{0, 0, 2}, data, false);
  queue->AddObject(FullSequence{0, 0, 3}, data, true);
  success = test_harness_.RunUntilWithDefaultTimeout([&]() {
    return MoqtSessionPeer::SubgroupHasBeenReset(
        MoqtSessionPeer::GetSubscription(server_->session(), 0),
        FullSequence{0, 0, 0});
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
  auto queue = std::make_shared<MoqtLiveRelayQueue>(
      full_track_name, MoqtForwardingPreference::kSubgroup,
      test_harness_.simulator().GetClock());
  auto track_publisher = std::make_shared<MockTrackPublisher>(full_track_name);
  publisher.Add(queue);

  MockSubscribeRemoteTrackVisitor client_visitor;
  std::optional<absl::string_view> expected_reason = std::nullopt;
  bool received_ok = false;
  EXPECT_CALL(client_visitor, OnReply(full_track_name, _, expected_reason))
      .WillOnce([&]() { received_ok = true; });
  MoqtSubscribeParameters parameters;
  // Set delivery timeout to ~ 1 RTT: any loss is fatal.
  parameters.delivery_timeout = quic::QuicTimeDelta::FromMilliseconds(100);
  client_->session()->SubscribeCurrentObject(full_track_name, &client_visitor,
                                             parameters);
  bool success =
      test_harness_.RunUntilWithDefaultTimeout([&]() { return received_ok; });
  EXPECT_TRUE(success);
  success = false;

  std::string data(1000, '\0');
  size_t bytes_received = 0;
  EXPECT_CALL(client_visitor, OnObjectFragment)
      .WillRepeatedly(
          [&](const FullTrackName&, FullSequence sequence,
              MoqtPriority /*publisher_priority*/, MoqtObjectStatus status,
              absl::string_view object,
              bool end_of_message) { bytes_received += object.size(); });
  queue->AddObject(FullSequence{0, 0, 0}, data, false);
  queue->AddObject(FullSequence{1, 0, 0}, data, false);
  success = test_harness_.RunUntilWithDefaultTimeout([&]() {
    return MoqtSessionPeer::SubgroupHasBeenReset(
        MoqtSessionPeer::GetSubscription(server_->session(), 0),
        FullSequence{0, 0, 0});
  });
  EXPECT_TRUE(success);
  EXPECT_EQ(bytes_received, 2000);
}

}  // namespace

}  // namespace moqt::test
