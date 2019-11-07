/* Copyright 2019 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <future>

#include "common/network/utility.h"
#include "gtest/gtest.h"
#include "int_client.h"
#include "int_server.h"
#include "test/test_common/network_utility.h"

namespace Mixer {
namespace Integration {

class ClientServerTest : public testing::Test,
                         Envoy::Logger::Loggable<Envoy::Logger::Id::testing> {
 public:
  ClientServerTest()
      : transport_socket_factory_(),
        ip_version_(Envoy::Network::Address::IpVersion::v4),
        listening_socket_(
            Envoy::Network::Utility::parseInternetAddressAndPort(fmt::format(
                "{}:{}",
                Envoy::Network::Test::getAnyAddressUrlString(ip_version_), 0)),
            nullptr, true),
        client_("client"),
        server_("server", listening_socket_, transport_socket_factory_,
                Envoy::Http::CodecClient::Type::HTTP1) {}

 protected:
  Envoy::Network::RawBufferSocketFactory transport_socket_factory_;
  Envoy::Network::Address::IpVersion ip_version_;
  Envoy::Network::TcpListenSocket listening_socket_;
  Client client_;
  Server server_;
};

TEST_F(ClientServerTest, HappyPath) {
  constexpr uint32_t connections_to_initiate = 30;
  constexpr uint32_t requests_to_send = 30 * connections_to_initiate;

  //
  // Server Setup
  //

  ServerCallbackHelper server_callbacks;  // sends a 200 OK to everything
  server_.start(server_callbacks);

  //
  // Client setup
  //

  Envoy::Network::Address::InstanceConstSharedPtr address =
      listening_socket_.localAddress();
  LoadGenerator load_generator(client_, transport_socket_factory_,
                               HttpVersion::HTTP1, address);

  //
  // Exec test and wait for it to finish
  //

  Envoy::Http::HeaderMapPtr request{
      new Envoy::Http::TestHeaderMapImpl{{":method", "GET"},
                                         {":path", "/"},
                                         {":scheme", "http"},
                                         {":authority", "host"}}};
  load_generator.run(connections_to_initiate, requests_to_send,
                     std::move(request));

  // wait until the server has closed all connections created by the client
  server_callbacks.wait(load_generator.connectSuccesses());

  //
  // Evaluate test
  //

  // All client connections are successfully established.
  EXPECT_EQ(load_generator.connectSuccesses(), connections_to_initiate);
  EXPECT_EQ(0, load_generator.connectFailures());
  // Client close callback called for every client connection.
  EXPECT_EQ(load_generator.localCloses(), connections_to_initiate);
  // Client response callback is called for every request sent
  EXPECT_EQ(load_generator.responsesReceived(), requests_to_send);
  // Every response was a 2xx class
  EXPECT_EQ(load_generator.class2xxResponses(), requests_to_send);
  EXPECT_EQ(0, load_generator.class4xxResponses());
  EXPECT_EQ(0, load_generator.class5xxResponses());
  // No client sockets are rudely closed by server / no client sockets are
  // reset.
  EXPECT_EQ(0, load_generator.remoteCloses());
  EXPECT_EQ(0, load_generator.responseTimeouts());

  // Server accept callback is called for every client connection initiated.
  EXPECT_EQ(server_callbacks.connectionsAccepted(), connections_to_initiate);
  // Server request callback is called for every client request sent
  EXPECT_EQ(server_callbacks.requestsReceived(), requests_to_send);
  // Server does not close its own sockets but instead relies on the client to
  // initate the close
  EXPECT_EQ(0, server_callbacks.localCloses());
  // Server sees a client-initiated close for every socket it accepts
  EXPECT_EQ(server_callbacks.remoteCloses(),
            server_callbacks.connectionsAccepted());
}

TEST_F(ClientServerTest, AcceptAndClose) {
  constexpr uint32_t connections_to_initiate = 30;
  constexpr uint32_t requests_to_send = 30 * connections_to_initiate;

  //
  // Server Setup
  //

  // Immediately close any connection accepted.
  ServerCallbackHelper server_callbacks(
      [](ServerConnection &, ServerStream &, Envoy::Http::HeaderMapPtr &&) {
        GTEST_FATAL_FAILURE_(
            "Connections immediately closed so no response should be received");
      },
      [](ServerConnection &) -> ServerCallbackResult {
        return ServerCallbackResult::CLOSE;
      });

  server_.start(server_callbacks);

  //
  // Client setup
  //

  Envoy::Network::Address::InstanceConstSharedPtr address =
      listening_socket_.localAddress();
  LoadGenerator load_generator(client_, transport_socket_factory_,
                               HttpVersion::HTTP1, address);

  //
  // Exec test and wait for it to finish
  //

  Envoy::Http::HeaderMapPtr request{
      new Envoy::Http::TestHeaderMapImpl{{":method", "GET"},
                                         {":path", "/"},
                                         {":scheme", "http"},
                                         {":authority", "host"}}};
  load_generator.run(connections_to_initiate, requests_to_send,
                     std::move(request));

  // wait until the server has closed all connections created by the client
  server_callbacks.wait(load_generator.connectSuccesses());

  //
  // Evaluate test
  //

  // Assert that all connections succeed but no responses are received and the
  // server closes the connections.
  EXPECT_EQ(load_generator.connectSuccesses(), connections_to_initiate);
  EXPECT_EQ(0, load_generator.connectFailures());
  EXPECT_EQ(load_generator.remoteCloses(), connections_to_initiate);
  EXPECT_EQ(0, load_generator.localCloses());
  EXPECT_EQ(0, load_generator.responsesReceived());
  EXPECT_EQ(0, load_generator.class2xxResponses());
  EXPECT_EQ(0, load_generator.class4xxResponses());
  EXPECT_EQ(0, load_generator.class5xxResponses());
  EXPECT_EQ(0, load_generator.responseTimeouts());

  // Server accept callback is called for every client connection initiated.
  EXPECT_EQ(server_callbacks.connectionsAccepted(), connections_to_initiate);
  // Server request callback is never called
  EXPECT_EQ(0, server_callbacks.requestsReceived());
  // Server closes every connection
  EXPECT_EQ(server_callbacks.connectionsAccepted(),
            server_callbacks.localCloses());
  EXPECT_EQ(0, server_callbacks.remoteCloses());
}

TEST_F(ClientServerTest, SlowResponse) {
  constexpr uint32_t connections_to_initiate = 30;
  constexpr uint32_t requests_to_send = 30 * connections_to_initiate;

  //
  // Server Setup
  //

  // Take a really long time (500 msec) to send a 200 OK response.
  ServerCallbackHelper server_callbacks([](ServerConnection &,
                                           ServerStream &stream,
                                           Envoy::Http::HeaderMapPtr &&) {
    Envoy::Http::TestHeaderMapImpl response{{":status", "200"}};
    stream.sendResponseHeaders(response, std::chrono::milliseconds(500));
  });

  server_.start(server_callbacks);

  //
  // Client setup
  //

  Envoy::Network::Address::InstanceConstSharedPtr address =
      listening_socket_.localAddress();
  LoadGenerator load_generator(client_, transport_socket_factory_,
                               HttpVersion::HTTP1, address);

  //
  // Exec test and wait for it to finish
  //

  Envoy::Http::HeaderMapPtr request{
      new Envoy::Http::TestHeaderMapImpl{{":method", "GET"},
                                         {":path", "/"},
                                         {":scheme", "http"},
                                         {":authority", "host"}}};
  load_generator.run(connections_to_initiate, requests_to_send,
                     std::move(request), std::chrono::milliseconds(250));

  // wait until the server has closed all connections created by the client
  server_callbacks.wait(load_generator.connectSuccesses());

  //
  // Evaluate test
  //

  // Assert that all connections succeed but all responses timeout leading to
  // local closing of all connections.
  EXPECT_EQ(load_generator.connectSuccesses(), connections_to_initiate);
  EXPECT_EQ(0, load_generator.connectFailures());
  EXPECT_EQ(load_generator.responseTimeouts(), connections_to_initiate);
  EXPECT_EQ(load_generator.localCloses(), connections_to_initiate);
  EXPECT_EQ(0, load_generator.remoteCloses());
  EXPECT_EQ(0, load_generator.responsesReceived());
  EXPECT_EQ(0, load_generator.class2xxResponses());
  EXPECT_EQ(0, load_generator.class4xxResponses());
  EXPECT_EQ(0, load_generator.class5xxResponses());

  // Server accept callback is called for every client connection initiated.
  EXPECT_EQ(server_callbacks.connectionsAccepted(), connections_to_initiate);
  // Server receives a request on each connection
  EXPECT_EQ(server_callbacks.requestsReceived(), connections_to_initiate);
  // Server sees that the client closes each connection after it gives up
  EXPECT_EQ(server_callbacks.connectionsAccepted(),
            server_callbacks.remoteCloses());
  EXPECT_EQ(0, server_callbacks.localCloses());
}

TEST_F(ClientServerTest, NoServer) {
  constexpr uint32_t connections_to_initiate = 30;
  constexpr uint32_t requests_to_send = 30 * connections_to_initiate;

  // Create a listening socket bound to an ephemeral port picked by the kernel,
  // but don't create a server to call listen() on it.  Result will be
  // ECONNREFUSEDs and we won't accidentally send connects to another process.

  Envoy::Network::TcpListenSocket listening_socket(
      Envoy::Network::Utility::parseInternetAddressAndPort(fmt::format(
          "{}:{}", Envoy::Network::Test::getAnyAddressUrlString(ip_version_),
          0)),
      nullptr, true);
  uint16_t port =
      static_cast<uint16_t>(listening_socket.localAddress()->ip()->port());

  Envoy::Network::Address::InstanceConstSharedPtr address =
      Envoy::Network::Utility::parseInternetAddress("127.0.0.1", port);

  //
  // Client setup
  //

  LoadGenerator load_generator(client_, transport_socket_factory_,
                               HttpVersion::HTTP1, address);

  //
  // Exec test and wait for it to finish
  //

  Envoy::Http::HeaderMapPtr request{
      new Envoy::Http::TestHeaderMapImpl{{":method", "GET"},
                                         {":path", "/"},
                                         {":scheme", "http"},
                                         {":authority", "host"}}};
  load_generator.run(connections_to_initiate, requests_to_send,
                     std::move(request));

  //
  // Evaluate test
  //

  // All client connections fail
  EXPECT_EQ(load_generator.connectFailures(), connections_to_initiate);
  // Nothing else happened
  EXPECT_EQ(0, load_generator.connectSuccesses());
  EXPECT_EQ(0, load_generator.localCloses());
  EXPECT_EQ(0, load_generator.responseTimeouts());
  EXPECT_EQ(0, load_generator.responsesReceived());
  EXPECT_EQ(0, load_generator.class2xxResponses());
  EXPECT_EQ(0, load_generator.class4xxResponses());
  EXPECT_EQ(0, load_generator.class5xxResponses());
  EXPECT_EQ(0, load_generator.remoteCloses());
}

TEST_F(ClientServerTest, NoAccept) {
  constexpr uint32_t connections_to_initiate = 30;
  constexpr uint32_t requests_to_send = 30 * connections_to_initiate;

  //
  // Server Setup
  //

  ServerCallbackHelper server_callbacks;  // sends a 200 OK to everything
  server_.start(server_callbacks);

  // but don't call accept() on the listening socket
  server_.stopAcceptingConnections();

  //
  // Client setup
  //

  Envoy::Network::Address::InstanceConstSharedPtr address =
      listening_socket_.localAddress();
  LoadGenerator load_generator(client_, transport_socket_factory_,
                               HttpVersion::HTTP1, address);

  //
  // Exec test and wait for it to finish
  //

  Envoy::Http::HeaderMapPtr request{
      new Envoy::Http::TestHeaderMapImpl{{":method", "GET"},
                                         {":path", "/"},
                                         {":scheme", "http"},
                                         {":authority", "host"}}};
  load_generator.run(connections_to_initiate, requests_to_send,
                     std::move(request), std::chrono::milliseconds(250));

  //
  // Evaluate test
  //

  // Assert that all connections succeed but all responses timeout leading to
  // local closing of all connections.
  EXPECT_EQ(load_generator.connectSuccesses(), connections_to_initiate);
  EXPECT_EQ(0, load_generator.connectFailures());
  EXPECT_EQ(load_generator.responseTimeouts(), connections_to_initiate);
  EXPECT_EQ(load_generator.localCloses(), connections_to_initiate);
  EXPECT_EQ(0, load_generator.remoteCloses());
  EXPECT_EQ(0, load_generator.responsesReceived());
  EXPECT_EQ(0, load_generator.class2xxResponses());
  EXPECT_EQ(0, load_generator.class4xxResponses());
  EXPECT_EQ(0, load_generator.class5xxResponses());

  // From the server point of view, nothing happened
  EXPECT_EQ(0, server_callbacks.connectionsAccepted());
  EXPECT_EQ(0, server_callbacks.requestsReceived());
  EXPECT_EQ(0, server_callbacks.connectionsAccepted());
  EXPECT_EQ(0, server_callbacks.remoteCloses());
  EXPECT_EQ(0, server_callbacks.localCloses());
}

}  // namespace Integration
}  // namespace Mixer
