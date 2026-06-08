// Copyright (c) 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "quiche/quic/core/io/quic_event_loop.h"
#include "quiche/quic/core/quic_server_id.h"
#include "quiche/quic/moqt/moqt_fetch_task.h"
#include "quiche/quic/moqt/moqt_names.h"
#include "quiche/quic/moqt/tools/chat_client.h"
#include "quiche/quic/moqt/tools/moq_chat.h"
#include "quiche/quic/moqt/tools/moqt_relay.h"
#include "quiche/quic/test_tools/crypto_test_utils.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_callbacks.h"

namespace moqt {

namespace test {

using ::testing::_;

constexpr absl::string_view kChatHostname = "127.0.0.1";

class MockChatUserInterface : public moqt::moq_chat::ChatUserInterface {
 public:
  void Initialize(quiche::MultiUseCallback<void(absl::string_view)> callback,
                  quic::QuicEventLoop* event_loop) override {
    callback_ = std::move(callback);
    event_loop_ = event_loop;
  }

  void IoLoop() override {
    event_loop_->RunEventLoopOnce(moqt::moq_chat::kChatEventLoopDuration);
  }

  MOCK_METHOD(void, WriteToOutput,
              (absl::string_view user, absl::string_view message), (override));

  void SendMessage(absl::string_view message) { callback_(message); }

 private:
  quiche::MultiUseCallback<void(absl::string_view)> callback_;
  quic::QuicEventLoop* event_loop_;
  std::string message_;
};

class TestMoqtRelay : public moqt::MoqtRelay {
 public:
  TestMoqtRelay(absl::string_view bind_address, uint16_t bind_port,
                absl::string_view default_upstream, bool ignore_certificate)
      : MoqtRelay(quic::test::crypto_test_utils::ProofSourceForTesting(),
                  std::string(bind_address), bind_port, default_upstream,
                  ignore_certificate) {}
  using MoqtRelay::publisher;
  using MoqtRelay::server;
};

class MoqChatEndToEndTest : public quiche::test::QuicheTest {
 public:
  MoqChatEndToEndTest() : relay_(kChatHostname, 0, "", true) {
    auto if1ptr = std::make_unique<MockChatUserInterface>();
    auto if2ptr = std::make_unique<MockChatUserInterface>();
    interface1_ = if1ptr.get();
    interface2_ = if2ptr.get();
    uint16_t port = relay_.server()->port();
    client1_ = std::make_unique<moqt::moq_chat::ChatClient>(
        quic::QuicServerId(std::string(kChatHostname), port), true,
        std::move(if1ptr), "test_chat", "client1", "device1",
        relay_.server()->event_loop());
    client2_ = std::make_unique<moqt::moq_chat::ChatClient>(
        quic::QuicServerId(std::string(kChatHostname), port), true,
        std::move(if2ptr), "test_chat", "client2", "device2",
        relay_.server()->event_loop());
  }

  void SendAndWaitForOutput(MockChatUserInterface* sender,
                            MockChatUserInterface* receiver,
                            absl::string_view sender_name,
                            absl::string_view message) {
    bool message_to_output = false;
    EXPECT_CALL(*receiver, WriteToOutput(sender_name, message)).WillOnce([&] {
      message_to_output = true;
    });
    sender->SendMessage(message);
    while (!message_to_output) {
      relay_.server()->WaitForEvents();
    }
  }

  TestMoqtRelay relay_;
  MockChatUserInterface *interface1_, *interface2_;
  std::unique_ptr<moqt::moq_chat::ChatClient> client1_, client2_;
};

TEST_F(MoqChatEndToEndTest, EndToEndTest) {
  EXPECT_TRUE(client1_->Connect(moqt::moq_chat::kWebtransPath));
  EXPECT_TRUE(client2_->Connect(moqt::moq_chat::kWebtransPath));
  EXPECT_TRUE(client1_->PublishNamespaceAndSubscribeNamespace());
  EXPECT_TRUE(client2_->PublishNamespaceAndSubscribeNamespace());
  while (client1_->is_syncing() || client2_->is_syncing()) {
    relay_.server()->WaitForEvents();
  }
  SendAndWaitForOutput(interface1_, interface2_, "client1", "Hello");
  SendAndWaitForOutput(interface2_, interface1_, "client2", "Hi");
  SendAndWaitForOutput(interface1_, interface2_, "client1", "How are you?");
  SendAndWaitForOutput(interface2_, interface1_, "client2", "Good, and you?");
  SendAndWaitForOutput(interface1_, interface2_, "client1", "I'm fine");
  SendAndWaitForOutput(interface2_, interface1_, "client2", "Goodbye");

  interface1_->SendMessage("/exit");
  EXPECT_CALL(*interface2_, WriteToOutput(_, _)).Times(0);
  relay_.server()->WaitForEvents();
}

TEST_F(MoqChatEndToEndTest, EndToEndTestUngracefulClose) {
  EXPECT_TRUE(client1_->Connect(moqt::moq_chat::kWebtransPath));
  EXPECT_TRUE(client2_->Connect(moqt::moq_chat::kWebtransPath));
  EXPECT_TRUE(client1_->PublishNamespaceAndSubscribeNamespace());
  EXPECT_TRUE(client2_->PublishNamespaceAndSubscribeNamespace());
  while (client1_->is_syncing() || client2_->is_syncing()) {
    relay_.server()->WaitForEvents();
  }
  SendAndWaitForOutput(interface1_, interface2_, "client1", "Hello");
  SendAndWaitForOutput(interface2_, interface1_, "client2", "Hi");
  SendAndWaitForOutput(interface1_, interface2_, "client1", "How are you?");
  SendAndWaitForOutput(interface2_, interface1_, "client2", "Good, and you?");
  SendAndWaitForOutput(interface1_, interface2_, "client1", "I'm fine");
  SendAndWaitForOutput(interface2_, interface1_, "client2", "Goodbye");
}

TEST_F(MoqChatEndToEndTest, LeaveAndRejoin) {
  EXPECT_TRUE(client1_->Connect(moqt::moq_chat::kWebtransPath));
  EXPECT_TRUE(client2_->Connect(moqt::moq_chat::kWebtransPath));
  EXPECT_TRUE(client1_->PublishNamespaceAndSubscribeNamespace());
  EXPECT_TRUE(client2_->PublishNamespaceAndSubscribeNamespace());
  while (client1_->is_syncing() || client2_->is_syncing()) {
    relay_.server()->WaitForEvents();
  }
  SendAndWaitForOutput(interface1_, interface2_, "client1", "Hello");
  SendAndWaitForOutput(interface2_, interface1_, "client2", "Hi");

  // Add a probe to see how many publishers are tracked on the relay.
  int namespaces_announced = 0;
  TrackNamespace last_suffix;
  TransactionType last_type;
  std::unique_ptr<MoqtNamespaceTask> namespace_probe =
      relay_.publisher()->AddNamespaceSubscriber(
          TrackNamespace({moq_chat::kBasePath}), nullptr);
  namespace_probe->SetObjectsAvailableCallback([&]() {
    while (namespace_probe->GetNextSuffix(last_suffix, last_type) == kSuccess) {
      if (last_type == TransactionType::kAdd) {
        ++namespaces_announced;
      } else {
        --namespaces_announced;
      }
    }
  });
  EXPECT_EQ(namespaces_announced, 2);

  interface1_->SendMessage("/exit");
  while (client1_->session_is_open()) {
    relay_.server()->WaitForEvents();
  }
  client1_.reset();
  while (last_type != TransactionType::kDelete) {
    // Wait for the relay's session cleanup to send PUBLISH_NAMESPACE_DONE
    // and PUBLISH_DONE.
    relay_.server()->WaitForEvents();
  }
  EXPECT_EQ(namespaces_announced, 1);
  // Create a new client with the same username and Reconnect.
  auto if1bptr = std::make_unique<MockChatUserInterface>();
  MockChatUserInterface* interface1b_ = if1bptr.get();
  uint16_t port = relay_.server()->port();
  client1_ = std::make_unique<moqt::moq_chat::ChatClient>(
      quic::QuicServerId(std::string(kChatHostname), port), true,
      std::move(if1bptr), "test_chat", "client1", "device1",
      relay_.server()->event_loop());
  EXPECT_TRUE(client1_->Connect(moqt::moq_chat::kWebtransPath));
  EXPECT_TRUE(client1_->PublishNamespaceAndSubscribeNamespace());
  while (client1_->is_syncing() || client2_->is_syncing()) {
    relay_.server()->WaitForEvents();
  }
  SendAndWaitForOutput(interface1b_, interface2_, "client1", "Hello again");
  SendAndWaitForOutput(interface2_, interface1b_, "client2", "Hi again");
  EXPECT_EQ(namespaces_announced, 2);
  // Cleanup the probe.
  namespace_probe.reset();
}

}  // namespace test

}  // namespace moqt
