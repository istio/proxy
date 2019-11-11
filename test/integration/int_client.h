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

#pragma once

#include <future>

#include "common/api/api_impl.h"
#include "common/common/thread.h"
#include "common/network/raw_buffer_socket.h"
#include "common/stats/isolated_store_impl.h"
#include "envoy/api/api.h"
#include "envoy/event/dispatcher.h"
#include "envoy/http/codec.h"
#include "envoy/network/address.h"
#include "envoy/thread/thread.h"
#include "fmt/printf.h"
#include "test/test_common/test_time.h"
#include "test/test_common/utility.h"

namespace Mixer {
namespace Integration {
enum class HttpVersion { HTTP1, HTTP2 };

class ClientStream;
class ClientConnection;
class Client;
typedef std::unique_ptr<ClientStream> ClientStreamPtr;
typedef std::shared_ptr<ClientStream> ClientStreamSharedPtr;
typedef std::unique_ptr<ClientConnection> ClientConnectionPtr;
typedef std::shared_ptr<ClientConnection> ClientConnectionSharedPtr;
typedef std::unique_ptr<Client> ClientPtr;
typedef std::shared_ptr<Client> ClientSharedPtr;

enum class ClientConnectionState {
  CONNECTED,  // Connection established.  Non-Terminal.  Will be followed by one
              // of the codes below.
  IDLE,  // Connection has no active streams.  Non-Terminal.  Close it, use it,
         // or put it in a pool.
};

enum class ClientCloseReason {
  CONNECT_FAILED,  // Connection could not be established
  REMOTE_CLOSE,    // Peer closed or connection was reset after it was
                   // established.
  LOCAL_CLOSE      // This process decided to close the connection.
};

enum class ClientCallbackResult {
  CONTINUE,  // Leave the connection open
  CLOSE      // Close the connection.
};

/**
 * Handle a non-terminal connection event asynchronously.
 *
 * @param connection The connection with the event
 * @param state The state of the connection (connected or idle).
 */
typedef std::function<ClientCallbackResult(ClientConnection &connection,
                                           ClientConnectionState state)>
    ClientConnectCallback;

/**
 * Handle a terminal connection close event asynchronously.
 *
 * @param connection The connection that was closed
 * @param reason The reason the connection was closed
 */
typedef std::function<void(ClientConnection &connection,
                           ClientCloseReason reason)>
    ClientCloseCallback;

/**
 * Handle a response asynchronously.
 *
 * @param connection The connection that received the response.
 * @param response_headers The response headers or null if timed out.
 */
typedef std::function<void(ClientConnection &connection,
                           Envoy::Http::HeaderMapPtr response_headers)>
    ClientResponseCallback;

class ClientConnection
    : public Envoy::Network::ConnectionCallbacks,
      public Envoy::Http::ConnectionCallbacks,
      public Envoy::Event::DeferredDeletable,
      protected Envoy::Logger::Loggable<Envoy::Logger::Id::testing> {
 public:
  ClientConnection(Client &client, uint32_t id,
                   ClientConnectCallback connect_callback,
                   ClientCloseCallback close_callback,
                   std::shared_ptr<Envoy::Event::Dispatcher> &dispatcher);

  virtual ~ClientConnection();

  const std::string &name() const;

  uint32_t id() const;

  virtual Envoy::Network::ClientConnection &networkConnection() PURE;

  virtual Envoy::Http::ClientConnection &httpConnection() PURE;

  Envoy::Event::Dispatcher &dispatcher();

  /**
   * Asynchronously send a request.  On HTTP1.1 connections at most one request
   * can be outstanding on a connection.  For HTTP2 multiple requests may
   * outstanding.
   *
   * @param request_headers
   * @param callback
   */
  virtual void sendRequest(const Envoy::Http::HeaderMap &request_headers,
                           ClientResponseCallback callback,
                           const std::chrono::milliseconds timeout =
                               std::chrono::milliseconds(5'000));

  /**
   * For internal use
   *
   * @param stream_id
   */
  void removeStream(uint32_t stream_id);

  //
  // Envoy::Network::ConnectionCallbacks
  //

  virtual void onEvent(Envoy::Network::ConnectionEvent event) override;

  virtual void onAboveWriteBufferHighWatermark() override;

  virtual void onBelowWriteBufferLowWatermark() override;

  //
  // Envoy::Http::ConnectionCallbacks
  //

  virtual void onGoAway() override;

 private:
  ClientConnection(const ClientConnection &) = delete;

  ClientConnection &operator=(const ClientConnection &) = delete;

  ClientStream &newStream(ClientResponseCallback callback);

  Client &client_;
  uint32_t id_;
  ClientConnectCallback connect_callback_;
  ClientCloseCallback close_callback_;
  std::shared_ptr<Envoy::Event::Dispatcher> dispatcher_;
  bool established_{false};

  std::mutex streams_lock_;
  std::unordered_map<uint32_t, ClientStreamPtr> streams_;
  std::atomic<uint32_t> stream_counter_{0U};
};

class Client : Envoy::Logger::Loggable<Envoy::Logger::Id::testing> {
 public:
  Client(const std::string &name);

  virtual ~Client();

  const std::string &name() const;

  /**
   * Start the client's dispatcher in a background thread.  This is a noop if
   * the client has already been started.  This will block until the dispatcher
   * is running on another thread.
   */
  void start();

  /**
   * Stop the client's dispatcher and join the background thread.  This will
   * block until the background thread exits.
   */
  void stop();

  /**
   * For internal use
   */
  void releaseConnection(uint32_t id);

  /**
   * For internal use
   */
  void releaseConnection(ClientConnection &connection);

  /**
   * Asynchronously connect to a peer.  The connect_callback will be called on
   * successful connection establishment and also on idle state, giving the
   * caller the opportunity to reuse or close connections.  The close_callback
   * will be called after the connection is closed, giving the caller the
   * opportunity to cleanup additional resources, etc.
   */
  void connect(
      Envoy::Network::TransportSocketFactory &socket_factory,
      HttpVersion http_version,
      Envoy::Network::Address::InstanceConstSharedPtr &address,
      const Envoy::Network::ConnectionSocket::OptionsSharedPtr &sockopts,
      ClientConnectCallback connect_callback,
      ClientCloseCallback close_callback);

 private:
  Client(const Client &) = delete;

  Client &operator=(const Client &) = delete;

  std::atomic<bool> is_running_{false};
  std::string name_;
  Envoy::Stats::IsolatedStoreImpl stats_;
  Envoy::Thread::ThreadPtr thread_;
  Envoy::Event::TestRealTimeSystem time_system_;
  Envoy::Api::Impl api_;
  std::shared_ptr<Envoy::Event::Dispatcher> dispatcher_;

  std::mutex connections_lock_;
  std::unordered_map<uint32_t, ClientConnectionPtr> connections_;
  uint32_t connection_counter_{0U};
};

class LoadGenerator : Envoy::Logger::Loggable<Envoy::Logger::Id::testing> {
 public:
  /**
   *  A wrapper around Client and its callbacks that implements a simple load
   * generator.
   *
   * @param socket_factory Socket factory (use for plain TCP vs. TLS)
   * @param http_version HTTP version (h1 vs h2)
   * @param address Address (ip addr, port, ip protocol version) to connect to
   * @param sockopts Socket options for the client sockets.  Use default if
   * null.
   */
  LoadGenerator(Client &client,
                Envoy::Network::TransportSocketFactory &socket_factory,
                HttpVersion http_version,
                Envoy::Network::Address::InstanceConstSharedPtr &address,
                const Envoy::Network::ConnectionSocket::OptionsSharedPtr
                    &sockopts = nullptr);

  virtual ~LoadGenerator();

  /**
   * Generate load and block until all connections have finished (successfully
   * or otherwise).
   *
   * @param connections Connections to create
   * @param requests Total requests across all connections to send
   * @param request The request to send
   * @param timeout The time in msec to wait to receive a response after sending
   * each request.
   */
  void run(uint32_t connections, uint32_t requests,
           Envoy::Http::HeaderMapPtr request,
           const std::chrono::milliseconds timeout =
               std::chrono::milliseconds(5'000));

  uint32_t connectFailures() const;
  uint32_t connectSuccesses() const;
  uint32_t responsesReceived() const;
  uint32_t responseTimeouts() const;
  uint32_t localCloses() const;
  uint32_t remoteCloses() const;
  uint32_t class2xxResponses() const;
  uint32_t class4xxResponses() const;
  uint32_t class5xxResponses() const;

 private:
  LoadGenerator(const LoadGenerator &) = delete;
  void operator=(const LoadGenerator &) = delete;

  uint32_t connections_to_initiate_{0};
  uint32_t requests_to_send_{0};
  Envoy::Http::HeaderMapPtr request_{};
  Client &client_;
  Envoy::Network::TransportSocketFactory &socket_factory_;
  HttpVersion http_version_;
  Envoy::Network::Address::InstanceConstSharedPtr address_;
  const Envoy::Network::ConnectionSocket::OptionsSharedPtr sockopts_;

  ClientConnectCallback connect_callback_;
  ClientResponseCallback response_callback_;
  ClientCloseCallback close_callback_;
  std::chrono::milliseconds timeout_{std::chrono::milliseconds(0)};
  std::atomic<int32_t> requests_remaining_{0};
  std::atomic<uint32_t> connect_failures_{0};
  std::atomic<uint32_t> connect_successes_{0};
  std::atomic<uint32_t> responses_received_{0};
  std::atomic<uint32_t> response_timeouts_{0};
  std::atomic<uint32_t> local_closes_{0};
  std::atomic<uint32_t> remote_closes_{0};
  std::atomic<uint32_t> class_2xx_{0};
  std::atomic<uint32_t> class_4xx_{0};
  std::atomic<uint32_t> class_5xx_{0};
  std::promise<bool> promise_all_connections_closed_;
};

typedef std::unique_ptr<LoadGenerator> LoadGeneratorPtr;

}  // namespace Integration
}  // namespace Mixer