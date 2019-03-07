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

#include "common/api/api_impl.h"
#include "common/grpc/common.h"
#include "common/http/codec_client.h"
#include "common/network/listen_socket_impl.h"
#include "common/stats/isolated_store_impl.h"
#include "test/test_common/test_time.h"
#include "test/test_common/utility.h"

namespace Mixer {
namespace Integration {

enum class ServerCloseReason {
  REMOTE_CLOSE,  // Peer closed or connection was reset after it was
                 // established.
  LOCAL_CLOSE    // This process decided to close the connection.
};

enum class ServerCallbackResult {
  CONTINUE,  // Leave the connection open
  CLOSE      // Close the connection.
};

class ServerStream {
 public:
  ServerStream();

  virtual ~ServerStream();

  ServerStream(ServerStream &&) = default;
  ServerStream &operator=(ServerStream &&) = default;

  /**
   * Send a HTTP header-only response and close the stream.
   *
   * @param response_headers the response headers
   * @param delay delay in msec before sending the response.  if 0 send
   * immediately
   */
  virtual void sendResponseHeaders(
      const Envoy::Http::HeaderMap &response_headers,
      const std::chrono::milliseconds delay =
          std::chrono::milliseconds(0)) PURE;

  /**
   * Send a gRPC response and close the stream
   *
   * @param status The gRPC status (carried in the HTTP response trailer)
   * @param response The gRPC response (carried in the HTTP response body)
   * @param delay delay in msec before sending the response.  if 0 send
   * immediately
   */
  virtual void sendGrpcResponse(Envoy::Grpc::Status::GrpcStatus status,
                                const Envoy::Protobuf::Message &response,
                                const std::chrono::milliseconds delay =
                                    std::chrono::milliseconds(0)) PURE;

 private:
  ServerStream(const ServerStream &) = delete;
  void operator=(const ServerStream &) = delete;
};

typedef std::unique_ptr<ServerStream> ServerStreamPtr;
typedef std::shared_ptr<ServerStream> ServerStreamSharedPtr;

class ServerConnection;

// NB: references passed to any of these callbacks are owned by the caller and
// must not be used after the callback returns -- except for the request headers
// which may be moved into the caller.
typedef std::function<ServerCallbackResult(ServerConnection &server_connection)>
    ServerAcceptCallback;
typedef std::function<void(ServerConnection &connection,
                           ServerCloseReason reason)>
    ServerCloseCallback;
// TODO support sending delayed responses
typedef std::function<void(ServerConnection &connection, ServerStream &stream,
                           Envoy::Http::HeaderMapPtr request_headers)>
    ServerRequestCallback;

class ServerConnection : public Envoy::Network::ReadFilter,
                         public Envoy::Network::ConnectionCallbacks,
                         public Envoy::Http::ServerConnectionCallbacks,
                         Envoy::Logger::Loggable<Envoy::Logger::Id::testing> {
 public:
  ServerConnection(const std::string &name, uint32_t id,
                   ServerRequestCallback request_callback,
                   ServerCloseCallback close_callback,
                   Envoy::Network::Connection &network_connection,
                   Envoy::Event::Dispatcher &dispatcher,
                   Envoy::Http::CodecClient::Type http_type,
                   Envoy::Stats::Scope &scope);

  virtual ~ServerConnection();

  ServerConnection(ServerConnection &&) = default;
  ServerConnection &operator=(ServerConnection &&) = default;

  const std::string &name() const;

  uint32_t id() const;

  Envoy::Network::Connection &networkConnection();
  const Envoy::Network::Connection &networkConnection() const;

  Envoy::Http::ServerConnection &httpConnection();
  const Envoy::Http::ServerConnection &httpConnection() const;

  Envoy::Event::Dispatcher &dispatcher();

  /**
   * For internal use
   */
  void removeStream(uint32_t stream_id);

  //
  // Envoy::Network::ReadFilter
  //

  virtual Envoy::Network::FilterStatus onData(Envoy::Buffer::Instance &data,
                                              bool end_stream) override;

  virtual Envoy::Network::FilterStatus onNewConnection() override;

  virtual void initializeReadFilterCallbacks(
      Envoy::Network::ReadFilterCallbacks &) override;

  //
  // Envoy::Http::ConnectionCallbacks
  //

  virtual void onGoAway() override;

  //
  // Envoy::Http::ServerConnectionCallbacks
  //

  virtual Envoy::Http::StreamDecoder &newStream(
      Envoy::Http::StreamEncoder &stream_encoder,
      bool is_internally_created = false) override;

  //
  // Envoy::Network::ConnectionCallbacks
  //

  virtual void onEvent(Envoy::Network::ConnectionEvent event) override;

  virtual void onAboveWriteBufferHighWatermark() override;

  virtual void onBelowWriteBufferLowWatermark() override;

 private:
  ServerConnection(const ServerConnection &) = delete;
  ServerConnection &operator=(const ServerConnection &) = delete;

  std::string name_;
  uint32_t id_;
  Envoy::Network::Connection &network_connection_;
  Envoy::Http::ServerConnectionPtr http_connection_;
  Envoy::Event::Dispatcher &dispatcher_;
  ServerRequestCallback request_callback_;
  ServerCloseCallback close_callback_;

  std::mutex streams_lock_;
  std::unordered_map<uint32_t, ServerStreamPtr> streams_;
  uint32_t stream_counter_{0U};
};

typedef std::unique_ptr<ServerConnection> ServerConnectionPtr;
typedef std::shared_ptr<ServerConnection> ServerConnectionSharedPtr;

class ServerFilterChain : public Envoy::Network::FilterChain {
 public:
  ServerFilterChain(
      Envoy::Network::TransportSocketFactory &transport_socket_factory);

  virtual ~ServerFilterChain();

  ServerFilterChain(ServerFilterChain &&) = default;
  ServerFilterChain &operator=(ServerFilterChain &&) = default;

  //
  // Envoy::Network::FilterChain
  //

  virtual const Envoy::Network::TransportSocketFactory &transportSocketFactory()
      const override;

  virtual const std::vector<Envoy::Network::FilterFactoryCb>
      &networkFilterFactories() const override;

 private:
  ServerFilterChain(const ServerFilterChain &) = delete;
  ServerFilterChain &operator=(const ServerFilterChain &) = delete;

  Envoy::Network::TransportSocketFactory &transport_socket_factory_;
  std::vector<Envoy::Network::FilterFactoryCb> network_filter_factories_;
};

/**
 * A convenience class for creating a listening socket bound to localhost
 */
class LocalListenSocket : public Envoy::Network::TcpListenSocket {
 public:
  /**
   * Create a listening socket bound to localhost.
   *
   * @param ip_version v4 or v6.  v4 by default.
   * @param port the port.  If 0, let the kernel allocate an avaiable ephemeral
   * port.  0 by default.
   * @param options socket options.  nullptr by default
   * @param bind_to_port if true immediately bind to the port, allocating one if
   * necessary.  true by default.
   */
  LocalListenSocket(
      Envoy::Network::Address::IpVersion ip_version =
          Envoy::Network::Address::IpVersion::v4,
      uint16_t port = 0,
      const Envoy::Network::Socket::OptionsSharedPtr &options = nullptr,
      bool bind_to_port = true);

  virtual ~LocalListenSocket();

  LocalListenSocket(LocalListenSocket &&) = default;
  LocalListenSocket &operator=(LocalListenSocket &&) = default;

 private:
  LocalListenSocket(const LocalListenSocket &) = delete;
  void operator=(const LocalListenSocket &) = delete;
};

/**
 * A convenience class for passing callbacks to a Server.  If no callbacks are
 * provided, default callbacks that track some simple metrics will be used.   If
 * callbacks are provided, they will be wrapped with callbacks that maintain the
 * same simple set of metrics.
 */
class ServerCallbackHelper {
 public:
  ServerCallbackHelper(ServerRequestCallback request_callback = nullptr,
                       ServerAcceptCallback accept_callback = nullptr,
                       ServerCloseCallback close_callback = nullptr);

  virtual ~ServerCallbackHelper();

  ServerCallbackHelper(ServerCallbackHelper &&) = default;
  ServerCallbackHelper &operator=(ServerCallbackHelper &&) = default;

  uint32_t connectionsAccepted() const;
  uint32_t requestsReceived() const;
  uint32_t localCloses() const;
  uint32_t remoteCloses() const;
  ServerAcceptCallback acceptCallback() const;
  ServerRequestCallback requestCallback() const;
  ServerCloseCallback closeCallback() const;

  /*
   * Wait until the server has accepted n connections and seen them closed (due
   * to error or client close)
   */
  void wait(uint32_t connections);

  /*
   * Wait until the server has seen a close for every connection it has
   * accepted.
   */
  void wait();

 private:
  ServerCallbackHelper(const ServerCallbackHelper &) = delete;
  void operator=(const ServerCallbackHelper &) = delete;

  ServerAcceptCallback accept_callback_;
  ServerAcceptCallback instrumented_accept_callback_;
  ServerRequestCallback request_callback_;
  ServerRequestCallback instrumented_request_callback_;
  ServerCloseCallback close_callback_;
  ServerCloseCallback instrumented_close_callback_;

  std::atomic<uint32_t> accepts_{0};
  std::atomic<uint32_t> requests_received_{0};
  uint32_t local_closes_{0};
  uint32_t remote_closes_{0};
  mutable absl::Mutex mutex_;
};

typedef std::unique_ptr<ServerCallbackHelper> ServerCallbackHelperPtr;
typedef std::shared_ptr<ServerCallbackHelper> ServerCallbackHelperSharedPtr;

class Server : public Envoy::Network::FilterChainManager,
               public Envoy::Network::FilterChainFactory,
               public Envoy::Network::ListenerConfig,
               Envoy::Logger::Loggable<Envoy::Logger::Id::testing> {
 public:
  // TODO make use of Network::Socket::OptionsSharedPtr
  Server(const std::string &name, Envoy::Network::Socket &listening_socket,
         Envoy::Network::TransportSocketFactory &transport_socket_factory,
         Envoy::Http::CodecClient::Type http_type);

  virtual ~Server();

  Server(Server &&) = default;
  Server &operator=(Server &&) = default;

  void start(ServerAcceptCallback accept_callback,
             ServerRequestCallback request_callback,
             ServerCloseCallback close_callback);

  void start(ServerCallbackHelper &helper);

  void stop();

  void stopAcceptingConnections();

  void startAcceptingConnections();

  const Envoy::Stats::Store &statsStore() const;

  // TODO does this affect socket recv buffer size?  Only for new connections?
  void setPerConnectionBufferLimitBytes(uint32_t limit);

  //
  // Envoy::Network::ListenerConfig
  //

  virtual Envoy::Network::FilterChainManager &filterChainManager() override;

  virtual Envoy::Network::FilterChainFactory &filterChainFactory() override;

  virtual Envoy::Network::Socket &socket() override;

  virtual const Envoy::Network::Socket &socket() const override;

  virtual bool bindToPort() override;

  virtual bool handOffRestoredDestinationConnections() const override;

  // TODO does this affect socket recv buffer size?  Only for new connections?
  virtual uint32_t perConnectionBufferLimitBytes() const override;

  virtual std::chrono::milliseconds listenerFiltersTimeout() const override;

  virtual Envoy::Stats::Scope &listenerScope() override;

  virtual uint64_t listenerTag() const override;

  virtual const std::string &name() const override;

  virtual bool reverseWriteFilterOrder() const override;

  //
  // Envoy::Network::FilterChainManager
  //

  virtual const Envoy::Network::FilterChain *findFilterChain(
      const Envoy::Network::ConnectionSocket &) const override;

  //
  // Envoy::Network::FilterChainFactory
  //

  virtual bool createNetworkFilterChain(
      Envoy::Network::Connection &network_connection,
      const std::vector<Envoy::Network::FilterFactoryCb> &) override;

  virtual bool createListenerFilterChain(
      Envoy::Network::ListenerFilterManager &) override;

 private:
  Server(const Server &) = delete;
  void operator=(const Server &) = delete;

  std::string name_;
  Envoy::Stats::IsolatedStoreImpl stats_;
  Envoy::Event::TestRealTimeSystem time_system_;
  Envoy::Api::Impl api_;
  Envoy::Event::DispatcherPtr dispatcher_;
  Envoy::Network::ConnectionHandlerPtr connection_handler_;
  Envoy::Thread::ThreadPtr thread_;
  std::atomic<bool> is_running{false};

  ServerAcceptCallback accept_callback_{nullptr};
  ServerRequestCallback request_callback_{nullptr};
  ServerCloseCallback close_callback_{nullptr};

  //
  // Envoy::Network::ListenerConfig
  //

  Envoy::Network::Socket &listening_socket_;
  std::atomic<uint32_t> connection_buffer_limit_bytes_{0U};

  //
  // Envoy::Network::FilterChainManager
  //

  ServerFilterChain server_filter_chain_;

  //
  // Envoy::Network::FilterChainFactory
  //

  Envoy::Http::CodecClient::Type http_type_;
  std::atomic<uint32_t> connection_counter_{0U};
};

typedef std::unique_ptr<Server> ServerPtr;
typedef std::shared_ptr<Server> ServerSharedPtr;

class ClusterHelper {
 public:
  /*template <typename... Args>
  ClusterHelper(Args &&... args) : servers_(std::forward<Args>(args)...){};*/

  ClusterHelper(std::initializer_list<ServerCallbackHelper *> server_callbacks);

  virtual ~ClusterHelper();

  const std::vector<ServerCallbackHelperPtr> &servers() const;
  std::vector<ServerCallbackHelperPtr> &servers();

  uint32_t connectionsAccepted() const;
  uint32_t requestsReceived() const;
  uint32_t localCloses() const;
  uint32_t remoteCloses() const;

  void wait();

 private:
  ClusterHelper(const ClusterHelper &) = delete;
  void operator=(const ClusterHelper &) = delete;

  std::vector<ServerCallbackHelperPtr> server_callback_helpers_;
};

}  // namespace Integration
}  // namespace Mixer
