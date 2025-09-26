// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/io/event_loop_connecting_client_socket.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

#include "absl/functional/bind_front.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/types/span.h"
#include "quiche/quic/core/connecting_client_socket.h"
#include "quiche/quic/core/io/event_loop_socket_factory.h"
#include "quiche/quic/core/io/quic_default_event_loop.h"
#include "quiche/quic/core/io/quic_event_loop.h"
#include "quiche/quic/core/io/socket.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/quic/test_tools/mock_clock.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/platform/api/quiche_logging.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/platform/api/quiche_test_loopback.h"
#include "quiche/common/platform/api/quiche_thread.h"
#include "quiche/common/quiche_callbacks.h"
#include "quiche/common/quiche_mem_slice.h"
#include "quiche/common/simple_buffer_allocator.h"

namespace quic::test {
namespace {

using ::testing::Combine;
using ::testing::Values;
using ::testing::ValuesIn;

class TestServerSocketRunner : public quiche::QuicheThread {
 public:
  using SocketBehavior = quiche::MultiUseCallback<void(
      SocketFd connected_socket, socket_api::SocketProtocol protocol)>;

  TestServerSocketRunner(SocketFd server_socket_descriptor,
                         SocketBehavior behavior)
      : QuicheThread("TestServerSocketRunner"),
        server_socket_descriptor_(server_socket_descriptor),
        behavior_(std::move(behavior)) {}
  ~TestServerSocketRunner() override { WaitForCompletion(); }

  void WaitForCompletion() { completion_notification_.WaitForNotification(); }

 protected:
  SocketFd server_socket_descriptor() const {
    return server_socket_descriptor_;
  }

  const SocketBehavior& behavior() const { return behavior_; }

  absl::Notification& completion_notification() {
    return completion_notification_;
  }

 private:
  const SocketFd server_socket_descriptor_;
  const SocketBehavior behavior_;

  absl::Notification completion_notification_;
};

class TestTcpServerSocketRunner : public TestServerSocketRunner {
 public:
  // On construction, spins a separate thread to accept a connection from
  // `server_socket_descriptor`, runs `behavior` with that connection, and then
  // closes the accepted connection socket.
  TestTcpServerSocketRunner(SocketFd server_socket_descriptor,
                            SocketBehavior behavior)
      : TestServerSocketRunner(server_socket_descriptor, std::move(behavior)) {
    Start();
  }

  ~TestTcpServerSocketRunner() override { Join(); }

 protected:
  void Run() override {
    AcceptSocket();
    behavior()(connection_socket_descriptor_, socket_api::SocketProtocol::kTcp);
    CloseSocket();

    completion_notification().Notify();
  }

 private:
  void AcceptSocket() {
    absl::StatusOr<socket_api::AcceptResult> connection_socket =
        socket_api::Accept(server_socket_descriptor(), /*blocking=*/true);
    QUICHE_CHECK(connection_socket.ok());
    connection_socket_descriptor_ = connection_socket.value().fd;
  }

  void CloseSocket() {
    QUICHE_CHECK(socket_api::Close(connection_socket_descriptor_).ok());
    QUICHE_CHECK(socket_api::Close(server_socket_descriptor()).ok());
  }

  SocketFd connection_socket_descriptor_ = kInvalidSocketFd;
};

class TestUdpServerSocketRunner : public TestServerSocketRunner {
 public:
  // On construction, spins a separate thread to connect
  // `server_socket_descriptor` to `client_socket_address`, runs `behavior` with
  // that connection, and then disconnects the socket.
  TestUdpServerSocketRunner(SocketFd server_socket_descriptor,
                            SocketBehavior behavior,
                            QuicSocketAddress client_socket_address)
      : TestServerSocketRunner(server_socket_descriptor, std::move(behavior)),
        client_socket_address_(std::move(client_socket_address)) {
    Start();
  }

  ~TestUdpServerSocketRunner() override { Join(); }

 protected:
  void Run() override {
    ConnectSocket();
    behavior()(server_socket_descriptor(), socket_api::SocketProtocol::kUdp);
    DisconnectSocket();

    completion_notification().Notify();
  }

 private:
  void ConnectSocket() {
    QUICHE_CHECK(
        socket_api::Connect(server_socket_descriptor(), client_socket_address_)
            .ok());
  }

  void DisconnectSocket() {
    QUICHE_CHECK(socket_api::Close(server_socket_descriptor()).ok());
  }

  QuicSocketAddress client_socket_address_;
};

class EventLoopConnectingClientSocketTest
    : public quiche::test::QuicheTestWithParam<
          std::tuple<socket_api::SocketProtocol, QuicEventLoopFactory*>>,
      public ConnectingClientSocket::AsyncVisitor {
 public:
  void SetUp() override {
    QuicEventLoopFactory* event_loop_factory;
    std::tie(protocol_, event_loop_factory) = GetParam();

    event_loop_ = event_loop_factory->Create(&clock_);
    socket_factory_ = std::make_unique<EventLoopSocketFactory>(
        event_loop_.get(), quiche::SimpleBufferAllocator::Get());

    QUICHE_CHECK(CreateListeningServerSocket());
  }

  void TearDown() override {
    if (server_socket_descriptor_ != kInvalidSocketFd) {
      QUICHE_CHECK(socket_api::Close(server_socket_descriptor_).ok());
    }
  }

  void ConnectComplete(absl::Status status) override {
    QUICHE_CHECK(!connect_result_.has_value());
    connect_result_ = std::move(status);
  }

  void ReceiveComplete(absl::StatusOr<quiche::QuicheMemSlice> data) override {
    QUICHE_CHECK(!receive_result_.has_value());
    receive_result_ = std::move(data);
  }

  void SendComplete(absl::Status status) override {
    QUICHE_CHECK(!send_result_.has_value());
    send_result_ = std::move(status);
  }

 protected:
  std::unique_ptr<ConnectingClientSocket> CreateSocket(
      const quic::QuicSocketAddress& peer_address,
      ConnectingClientSocket::AsyncVisitor* async_visitor) {
    switch (protocol_) {
      case socket_api::SocketProtocol::kUdp:
        return socket_factory_->CreateConnectingUdpClientSocket(
            peer_address, /*receive_buffer_size=*/0, /*send_buffer_size=*/0,
            async_visitor);
      case socket_api::SocketProtocol::kTcp:
        return socket_factory_->CreateTcpClientSocket(
            peer_address, /*receive_buffer_size=*/0, /*send_buffer_size=*/0,
            async_visitor);
      default:
        // Unexpected protocol.
        QUICHE_NOTREACHED();
        return nullptr;
    }
  }

  std::unique_ptr<ConnectingClientSocket> CreateSocketToEncourageDelayedSend(
      const quic::QuicSocketAddress& peer_address,
      ConnectingClientSocket::AsyncVisitor* async_visitor) {
    switch (protocol_) {
      case socket_api::SocketProtocol::kUdp:
        // Nothing special for UDP since UDP does not gaurantee packets will be
        // sent once send buffers are full.
        return socket_factory_->CreateConnectingUdpClientSocket(
            peer_address, /*receive_buffer_size=*/0, /*send_buffer_size=*/0,
            async_visitor);
      case socket_api::SocketProtocol::kTcp:
        // For TCP, set a very small send buffer to encourage sends to be
        // delayed.
        return socket_factory_->CreateTcpClientSocket(
            peer_address, /*receive_buffer_size=*/0, /*send_buffer_size=*/4,
            async_visitor);
      default:
        // Unexpected protocol.
        QUICHE_NOTREACHED();
        return nullptr;
    }
  }

  bool CreateListeningServerSocket() {
    absl::StatusOr<SocketFd> socket = socket_api::CreateSocket(
        quiche::TestLoopback().address_family(), protocol_,
        /*blocking=*/true);
    QUICHE_CHECK(socket.ok());

    // For TCP, set an extremely small receive buffer size to increase the odds
    // of buffers filling up when testing asynchronous writes.
    if (protocol_ == socket_api::SocketProtocol::kTcp) {
      static const QuicByteCount kReceiveBufferSize = 2;
      absl::Status result =
          socket_api::SetReceiveBufferSize(socket.value(), kReceiveBufferSize);
      QUICHE_CHECK(result.ok());
    }

    QuicSocketAddress bind_address(quiche::TestLoopback(), /*port=*/0);
    absl::Status result = socket_api::Bind(socket.value(), bind_address);
    QUICHE_CHECK(result.ok());

    absl::StatusOr<QuicSocketAddress> socket_address =
        socket_api::GetSocketAddress(socket.value());
    QUICHE_CHECK(socket_address.ok());

    // TCP sockets need to listen for connections. UDP sockets are ready to
    // receive.
    if (protocol_ == socket_api::SocketProtocol::kTcp) {
      result = socket_api::Listen(socket.value(), /*backlog=*/1);
      QUICHE_CHECK(result.ok());
    }

    server_socket_descriptor_ = socket.value();
    server_socket_address_ = std::move(socket_address).value();
    return true;
  }

  std::unique_ptr<TestServerSocketRunner> CreateServerSocketRunner(
      TestServerSocketRunner::SocketBehavior behavior,
      ConnectingClientSocket* client_socket) {
    std::unique_ptr<TestServerSocketRunner> runner;
    switch (protocol_) {
      case socket_api::SocketProtocol::kUdp: {
        absl::StatusOr<QuicSocketAddress> client_socket_address =
            client_socket->GetLocalAddress();
        QUICHE_CHECK(client_socket_address.ok());
        runner = std::make_unique<TestUdpServerSocketRunner>(
            server_socket_descriptor_, std::move(behavior),
            std::move(client_socket_address).value());
        break;
      }
      case socket_api::SocketProtocol::kTcp:
        runner = std::make_unique<TestTcpServerSocketRunner>(
            server_socket_descriptor_, std::move(behavior));
        break;
      default:
        // Unexpected protocol.
        QUICHE_NOTREACHED();
    }

    // Runner takes responsibility for closing server socket.
    server_socket_descriptor_ = kInvalidSocketFd;

    return runner;
  }

  socket_api::SocketProtocol protocol_;

  SocketFd server_socket_descriptor_ = kInvalidSocketFd;
  QuicSocketAddress server_socket_address_;

  MockClock clock_;
  std::unique_ptr<QuicEventLoop> event_loop_;
  std::unique_ptr<EventLoopSocketFactory> socket_factory_;

  std::optional<absl::Status> connect_result_;
  std::optional<absl::StatusOr<quiche::QuicheMemSlice>> receive_result_;
  std::optional<absl::Status> send_result_;
};

std::string GetTestParamName(
    ::testing::TestParamInfo<
        std::tuple<socket_api::SocketProtocol, QuicEventLoopFactory*>>
        info) {
  auto [protocol, event_loop_factory] = info.param;

  return EscapeTestParamName(absl::StrCat(socket_api::GetProtocolName(protocol),
                                          "_", event_loop_factory->GetName()));
}

INSTANTIATE_TEST_SUITE_P(EventLoopConnectingClientSocketTests,
                         EventLoopConnectingClientSocketTest,
                         Combine(Values(socket_api::SocketProtocol::kUdp,
                                        socket_api::SocketProtocol::kTcp),
                                 ValuesIn(GetAllSupportedEventLoops())),
                         &GetTestParamName);

TEST_P(EventLoopConnectingClientSocketTest, ConnectBlocking) {
  std::unique_ptr<ConnectingClientSocket> socket =
      CreateSocket(server_socket_address_,
                   /*async_visitor=*/nullptr);

  // No socket runner to accept the connection for the server, but that is not
  // expected to be necessary for the connection to complete from the client for
  // TCP or UDP.
  EXPECT_TRUE(socket->ConnectBlocking().ok());

  socket->Disconnect();
}

TEST_P(EventLoopConnectingClientSocketTest, ConnectAsync) {
  std::unique_ptr<ConnectingClientSocket> socket =
      CreateSocket(server_socket_address_,
                   /*async_visitor=*/this);

  socket->ConnectAsync();

  // TCP connection typically completes asynchronously and UDP connection
  // typically completes before ConnectAsync returns, but there is no simple way
  // to ensure either behaves one way or the other. If connecting is
  // asynchronous, expect completion once signalled by the event loop.
  if (!connect_result_.has_value()) {
    event_loop_->RunEventLoopOnce(QuicTime::Delta::FromSeconds(1));
    ASSERT_TRUE(connect_result_.has_value());
  }
  EXPECT_TRUE(connect_result_.value().ok());

  connect_result_.reset();
  socket->Disconnect();
  EXPECT_FALSE(connect_result_.has_value());
}

TEST_P(EventLoopConnectingClientSocketTest, ErrorBeforeConnectAsync) {
  std::unique_ptr<ConnectingClientSocket> socket =
      CreateSocket(server_socket_address_,
                   /*async_visitor=*/this);

  // Close the server socket.
  EXPECT_TRUE(socket_api::Close(server_socket_descriptor_).ok());
  server_socket_descriptor_ = kInvalidSocketFd;

  socket->ConnectAsync();
  if (!connect_result_.has_value()) {
    event_loop_->RunEventLoopOnce(QuicTime::Delta::FromSeconds(1));
    ASSERT_TRUE(connect_result_.has_value());
  }

  switch (protocol_) {
    case socket_api::SocketProtocol::kTcp:
      // Expect an error because server socket was closed before connection.
      EXPECT_FALSE(connect_result_.value().ok());
      break;
    case socket_api::SocketProtocol::kUdp:
      // No error for UDP because UDP connection success does not rely on the
      // server.
      EXPECT_TRUE(connect_result_.value().ok());
      socket->Disconnect();
      break;
    default:
      // Unexpected protocol.
      FAIL();
  }
}

TEST_P(EventLoopConnectingClientSocketTest, ErrorDuringConnectAsync) {
  std::unique_ptr<ConnectingClientSocket> socket =
      CreateSocket(server_socket_address_,
                   /*async_visitor=*/this);

  socket->ConnectAsync();

  if (connect_result_.has_value()) {
    // UDP typically completes connection immediately before this test has a
    // chance to actually attempt the error. TCP typically completes
    // asynchronously, but no simple way to ensure that always happens.
    EXPECT_TRUE(connect_result_.value().ok());
    socket->Disconnect();
    return;
  }

  // Close the server socket.
  EXPECT_TRUE(socket_api::Close(server_socket_descriptor_).ok());
  server_socket_descriptor_ = kInvalidSocketFd;

  EXPECT_FALSE(connect_result_.has_value());
  event_loop_->RunEventLoopOnce(QuicTime::Delta::FromSeconds(1));
  ASSERT_TRUE(connect_result_.has_value());

  switch (protocol_) {
    case socket_api::SocketProtocol::kTcp:
      EXPECT_FALSE(connect_result_.value().ok());
      break;
    case socket_api::SocketProtocol::kUdp:
      // No error for UDP because UDP connection success does not rely on the
      // server.
      EXPECT_TRUE(connect_result_.value().ok());
      break;
    default:
      // Unexpected protocol.
      FAIL();
  }
}

TEST_P(EventLoopConnectingClientSocketTest, Disconnect) {
  std::unique_ptr<ConnectingClientSocket> socket =
      CreateSocket(server_socket_address_,
                   /*async_visitor=*/nullptr);

  ASSERT_TRUE(socket->ConnectBlocking().ok());
  socket->Disconnect();
}

TEST_P(EventLoopConnectingClientSocketTest, DisconnectCancelsConnectAsync) {
  std::unique_ptr<ConnectingClientSocket> socket =
      CreateSocket(server_socket_address_,
                   /*async_visitor=*/this);

  socket->ConnectAsync();

  bool expect_canceled = true;
  if (connect_result_.has_value()) {
    // UDP typically completes connection immediately before this test has a
    // chance to actually attempt the disconnect. TCP typically completes
    // asynchronously, but no simple way to ensure that always happens.
    EXPECT_TRUE(connect_result_.value().ok());
    expect_canceled = false;
  }

  socket->Disconnect();

  if (expect_canceled) {
    // Expect immediate cancelled error.
    ASSERT_TRUE(connect_result_.has_value());
    EXPECT_TRUE(absl::IsCancelled(connect_result_.value()));
  }
}

TEST_P(EventLoopConnectingClientSocketTest, ConnectAndReconnect) {
  std::unique_ptr<ConnectingClientSocket> socket =
      CreateSocket(server_socket_address_,
                   /*async_visitor=*/nullptr);

  ASSERT_TRUE(socket->ConnectBlocking().ok());
  socket->Disconnect();

  // Expect `socket` can reconnect now that it has been disconnected.
  EXPECT_TRUE(socket->ConnectBlocking().ok());
  socket->Disconnect();
}

TEST_P(EventLoopConnectingClientSocketTest, GetLocalAddress) {
  std::unique_ptr<ConnectingClientSocket> socket =
      CreateSocket(server_socket_address_,
                   /*async_visitor=*/nullptr);
  ASSERT_TRUE(socket->ConnectBlocking().ok());

  absl::StatusOr<QuicSocketAddress> address = socket->GetLocalAddress();
  ASSERT_TRUE(address.ok());
  EXPECT_TRUE(address.value().IsInitialized());

  socket->Disconnect();
}

void SendDataOnSocket(absl::string_view data, SocketFd connected_socket,
                      socket_api::SocketProtocol protocol) {
  QUICHE_CHECK(!data.empty());

  // May attempt to send in pieces for TCP. For UDP, expect failure if `data`
  // cannot be sent in a single packet.
  do {
    absl::StatusOr<absl::string_view> remainder =
        socket_api::Send(connected_socket, data);
    if (!remainder.ok()) {
      return;
    }
    data = remainder.value();
  } while (protocol == socket_api::SocketProtocol::kTcp && !data.empty());

  QUICHE_CHECK(data.empty());
}

TEST_P(EventLoopConnectingClientSocketTest, ReceiveBlocking) {
  std::unique_ptr<ConnectingClientSocket> socket =
      CreateSocket(server_socket_address_,
                   /*async_visitor=*/nullptr);
  ASSERT_TRUE(socket->ConnectBlocking().ok());

  std::string expected = {1, 2, 3, 4, 5, 6, 7, 8};
  std::unique_ptr<TestServerSocketRunner> runner = CreateServerSocketRunner(
      absl::bind_front(&SendDataOnSocket, expected), socket.get());

  std::string received;
  absl::StatusOr<quiche::QuicheMemSlice> data;

  // Expect exactly one packet for UDP, and at least two receives (data + FIN)
  // for TCP.
  do {
    data = socket->ReceiveBlocking(100);
    ASSERT_TRUE(data.ok());
    received.append(data.value().data(), data.value().length());
  } while (protocol_ == socket_api::SocketProtocol::kTcp &&
           !data.value().empty());

  EXPECT_EQ(received, expected);

  socket->Disconnect();
}

TEST_P(EventLoopConnectingClientSocketTest, ReceiveAsync) {
  std::unique_ptr<ConnectingClientSocket> socket =
      CreateSocket(server_socket_address_,
                   /*async_visitor=*/this);
  ASSERT_TRUE(socket->ConnectBlocking().ok());

  // Start an async receive.  Expect no immediate results because runner not
  // yet setup to send.
  socket->ReceiveAsync(100);
  EXPECT_FALSE(receive_result_.has_value());

  // Send data from server.
  std::string expected = {1, 2, 3, 4, 5, 6, 7, 8};
  std::unique_ptr<TestServerSocketRunner> runner = CreateServerSocketRunner(
      absl::bind_front(&SendDataOnSocket, expected), socket.get());

  EXPECT_FALSE(receive_result_.has_value());
  for (int i = 0; i < 5 && !receive_result_.has_value(); ++i) {
    event_loop_->RunEventLoopOnce(QuicTime::Delta::FromSeconds(1));
  }

  // Expect to receive at least some of the sent data.
  ASSERT_TRUE(receive_result_.has_value());
  ASSERT_TRUE(receive_result_.value().ok());
  EXPECT_FALSE(receive_result_.value().value().empty());
  std::string received(receive_result_.value().value().data(),
                       receive_result_.value().value().length());

  // For TCP, expect at least one more receive for the FIN.
  if (protocol_ == socket_api::SocketProtocol::kTcp) {
    absl::StatusOr<quiche::QuicheMemSlice> data;
    do {
      data = socket->ReceiveBlocking(100);
      ASSERT_TRUE(data.ok());
      received.append(data.value().data(), data.value().length());
    } while (!data.value().empty());
  }

  EXPECT_EQ(received, expected);

  receive_result_.reset();
  socket->Disconnect();
  EXPECT_FALSE(receive_result_.has_value());
}

TEST_P(EventLoopConnectingClientSocketTest, DisconnectCancelsReceiveAsync) {
  std::unique_ptr<ConnectingClientSocket> socket =
      CreateSocket(server_socket_address_,
                   /*async_visitor=*/this);

  ASSERT_TRUE(socket->ConnectBlocking().ok());

  // Start an asynchronous read, expecting no completion because server never
  // sends any data.
  socket->ReceiveAsync(100);
  EXPECT_FALSE(receive_result_.has_value());

  // Disconnect and expect an immediate cancelled error.
  socket->Disconnect();
  ASSERT_TRUE(receive_result_.has_value());
  ASSERT_FALSE(receive_result_.value().ok());
  EXPECT_TRUE(absl::IsCancelled(receive_result_.value().status()));
}

// Receive from `connected_socket` until connection is closed, writing
// received data to `out_received`.
void ReceiveDataFromSocket(std::string* out_received, SocketFd connected_socket,
                           socket_api::SocketProtocol protocol) {
  out_received->clear();

  std::string buffer(100, 0);
  absl::StatusOr<absl::Span<char>> received;

  // Expect exactly one packet for UDP, and at least two receives (data + FIN)
  // for TCP.
  do {
    received = socket_api::Receive(connected_socket, absl::MakeSpan(buffer));
    QUICHE_CHECK(received.ok());
    out_received->insert(out_received->end(), received.value().begin(),
                         received.value().end());
  } while (protocol == socket_api::SocketProtocol::kTcp &&
           !received.value().empty());
  QUICHE_CHECK(!out_received->empty());
}

TEST_P(EventLoopConnectingClientSocketTest, SendBlocking) {
  std::unique_ptr<ConnectingClientSocket> socket =
      CreateSocket(server_socket_address_,
                   /*async_visitor=*/nullptr);
  ASSERT_TRUE(socket->ConnectBlocking().ok());

  std::string sent;
  std::unique_ptr<TestServerSocketRunner> runner = CreateServerSocketRunner(
      absl::bind_front(&ReceiveDataFromSocket, &sent), socket.get());

  std::string expected = {1, 2, 3, 4, 5, 6, 7, 8};
  EXPECT_TRUE(socket->SendBlocking(expected).ok());
  socket->Disconnect();

  runner->WaitForCompletion();
  EXPECT_EQ(sent, expected);
}

TEST_P(EventLoopConnectingClientSocketTest, SendAsync) {
  std::unique_ptr<ConnectingClientSocket> socket =
      CreateSocketToEncourageDelayedSend(server_socket_address_,
                                         /*async_visitor=*/this);
  ASSERT_TRUE(socket->ConnectBlocking().ok());

  std::string data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  std::string expected;

  std::unique_ptr<TestServerSocketRunner> runner;
  std::string sent;
  switch (protocol_) {
    case socket_api::SocketProtocol::kTcp:
      // Repeatedly write to socket until it does not complete synchronously.
      do {
        expected.insert(expected.end(), data.begin(), data.end());
        send_result_.reset();
        socket->SendAsync(data);
        ASSERT_TRUE(!send_result_.has_value() || send_result_.value().ok());
      } while (send_result_.has_value());

      // Begin receiving from server and expect more data to send.
      runner = CreateServerSocketRunner(
          absl::bind_front(&ReceiveDataFromSocket, &sent), socket.get());
      EXPECT_FALSE(send_result_.has_value());
      for (int i = 0; i < 5 && !send_result_.has_value(); ++i) {
        event_loop_->RunEventLoopOnce(QuicTime::Delta::FromSeconds(1));
      }
      break;

    case socket_api::SocketProtocol::kUdp:
      // Expect UDP send to always send immediately.
      runner = CreateServerSocketRunner(
          absl::bind_front(&ReceiveDataFromSocket, &sent), socket.get());
      socket->SendAsync(data);
      expected = data;
      break;
    default:
      // Unexpected protocol.
      FAIL();
  }
  ASSERT_TRUE(send_result_.has_value());
  EXPECT_TRUE(send_result_.value().ok());

  send_result_.reset();
  socket->Disconnect();
  EXPECT_FALSE(send_result_.has_value());

  runner->WaitForCompletion();
  EXPECT_EQ(sent, expected);
}

TEST_P(EventLoopConnectingClientSocketTest, DisconnectCancelsSendAsync) {
  if (protocol_ == socket_api::SocketProtocol::kUdp) {
    // UDP sends are always immediate, so cannot disconect mid-send.
    return;
  }

  std::unique_ptr<ConnectingClientSocket> socket =
      CreateSocketToEncourageDelayedSend(server_socket_address_,
                                         /*async_visitor=*/this);
  ASSERT_TRUE(socket->ConnectBlocking().ok());

  std::string data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

  // Repeatedly write to socket until it does not complete synchronously.
  do {
    send_result_.reset();
    socket->SendAsync(data);
    ASSERT_TRUE(!send_result_.has_value() || send_result_.value().ok());
  } while (send_result_.has_value());

  // Disconnect and expect immediate cancelled error.
  socket->Disconnect();
  ASSERT_TRUE(send_result_.has_value());
  EXPECT_TRUE(absl::IsCancelled(send_result_.value()));
}

}  // namespace
}  // namespace quic::test
