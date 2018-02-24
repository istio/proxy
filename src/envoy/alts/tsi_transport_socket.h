/* Copyright 2017 Istio Authors. All Rights Reserved.
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

#include "common/buffer/buffer_impl.h"
#include "common/network/raw_buffer_socket.h"
#include "envoy/network/transport_socket.h"
#include "src/envoy/alts/tsi_handshaker.h"

namespace Envoy {
namespace Security {

class TsiSocket : public Network::TransportSocket,
                  public TsiHandshakerCallbacks,
                  public Logger::Loggable<Logger::Id::connection> {
 public:
  explicit TsiSocket(TsiHandshakerPtr&& handshaker);
  virtual ~TsiSocket();

  // Network::TransportSocket
  void setTransportSocketCallbacks(
      Envoy::Network::TransportSocketCallbacks& callbacks) override;
  std::string protocol() const override;
  bool canFlushClose() override { return handshake_complete_; }
  Envoy::Ssl::Connection* ssl() override { return nullptr; }
  const Envoy::Ssl::Connection* ssl() const override { return nullptr; }
  Network::IoResult doWrite(Buffer::Instance& buffer, bool end_stream) override;
  void closeSocket(Network::ConnectionEvent event) override;
  Network::IoResult doRead(Buffer::Instance& buffer) override;
  void onConnected() override;

  // TsiHandshakerCallbacks
  void onNextDone(NextResult&& result) override;

 private:
  class RawBufferCallbacks : public Network::TransportSocketCallbacks {
   public:
    explicit RawBufferCallbacks(TsiSocket& parent) : parent_(parent) {}

    int fd() const override { return parent_.callbacks_->fd(); }
    Network::Connection& connection() override {
      return parent_.callbacks_->connection();
    }
    bool shouldDrainReadBuffer() override { return false; }
    void setReadBufferReady() override {}
    void raiseEvent(Network::ConnectionEvent) override {}

   private:
    TsiSocket& parent_;
  };

  Network::PostIoAction doHandshake();

  TsiHandshakerPtr handshaker_{};
  std::mutex handshaker_in_flight_;
  NextResult handshaker_result_;
  tsi_frame_protector* frame_protector_{};

  Envoy::Network::TransportSocketCallbacks* callbacks_{};
  RawBufferCallbacks raw_buffer_callbacks_;
  Network::RawBufferSocket raw_buffer_socket_;

  Envoy::Buffer::OwnedImpl raw_read_buffer_;
  Envoy::Buffer::OwnedImpl raw_write_buffer_;
  bool handshake_complete_{};
  size_t max_output_protected_frame_size_{};
};

class TsiSocketFactory : public Network::TransportSocketFactory {
 public:
  typedef std::function<TsiHandshakerPtr()> HandshakerFactoryCb;

  explicit TsiSocketFactory(HandshakerFactoryCb handshaker_factory);

  bool implementsSecureTransport() const override;
  Network::TransportSocketPtr createTransportSocket() const override;

 private:
  HandshakerFactoryCb handshaker_factory_;
};
}
}