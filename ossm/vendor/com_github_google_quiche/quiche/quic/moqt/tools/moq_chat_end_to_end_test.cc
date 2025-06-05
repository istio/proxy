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
#include "quiche/quic/moqt/tools/chat_client.h"
#include "quiche/quic/moqt/tools/chat_server.h"
#include "quiche/quic/moqt/tools/moq_chat.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/quic/test_tools/crypto_test_utils.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_callbacks.h"
#include "quiche/common/quiche_ip_address.h"

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

class MoqChatEndToEndTest : public quiche::test::QuicheTest {
 public:
  MoqChatEndToEndTest()
      : server_(quic::test::crypto_test_utils::ProofSourceForTesting(), "") {
    quiche::QuicheIpAddress bind_address;
    std::string hostname(kChatHostname);
    bind_address.FromString(hostname);
    EXPECT_TRUE(server_.moqt_server().quic_server().CreateUDPSocketAndListen(
        quic::QuicSocketAddress(bind_address, 0)));
    auto if1ptr = std::make_unique<MockChatUserInterface>();
    auto if2ptr = std::make_unique<MockChatUserInterface>();
    interface1_ = if1ptr.get();
    interface2_ = if2ptr.get();
    uint16_t port = server_.moqt_server().quic_server().port();
    client1_ = std::make_unique<moqt::moq_chat::ChatClient>(
        quic::QuicServerId(hostname, port), true, std::move(if1ptr),
        "test_chat", "client1", "device1",
        server_.moqt_server().quic_server().event_loop());
    client2_ = std::make_unique<moqt::moq_chat::ChatClient>(
        quic::QuicServerId(hostname, port), true, std::move(if2ptr),
        "test_chat", "client2", "device2",
        server_.moqt_server().quic_server().event_loop());
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
      server_.moqt_server().quic_server().WaitForEvents();
    }
  }

  moqt::moq_chat::ChatServer server_;
  MockChatUserInterface *interface1_, *interface2_;
  std::unique_ptr<moqt::moq_chat::ChatClient> client1_, client2_;
};

TEST_F(MoqChatEndToEndTest, EndToEndTest) {
  EXPECT_TRUE(client1_->Connect(moqt::moq_chat::kWebtransPath));
  EXPECT_TRUE(client2_->Connect(moqt::moq_chat::kWebtransPath));
  EXPECT_TRUE(client1_->AnnounceAndSubscribeAnnounces());
  EXPECT_TRUE(client2_->AnnounceAndSubscribeAnnounces());
  SendAndWaitForOutput(interface1_, interface2_, "client1", "Hello");
  SendAndWaitForOutput(interface2_, interface1_, "client2", "Hi");
  SendAndWaitForOutput(interface1_, interface2_, "client1", "How are you?");
  SendAndWaitForOutput(interface2_, interface1_, "client2", "Good, and you?");
  SendAndWaitForOutput(interface1_, interface2_, "client1", "I'm fine");
  SendAndWaitForOutput(interface2_, interface1_, "client2", "Goodbye");

  interface1_->SendMessage("/exit");
  EXPECT_CALL(*interface2_, WriteToOutput(_, _)).Times(0);
  server_.moqt_server().quic_server().WaitForEvents();
}

TEST_F(MoqChatEndToEndTest, LeaveAndRejoin) {
  EXPECT_TRUE(client1_->Connect(moqt::moq_chat::kWebtransPath));
  EXPECT_TRUE(client2_->Connect(moqt::moq_chat::kWebtransPath));
  EXPECT_TRUE(client1_->AnnounceAndSubscribeAnnounces());
  EXPECT_TRUE(client2_->AnnounceAndSubscribeAnnounces());
  SendAndWaitForOutput(interface1_, interface2_, "client1", "Hello");
  SendAndWaitForOutput(interface2_, interface1_, "client2", "Hi");

  interface1_->SendMessage("/exit");
  while (client1_->session_is_open()) {
    server_.moqt_server().quic_server().WaitForEvents();
  }
  client1_.reset();
  while (server_.num_users() > 1) {
    server_.moqt_server().quic_server().WaitForEvents();
  }

  // Create a new client with the same username and Reconnect.
  auto if1bptr = std::make_unique<MockChatUserInterface>();
  MockChatUserInterface* interface1b_ = if1bptr.get();
  uint16_t port = server_.moqt_server().quic_server().port();
  client1_ = std::make_unique<moqt::moq_chat::ChatClient>(
      quic::QuicServerId(std::string(kChatHostname), port), true,
      std::move(if1bptr), "test_chat", "client1", "device1",
      server_.moqt_server().quic_server().event_loop());
  EXPECT_TRUE(client1_->Connect(moqt::moq_chat::kWebtransPath));
  EXPECT_TRUE(client1_->AnnounceAndSubscribeAnnounces());
  SendAndWaitForOutput(interface1b_, interface2_, "client1", "Hello again");
  SendAndWaitForOutput(interface2_, interface1b_, "client2", "Hi again");
}

}  // namespace test

}  // namespace moqt
