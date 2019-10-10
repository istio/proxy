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

#include "int_client.h"

#include <future>
#include "common/http/http1/codec_impl.h"
#include "common/http/http2/codec_impl.h"
#include "common/stats/isolated_store_impl.h"
#include "envoy/thread/thread.h"

namespace Mixer {
namespace Integration {

class ClientStream : public Envoy::Http::StreamDecoder,
                     public Envoy::Http::StreamCallbacks,
                     Envoy::Logger::Loggable<Envoy::Logger::Id::testing> {
 public:
  ClientStream(uint32_t id, ClientConnection &connection,
               ClientResponseCallback callback)
      : id_(id), connection_(connection), callback_(callback) {}

  virtual ~ClientStream() {
    ENVOY_LOG(trace, "ClientStream({}:{}:{}) destroyed", connection_.name(),
              connection_.id(), id_);
  }

  //
  // Envoy::Http::StreamDecoder
  //

  virtual void decode100ContinueHeaders(Envoy::Http::HeaderMapPtr &&) override {
    ENVOY_LOG(trace, "ClientStream({}:{}:{}) got continue headers",
              connection_.name(), connection_.id(), id_);
  }

  virtual void decodeHeaders(Envoy::Http::HeaderMapPtr &&response_headers,
                             bool end_stream) override {
    ENVOY_LOG(debug, "ClientStream({}:{}:{}) got response headers",
              connection_.name(), connection_.id(), id_);

    response_headers_ = std::move(response_headers);

    if (end_stream) {
      onEndStream();
      // stream is now destroyed
    }
  }

  virtual void decodeData(Envoy::Buffer::Instance &, bool end_stream) override {
    ENVOY_LOG(debug, "ClientStream({}:{}:{}) got response body data",
              connection_.name(), connection_.id(), id_);

    if (end_stream) {
      onEndStream();
      // stream is now destroyed
    }
  }

  virtual void decodeTrailers(Envoy::Http::HeaderMapPtr &&) override {
    ENVOY_LOG(trace, "ClientStream({}:{}:{}) got response trailers",
              connection_.name(), connection_.id(), id_);
    onEndStream();
    // stream is now destroyed
  }

  virtual void decodeMetadata(Envoy::Http::MetadataMapPtr &&) override {
    ENVOY_LOG(trace, "ClientStream({}:{}):{} got metadata", connection_.name(),
              connection_.id(), id_);
  }

  //
  // Envoy::Http::StreamCallbacks
  //

  virtual void onResetStream(Envoy::Http::StreamResetReason reason,
                             absl::string_view) override {
    // TODO test with h2 to see if we get any of these and whether the
    // connection error handling is enough to handle it.
    switch (reason) {
      case Envoy::Http::StreamResetReason::LocalReset:
        ENVOY_LOG(trace, "ClientStream({}:{}:{}) was locally reset",
                  connection_.name(), connection_.id(), id_);
        break;
      case Envoy::Http::StreamResetReason::LocalRefusedStreamReset:
        ENVOY_LOG(trace, "ClientStream({}:{}:{}) refused local stream reset",
                  connection_.name(), connection_.id(), id_);
        break;
      case Envoy::Http::StreamResetReason::RemoteReset:
        ENVOY_LOG(trace, "ClientStream({}:{}:{}) was remotely reset",
                  connection_.name(), connection_.id(), id_);
        break;
      case Envoy::Http::StreamResetReason::RemoteRefusedStreamReset:
        ENVOY_LOG(trace, "ClientStream({}:{}:{}) refused remote stream reset",
                  connection_.name(), connection_.id(), id_);
        break;
      case Envoy::Http::StreamResetReason::ConnectionFailure:
        ENVOY_LOG(
            trace,
            "ClientStream({}:{}:{}) reseet due to initial connection failure",
            connection_.name(), connection_.id(), id_);
        break;
      case Envoy::Http::StreamResetReason::ConnectionTermination:
        ENVOY_LOG(
            trace,
            "ClientStream({}:{}:{}) reset due to underlying connection reset",
            connection_.name(), connection_.id(), id_);
        break;
      case Envoy::Http::StreamResetReason::Overflow:
        ENVOY_LOG(trace,
                  "ClientStream({}:{}:{}) reset due to resource overflow",
                  connection_.name(), connection_.id(), id_);
        break;
      default:
        ENVOY_LOG(trace, "ClientStream({}:{}:{}) reset due to unknown reason",
                  connection_.name(), connection_.id(), id_);
        break;
    }
  }

  virtual void onAboveWriteBufferHighWatermark() override {
    // TODO how should this be handled?
    ENVOY_LOG(trace, "ClientStream({}:{}:{}) above write buffer high watermark",
              connection_.name(), connection_.id(), id_);
  }

  virtual void onBelowWriteBufferLowWatermark() override {
    // TODO how should this be handled?
    ENVOY_LOG(trace, "ClientStream({}:{}:{}) below write buffer low watermark",
              connection_.name(), connection_.id(), id_);
  }

  virtual void sendRequest(const Envoy::Http::HeaderMap &request_headers,
                           const std::chrono::milliseconds timeout) {
    if (connection_.networkConnection().state() !=
        Envoy::Network::Connection::State::Open) {
      ENVOY_LOG(warn,
                "ClientStream({}:{}:{})'s underlying connection is not open!",
                connection_.name(), connection_.id(), id_);
      connection_.removeStream(id_);
      // This stream is now destroyed
      return;
    }

    Envoy::Http::StreamEncoder &encoder =
        connection_.httpConnection().newStream(*this);
    encoder.getStream().addCallbacks(*this);

    ENVOY_LOG(debug, "ClientStream({}:{}:{}) sending request headers",
              connection_.name(), connection_.id(), id_);
    encoder.encodeHeaders(request_headers, true);

    timeout_timer_ = connection_.dispatcher().createTimer([this, timeout]() {
      ENVOY_LOG(
          debug,
          "ClientStream({}:{}:{}) timed out after {} msec waiting for response",
          connection_.name(), connection_.id(), id_,
          static_cast<long int>(timeout.count()));
      callback_(connection_, nullptr);
      connection_.removeStream(id_);
      // This stream is now destroyed
    });
    timeout_timer_->enableTimer(timeout);
  }

 private:
  virtual void onEndStream() {
    ENVOY_LOG(debug, "ClientStream({}:{}:{}) complete", connection_.name(),
              connection_.id(), id_);
    callback_(connection_, std::move(response_headers_));
    connection_.removeStream(id_);
    // This stream is now destroyed
  }

  ClientStream(const ClientStream &) = delete;

  void operator=(const ClientStream &) = delete;

  uint32_t id_;
  ClientConnection &connection_;
  Envoy::Http::HeaderMapPtr response_headers_{nullptr};
  ClientResponseCallback callback_;
  Envoy::Event::TimerPtr timeout_timer_{nullptr};
};

class HttpClientReadFilter
    : public Envoy::Network::ReadFilter,
      Envoy::Logger::Loggable<Envoy::Logger::Id::testing> {
 public:
  HttpClientReadFilter(const std::string name, uint32_t id,
                       Envoy::Http::ClientConnection &connection)
      : name_(name), id_(id), connection_(connection) {}

  virtual ~HttpClientReadFilter() {}

  //
  // Envoy::Network::ReadFilter
  //

  virtual Envoy::Network::FilterStatus onData(Envoy::Buffer::Instance &data,
                                              bool end_stream) override {
    ENVOY_LOG(trace, "ClientConnection({}:{}) got data", name_, id_);

    connection_.dispatch(data);

    if (end_stream) {
      // TODO how should this be handled?
      ENVOY_LOG(error, "ClientConnection({}:{}) got end stream", name_, id_);
    }

    return Envoy::Network::FilterStatus::StopIteration;
  }

  virtual Envoy::Network::FilterStatus onNewConnection() override {
    return Envoy::Network::FilterStatus::Continue;
  }

  virtual void initializeReadFilterCallbacks(
      Envoy::Network::ReadFilterCallbacks &) override {}

 private:
  HttpClientReadFilter(const HttpClientReadFilter &) = delete;

  void operator=(const HttpClientReadFilter &) = delete;

  std::string name_;
  uint32_t id_;
  Envoy::Http::ClientConnection &connection_;
};

typedef std::unique_ptr<HttpClientReadFilter> HttpClientReadFilterPtr;
typedef std::shared_ptr<HttpClientReadFilter> HttpClientReadFilterSharedPtr;

class Http1ClientConnection : public ClientConnection {
 public:
  Http1ClientConnection(Client &client, uint32_t id,
                        ClientConnectCallback connect_callback,
                        ClientCloseCallback close_callback,
                        std::shared_ptr<Envoy::Event::Dispatcher> &dispatcher,
                        Envoy::Network::ClientConnectionPtr network_connection)
      : ClientConnection(client, id, connect_callback, close_callback,
                         dispatcher),
        network_connection_(std::move(network_connection)),
        http_connection_(*network_connection_, *this,
                         Envoy::Http::DEFAULT_MAX_HEADERS_COUNT),
        read_filter_{std::make_shared<HttpClientReadFilter>(client.name(), id,
                                                            http_connection_)} {
    network_connection_->addReadFilter(read_filter_);
    network_connection_->addConnectionCallbacks(*this);
  }

  virtual ~Http1ClientConnection() {}

  virtual Envoy::Network::ClientConnection &networkConnection() override {
    return *network_connection_;
  }

  virtual Envoy::Http::ClientConnection &httpConnection() override {
    return http_connection_;
  }

 private:
  Http1ClientConnection(const Http1ClientConnection &) = delete;

  Http1ClientConnection &operator=(const Http1ClientConnection &) = delete;

  Envoy::Network::ClientConnectionPtr network_connection_;
  Envoy::Http::Http1::ClientConnectionImpl http_connection_;
  HttpClientReadFilterSharedPtr read_filter_;
};

static constexpr uint32_t max_request_headers_kb = 2U;

class Http2ClientConnection : public ClientConnection {
 public:
  Http2ClientConnection(Client &client, uint32_t id,
                        ClientConnectCallback connect_callback,
                        ClientCloseCallback close_callback,
                        std::shared_ptr<Envoy::Event::Dispatcher> &dispatcher,
                        Envoy::Network::ClientConnectionPtr network_connection)
      : ClientConnection(client, id, connect_callback, close_callback,
                         dispatcher),
        stats_(),
        settings_(),
        network_connection_(std::move(network_connection)),
        http_connection_(*network_connection_, *this, stats_, settings_,
                         max_request_headers_kb,
                         Envoy::Http::DEFAULT_MAX_HEADERS_COUNT),
        read_filter_{std::make_shared<HttpClientReadFilter>(client.name(), id,
                                                            http_connection_)} {
    network_connection_->addReadFilter(read_filter_);
    network_connection_->addConnectionCallbacks(*this);
  }

  virtual ~Http2ClientConnection() {}

  virtual Envoy::Network::ClientConnection &networkConnection() override {
    return *network_connection_;
  }

  virtual Envoy::Http::ClientConnection &httpConnection() override {
    return http_connection_;
  }

 private:
  Http2ClientConnection(const Http2ClientConnection &) = delete;

  Http2ClientConnection &operator=(const Http2ClientConnection &) = delete;

  Envoy::Stats::IsolatedStoreImpl stats_;
  Envoy::Http::Http2Settings settings_;
  Envoy::Network::ClientConnectionPtr network_connection_;
  Envoy::Http::Http2::ClientConnectionImpl http_connection_;
  HttpClientReadFilterSharedPtr read_filter_;
};

ClientStream &ClientConnection::newStream(ClientResponseCallback callback) {
  std::lock_guard<std::mutex> guard(streams_lock_);

  uint32_t id = stream_counter_++;
  ClientStreamPtr stream = std::make_unique<ClientStream>(id, *this, callback);
  ClientStream *raw = stream.get();
  streams_[id] = std::move(stream);

  return *raw;
}

ClientConnection::ClientConnection(
    Client &client, uint32_t id, ClientConnectCallback connect_callback,
    ClientCloseCallback close_callback,
    std::shared_ptr<Envoy::Event::Dispatcher> &dispatcher)
    : client_(client),
      id_(id),
      connect_callback_(connect_callback),
      close_callback_(close_callback),
      dispatcher_(dispatcher) {}

ClientConnection::~ClientConnection() {
  ENVOY_LOG(trace, "ClientConnection({}:{}) destroyed", client_.name(), id_);
}

const std::string &ClientConnection::name() const { return client_.name(); }

uint32_t ClientConnection::id() const { return id_; }

Envoy::Event::Dispatcher &ClientConnection::dispatcher() {
  return *dispatcher_;
};

void ClientConnection::removeStream(uint32_t stream_id) {
  unsigned long size = 0UL;

  {
    std::lock_guard<std::mutex> guard(streams_lock_);
    streams_.erase(stream_id);
    size = streams_.size();
  }

  if (0 == size) {
    ENVOY_LOG(debug, "ClientConnection({}:{}) is idle", client_.name(), id_);
    if (ClientCallbackResult::CLOSE ==
        connect_callback_(*this, ClientConnectionState::IDLE)) {
      // This will trigger a
      // networkConnection().onEvent(Envoy::Network::ConnectionEvent::LocalClose)
      networkConnection().close(Envoy::Network::ConnectionCloseType::NoFlush);
    }
  }
}

void ClientConnection::onEvent(Envoy::Network::ConnectionEvent event) {
  switch (event) {
    // properly on connection destruction.
    case Envoy::Network::ConnectionEvent::RemoteClose:
      if (established_) {
        ENVOY_LOG(debug, "ClientConnection({}:{}) closed by peer or reset",
                  client_.name(), id_);
        close_callback_(*this, ClientCloseReason::REMOTE_CLOSE);
      } else {
        ENVOY_LOG(debug, "ClientConnection({}:{}) cannot connect to peer",
                  client_.name(), id_);
        close_callback_(*this, ClientCloseReason::CONNECT_FAILED);
      }
      client_.releaseConnection(*this);
      // ClientConnection has been destroyed
      return;
    case Envoy::Network::ConnectionEvent::LocalClose:
      ENVOY_LOG(debug, "ClientConnection({}:{}) closed locally", client_.name(),
                id_);
      close_callback_(*this, ClientCloseReason::LOCAL_CLOSE);
      client_.releaseConnection(*this);
      // ClientConnection has been destroyed
      return;
    case Envoy::Network::ConnectionEvent::Connected:
      established_ = true;
      ENVOY_LOG(debug, "ClientConnection({}:{}) established", client_.name(),
                id_);
      if (ClientCallbackResult::CLOSE ==
          connect_callback_(*this, ClientConnectionState::CONNECTED)) {
        // This will trigger a
        // networkConnection().onEvent(Envoy::Network::ConnectionEvent::LocalClose)
        networkConnection().close(Envoy::Network::ConnectionCloseType::NoFlush);
      }
      break;
    default:
      ENVOY_LOG(error, "ClientConnection({}:{}) got unknown event",
                client_.name(), id_);
  };
}

void ClientConnection::onAboveWriteBufferHighWatermark() {
  ENVOY_LOG(warn, "ClientConnection({}:{}) above write buffer high watermark",
            client_.name(), id_);
  // TODO how should this be handled?
  httpConnection().onUnderlyingConnectionAboveWriteBufferHighWatermark();
}

void ClientConnection::onBelowWriteBufferLowWatermark() {
  ENVOY_LOG(warn, "ClientConnection({}:{}) below write buffer low watermark",
            client_.name(), id_);
  // TODO how should this be handled?
  httpConnection().onUnderlyingConnectionBelowWriteBufferLowWatermark();
}

void ClientConnection::onGoAway() {
  ENVOY_LOG(warn, "ClientConnection({}:{}) remote closed", client_.name(), id_);
  // TODO how should this be handled?
}

void ClientConnection::sendRequest(const Envoy::Http::HeaderMap &headers,
                                   ClientResponseCallback callback,
                                   const std::chrono::milliseconds timeout) {
  newStream(callback).sendRequest(headers, timeout);
}

Client::Client(const std::string &name)
    : name_(name),
      stats_(),
      thread_(nullptr),
      time_system_(),
      api_(Envoy::Thread::ThreadFactorySingleton::get(), stats_, time_system_,
           Envoy::Filesystem::fileSystemForTest()),
      dispatcher_{api_.allocateDispatcher()} {}

Client::~Client() {
  stop();
  ENVOY_LOG(trace, "Client({}) destroyed", name_);
}

const std::string &Client::name() const { return name_; }

void Client::connect(
    Envoy::Network::TransportSocketFactory &socket_factory,
    HttpVersion http_version,
    Envoy::Network::Address::InstanceConstSharedPtr &address,
    const Envoy::Network::ConnectionSocket::OptionsSharedPtr &sockopts,
    ClientConnectCallback connect_cb, ClientCloseCallback close_cb) {
  dispatcher_->post([this, &socket_factory, http_version, address, sockopts,
                     connect_cb, close_cb]() {
    Envoy::Network::ClientConnectionPtr connection =
        dispatcher_->createClientConnection(
            address, nullptr, socket_factory.createTransportSocket(nullptr),
            sockopts);
    uint32_t id = connection_counter_++;

    ClientConnectionPtr ptr;
    if (HttpVersion::HTTP1 == http_version) {
      ptr = std::make_unique<Http1ClientConnection>(
          *this, id, connect_cb, close_cb, dispatcher_, std::move(connection));
    } else {
      ptr = std::make_unique<Http2ClientConnection>(
          *this, id, connect_cb, close_cb, dispatcher_, std::move(connection));
    }
    ClientConnection *raw = ptr.get();

    {
      std::lock_guard<std::mutex> guard(connections_lock_);
      connections_[id] = std::move(ptr);
    }

    ENVOY_LOG(debug, "ClientConnection({}:{}) connecting to {}", name_, id,
              address->asString());
    raw->networkConnection().connect();
  });
}

void Client::start() {
  std::promise<bool> promise;

  if (is_running_) {
    return;
  }

  thread_ = api_.threadFactory().createThread([this, &promise]() {
    ENVOY_LOG(debug, "Client({}) dispatcher started", name_);

    is_running_ = true;
    promise.set_value(true);  // do not use promise again after this
    while (is_running_) {
      dispatcher_->run(Envoy::Event::Dispatcher::RunType::NonBlock);
    }

    ENVOY_LOG(debug, "Client({}) dispatcher stopped", name_);
  });

  promise.get_future().get();
}

void Client::stop() {
  ENVOY_LOG(debug, "Client({}) stop requested", name_);

  is_running_ = false;
  if (thread_) {
    thread_->join();
    thread_ = nullptr;
  }

  ENVOY_LOG(debug, "Client({}) stopped", name_);
}

void Client::releaseConnection(uint32_t id) {
  size_t erased = 0;
  {
    std::lock_guard<std::mutex> guard(connections_lock_);
    dispatcher_->deferredDelete(std::move(connections_[id]));
    erased = connections_.erase(id);
  }
  if (1 > erased) {
    ENVOY_LOG(error, "Client({}) cannot remove ClientConnection({}:{})", name_,
              name_, id);
  }
}

void Client::releaseConnection(ClientConnection &connection) {
  releaseConnection(connection.id());
}

LoadGenerator::LoadGenerator(
    Client &client, Envoy::Network::TransportSocketFactory &socket_factory,
    HttpVersion http_version,
    Envoy::Network::Address::InstanceConstSharedPtr &address,
    const Envoy::Network::ConnectionSocket::OptionsSharedPtr &sockopts)
    : client_(client),
      socket_factory_(socket_factory),
      http_version_(http_version),
      address_(address),
      sockopts_(sockopts) {
  response_callback_ = [this](ClientConnection &connection,
                              Envoy::Http::HeaderMapPtr response) {
    if (!response) {
      ENVOY_LOG(debug, "Connection({}:{}) timedout waiting for response",
                connection.name(), connection.id());
      ++response_timeouts_;
      return;
    }

    ++responses_received_;

    uint64_t status = 0;
    if (!Envoy::StringUtil::atoull(response->Status()->value().c_str(),
                                   status)) {
      ENVOY_LOG(error, "Connection({}:{}) received response with bad status",
                connection.name(), connection.id());
    } else if (200 <= status && status < 300) {
      ++class_2xx_;
    } else if (400 <= status && status < 500) {
      ++class_4xx_;
    } else if (500 <= status && status < 600) {
      ++class_5xx_;
    }

    if (0 >= requests_remaining_--) {
      // Break if we've already sent or scheduled every request we wanted to
      return;
    }

    connection.sendRequest(*request_, response_callback_, timeout_);
  };

  connect_callback_ = [this](
                          ClientConnection &connection,
                          ClientConnectionState state) -> ClientCallbackResult {
    if (state == ClientConnectionState::IDLE) {
      // This will result in a CloseReason::LOCAL_CLOSE passed to the
      // close_callback
      return ClientCallbackResult::CLOSE;
    }
    // If ConnectionResult::SUCCESS:

    ++connect_successes_;

    if (0 >= requests_remaining_--) {
      // This will result in a ConnectionState::IDLE passed to this callback
      // once all active streams have finished.
      return ClientCallbackResult::CONTINUE;
    }

    connection.sendRequest(*request_, response_callback_, timeout_);

    return ClientCallbackResult::CONTINUE;
  };

  close_callback_ = [this](ClientConnection &, ClientCloseReason reason) {
    switch (reason) {
      case ClientCloseReason::CONNECT_FAILED:
        ++connect_failures_;
        break;
      case ClientCloseReason::REMOTE_CLOSE:
        ++remote_closes_;
        break;
      case ClientCloseReason::LOCAL_CLOSE:
        // We initiated this by responding to ConnectionState::IDLE with a
        // CallbackResult::Close
        ++local_closes_;
        break;
    }

    // Unblock run() once we've seen a close for every connection initiated.
    if (remote_closes_ + local_closes_ + connect_failures_ >=
        connections_to_initiate_) {
      promise_all_connections_closed_.set_value(true);
    }
  };
}

LoadGenerator::~LoadGenerator() {}

void LoadGenerator::run(uint32_t connections, uint32_t requests,
                        Envoy::Http::HeaderMapPtr request,
                        const std::chrono::milliseconds timeout) {
  connections_to_initiate_ = connections;
  requests_to_send_ = requests;
  request_ = std::move(request);
  promise_all_connections_closed_ = std::promise<bool>();
  timeout_ = timeout;
  requests_remaining_ = requests_to_send_;
  connect_failures_ = 0;
  connect_successes_ = 0;
  responses_received_ = 0;
  response_timeouts_ = 0;
  local_closes_ = 0;
  remote_closes_ = 0;
  class_2xx_ = 0;
  class_4xx_ = 0;
  class_5xx_ = 0;

  client_.start();  // idempotent

  for (uint32_t i = 0; i < connections_to_initiate_; ++i) {
    client_.connect(socket_factory_, http_version_, address_, sockopts_,
                    connect_callback_, close_callback_);
  }

  promise_all_connections_closed_.get_future().get();
}

uint32_t LoadGenerator::connectFailures() const { return connect_failures_; }
uint32_t LoadGenerator::connectSuccesses() const { return connect_successes_; }
uint32_t LoadGenerator::responsesReceived() const {
  return responses_received_;
}
uint32_t LoadGenerator::responseTimeouts() const { return response_timeouts_; }
uint32_t LoadGenerator::localCloses() const { return local_closes_; }
uint32_t LoadGenerator::remoteCloses() const { return remote_closes_; }
uint32_t LoadGenerator::class2xxResponses() const { return class_2xx_; }
uint32_t LoadGenerator::class4xxResponses() const { return class_4xx_; }
uint32_t LoadGenerator::class5xxResponses() const { return class_5xx_; }

}  // namespace Integration
}  // namespace Mixer
