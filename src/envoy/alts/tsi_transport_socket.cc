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
#include "src/envoy/alts/tsi_transport_socket.h"
#include "common/common/assert.h"
#include "common/common/enum_to_int.h"

namespace Envoy {
namespace Security {

TsiSocket::TsiSocket(TsiHandshakerPtr &&handshaker)
    : handshaker_(std::move(handshaker)), raw_buffer_callbacks_(*this) {
  raw_buffer_socket_.setTransportSocketCallbacks(raw_buffer_callbacks_);
}

TsiSocket::~TsiSocket() {
  tsi_frame_protector_destroy(frame_protector_);
  frame_protector_ = nullptr;
}

void TsiSocket::setTransportSocketCallbacks(
    Envoy::Network::TransportSocketCallbacks &callbacks) {
  callbacks_ = &callbacks;
}

std::string TsiSocket::protocol() const { return ""; }

Network::PostIoAction TsiSocket::doHandshake() {
  ASSERT(!handshake_complete_);
  ENVOY_CONN_LOG(debug, "TSI: doHandshake", callbacks_->connection());

  std::mutex mu;
  mu.lock();

  tsi_result status = TSI_OK;
  const unsigned char *bytes_to_send = nullptr;
  size_t bytes_to_send_size = 0;
  tsi_handshaker_result *handshaker_result = nullptr;

  handshaker_->Next(raw_read_buffer_,
                    [&](tsi_result _status, const unsigned char *_bytes_to_send,
                        size_t _bytes_to_send_size,
                        tsi_handshaker_result *_handshaker_result) {
                      status = _status;
                      bytes_to_send = _bytes_to_send;
                      bytes_to_send_size = _bytes_to_send_size;
                      handshaker_result = _handshaker_result;
                      mu.unlock();
                    });

  // TODO: make this async
  mu.lock();
  mu.unlock();
  ENVOY_CONN_LOG(
      debug, "TSI: doHandshake next done: status: {} received: {} to_send: {}",
      callbacks_->connection(), status, raw_read_buffer_.length(),
      bytes_to_send_size);
  raw_write_buffer_.add(bytes_to_send, bytes_to_send_size);
  raw_read_buffer_.drain(raw_read_buffer_.length());

  if (status == TSI_OK && handshaker_result != nullptr) {
    tsi_peer peer;
    tsi_handshaker_result_extract_peer(handshaker_result, &peer);
    ENVOY_CONN_LOG(debug, "TSI: Handshake successful: peer properties: {}",
                   callbacks_->connection(), peer.property_count);
    for (size_t i = 0; i < peer.property_count; ++i) {
      ENVOY_CONN_LOG(debug, "  {}: {}", callbacks_->connection(),
                     peer.properties[i].name,
                     std::string(peer.properties[i].value.data,
                                 peer.properties[i].value.length));
    }
    tsi_peer_destruct(&peer);

    const unsigned char *unused_bytes;
    size_t unused_byte_size;

    status = tsi_handshaker_result_get_unused_bytes(
        handshaker_result, &unused_bytes, &unused_byte_size);
    ASSERT(status == TSI_OK);
    if (unused_byte_size > 0) {
      raw_read_buffer_.add(unused_bytes, unused_byte_size);
    }
    ENVOY_CONN_LOG(debug, "TSI: Handshake successful: unused_bytes: {}",
                   callbacks_->connection(), unused_byte_size);

    status = tsi_handshaker_result_create_frame_protector(
        handshaker_result, &max_output_protected_frame_size_,
        &frame_protector_);
    ASSERT(status == TSI_OK);
    ENVOY_CONN_LOG(debug, "TSI: Handshake successful: max frame: {}",
                   callbacks_->connection(), max_output_protected_frame_size_);

    tsi_handshaker_result_destroy(handshaker_result);

    handshake_complete_ = true;
    callbacks_->raiseEvent(Network::ConnectionEvent::Connected);
  }

  return Network::PostIoAction::KeepOpen;
}

Network::IoResult TsiSocket::doRead(Buffer::Instance &buffer) {
  Network::IoResult result = raw_buffer_socket_.doRead(raw_read_buffer_);
  ENVOY_CONN_LOG(debug, "TSI: raw_read result action {} bytes {} end_stream {}",
                 callbacks_->connection(), enumToInt(result.action_),
                 result.bytes_processed_, result.end_stream_read_);
  if (result.action_ == Network::PostIoAction::Close &&
      result.bytes_processed_ == 0) {
    return result;
  }

  if (!handshake_complete_) {
    Network::PostIoAction action = doHandshake();
    if (action == Network::PostIoAction::Close || !handshake_complete_) {
      return {action, 0, false};
    }
  }

  if (handshake_complete_) {
    size_t message_size = raw_read_buffer_.length();
    auto *message_bytes = reinterpret_cast<unsigned char *>(
        raw_read_buffer_.linearize(message_size));

    unsigned char unprotected_buffer[4096];
    size_t unprotected_buffer_size = sizeof(unprotected_buffer);
    tsi_result result = TSI_OK;

    ENVOY_CONN_LOG(debug, "TSI: unprotecting message size: {}",
                   callbacks_->connection(), message_size);

    while (message_size > 0) {
      size_t unprotected_buffer_size_to_send = unprotected_buffer_size;
      size_t processed_message_size = message_size;
      result = tsi_frame_protector_unprotect(
          frame_protector_, message_bytes, &processed_message_size,
          unprotected_buffer, &unprotected_buffer_size_to_send);
      if (result != TSI_OK) break;
      buffer.add(unprotected_buffer, unprotected_buffer_size_to_send);
      message_bytes += processed_message_size;
      message_size -= processed_message_size;
      ENVOY_CONN_LOG(debug, "TSI: unprotecting message processed: {}",
                     callbacks_->connection(), processed_message_size);
    }

    raw_read_buffer_.drain(raw_read_buffer_.length() - message_size);

    ASSERT(result == TSI_OK);
  }

  return result;
}

Network::IoResult TsiSocket::doWrite(Buffer::Instance &buffer,
                                     bool end_stream) {
  if (!handshake_complete_) {
    Network::PostIoAction action = doHandshake();
    if (action == Network::PostIoAction::Close) {
      return {action, 0, false};
    }
  }

  if (handshake_complete_) {
    size_t message_size = buffer.length();
    auto *message_bytes =
        reinterpret_cast<unsigned char *>(buffer.linearize(message_size));

    unsigned char protected_buffer[4096];
    size_t protected_buffer_size = sizeof(protected_buffer);
    tsi_result result = TSI_OK;

    ENVOY_CONN_LOG(debug, "TSI: protecting message size: {}",
                   callbacks_->connection(), message_size);

    while (message_size > 0) {
      size_t protected_buffer_size_to_send = protected_buffer_size;
      size_t processed_message_size = message_size;
      result = tsi_frame_protector_protect(
          frame_protector_, message_bytes, &processed_message_size,
          protected_buffer, &protected_buffer_size_to_send);
      ASSERT(result == TSI_OK);
      raw_write_buffer_.add(protected_buffer, protected_buffer_size_to_send);
      message_bytes += processed_message_size;
      message_size -= processed_message_size;

      ENVOY_CONN_LOG(debug, "TSI: protecting message processed: {}",
                     callbacks_->connection(), processed_message_size);

      // Don't forget to flush.
      if (message_size == 0) {
        ENVOY_CONN_LOG(debug, "TSI: protecting message flush: {}",
                       callbacks_->connection(), message_size);
        size_t still_pending_size;
        do {
          protected_buffer_size_to_send = protected_buffer_size;
          result = tsi_frame_protector_protect_flush(
              frame_protector_, protected_buffer,
              &protected_buffer_size_to_send, &still_pending_size);
          if (result != TSI_OK) break;
          raw_write_buffer_.add(protected_buffer,
                                protected_buffer_size_to_send);
        } while (still_pending_size > 0);
      }
    }

    buffer.drain(buffer.length() - message_size);
    ASSERT(result == TSI_OK);
  }

  ENVOY_CONN_LOG(debug, "TSI: raw_write length {} end_stream {}",
                 callbacks_->connection(), raw_write_buffer_.length(),
                 end_stream);
  return raw_buffer_socket_.doWrite(raw_write_buffer_,
                                    end_stream && (buffer.length() == 0));
}

void TsiSocket::closeSocket(Network::ConnectionEvent) {}

void TsiSocket::onConnected() { ASSERT(!handshake_complete_); }

TsiSocketFactory::TsiSocketFactory(HandshakerFactoryCb handshaker_factory)
    : handshaker_factory_(handshaker_factory) {}

bool TsiSocketFactory::implementsSecureTransport() const { return true; }

Network::TransportSocketPtr TsiSocketFactory::createTransportSocket() const {
  return std::make_unique<TsiSocket>(handshaker_factory_());
}
}
}
