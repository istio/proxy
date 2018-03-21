/* Copyright 2018 Istio Authors. All Rights Reserved.
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
#include "envoy/event/dispatcher.h"

namespace Envoy {
namespace Security {

TsiSocket::TsiSocket(HandshakerFactory handshaker_factory)
    : handshaker_factory_(handshaker_factory), raw_buffer_callbacks_(*this) {
  raw_buffer_socket_.setTransportSocketCallbacks(raw_buffer_callbacks_);
}

TsiSocket::~TsiSocket() {
  ASSERT(!handshaker_);
  tsi_frame_protector_destroy(frame_protector_);
  frame_protector_ = nullptr;
}

void TsiSocket::setTransportSocketCallbacks(
    Envoy::Network::TransportSocketCallbacks &callbacks) {
  callbacks_ = &callbacks;

  handshaker_ = handshaker_factory_(callbacks.connection().dispatcher());
  handshaker_->setHandshakerCallbacks(*this);
}

std::string TsiSocket::protocol() const { return ""; }

Network::PostIoAction TsiSocket::doHandshake() {
  ASSERT(!handshake_complete_);
  ENVOY_CONN_LOG(debug, "TSI: doHandshake", callbacks_->connection());

  if (handshaker_next_calling_) {
    ENVOY_CONN_LOG(debug, "TSI: doHandshake next is pending, wait...",
                   callbacks_->connection());
    return Network::PostIoAction::KeepOpen;
  }

  doHandshakeNext();
  return Network::PostIoAction::KeepOpen;
}

void TsiSocket::doHandshakeNext() {
  ENVOY_CONN_LOG(debug, "TSI: doHandshake next: received: {}",
                 callbacks_->connection(), raw_read_buffer_.length());
  handshaker_next_calling_ = true;
  Buffer::OwnedImpl handshaker_buffer;
  handshaker_buffer.move(raw_read_buffer_);
  handshaker_->next(handshaker_buffer);
}

Network::PostIoAction TsiSocket::doHandshakeNextDone(
    NextResultPtr &&next_result) {
  ASSERT(next_result);

  ENVOY_CONN_LOG(debug, "TSI: doHandshake next done: status: {} to_send: {}",
                 callbacks_->connection(), next_result->status_,
                 next_result->to_send_->length());

  tsi_result status;
  tsi_handshaker_result *handshaker_result;

  status = next_result->status_;
  handshaker_result = next_result->result_;

  if (status != TSI_INCOMPLETE_DATA && status != TSI_OK) {
    ENVOY_CONN_LOG(debug, "TSI: Handshake failed: status: {}",
                   callbacks_->connection(), status);
    return Network::PostIoAction::Close;
  }

  if (next_result->to_send_->length() > 0) {
    raw_write_buffer_.move(*next_result->to_send_);
  }

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

  if (raw_read_buffer_.length() > 0) {
    callbacks_->setReadBufferReady();
  }
  return Network::PostIoAction::KeepOpen;
}

Network::IoResult TsiSocket::doRead(Buffer::Instance &buffer) {
  Network::IoResult result = raw_buffer_socket_.doRead(raw_read_buffer_);
  ENVOY_CONN_LOG(debug, "TSI: raw read result action {} bytes {} end_stream {}",
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
    // TODO(lizan): Do not linearize all buffer
    size_t message_size = raw_read_buffer_.length();
    auto *message_bytes = reinterpret_cast<unsigned char *>(
        raw_read_buffer_.linearize(message_size));

    // TODO(lizan): Tune the buffer size.
    unsigned char unprotected_buffer[4096];
    size_t unprotected_buffer_size = sizeof(unprotected_buffer);
    tsi_result status = TSI_OK;

    ENVOY_CONN_LOG(debug, "TSI: unprotecting message size: {}",
                   callbacks_->connection(), message_size);

    while (message_size > 0) {
      size_t unprotected_buffer_size_to_send = unprotected_buffer_size;
      size_t processed_message_size = message_size;
      status = tsi_frame_protector_unprotect(
          frame_protector_, message_bytes, &processed_message_size,
          unprotected_buffer, &unprotected_buffer_size_to_send);
      if (status != TSI_OK) {
        ENVOY_CONN_LOG(
            info, "TSI: unprotecting message failure {}, closing connection",
            callbacks_->connection(), status);
        result.action_ = Network::PostIoAction::Close;
        return result;
      }
      buffer.add(unprotected_buffer, unprotected_buffer_size_to_send);
      message_bytes += processed_message_size;
      message_size -= processed_message_size;
      ENVOY_CONN_LOG(debug, "TSI: unprotecting message processed: {}",
                     callbacks_->connection(), processed_message_size);
    }

    result.bytes_processed_ = raw_read_buffer_.length() - message_size;
    raw_read_buffer_.drain(result.bytes_processed_);

    ASSERT(status == TSI_OK);
  }

  ENVOY_CONN_LOG(debug, "TSI: do read result action {} bytes {} end_stream {}",
                 callbacks_->connection(), enumToInt(result.action_),
                 result.bytes_processed_, result.end_stream_read_);
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
      if (result != TSI_OK) {
        ENVOY_CONN_LOG(
            info, "TSI: protecting message failure {} wait until next write",
            callbacks_->connection(), result);
        break;
      }
      raw_write_buffer_.add(protected_buffer, protected_buffer_size_to_send);
      message_bytes += processed_message_size;
      message_size -= processed_message_size;

      ENVOY_CONN_LOG(debug, "TSI: protecting message processed: {}",
                     callbacks_->connection(), processed_message_size);
    }

    // Don't forget to flush.
    if (message_size == 0) {
      ENVOY_CONN_LOG(debug, "TSI: protecting message flush: {}",
                     callbacks_->connection(), message_size);
      size_t still_pending_size;
      do {
        size_t protected_buffer_size_to_send = protected_buffer_size;
        result = tsi_frame_protector_protect_flush(
            frame_protector_, protected_buffer, &protected_buffer_size_to_send,
            &still_pending_size);
        if (result != TSI_OK) {
          ENVOY_CONN_LOG(
              info, "TSI: protect flush message failure {}, closing connection",
              callbacks_->connection(), result);
          return {Network::PostIoAction::Close, 0, false};
        }
        raw_write_buffer_.add(protected_buffer, protected_buffer_size_to_send);
      } while (still_pending_size > 0);
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

void TsiSocket::closeSocket(Network::ConnectionEvent) {
  handshaker_.release()->deferredDelete();
}

void TsiSocket::onConnected() { ASSERT(!handshake_complete_); }

void TsiSocket::onNextDone(NextResultPtr &&result) {
  handshaker_next_calling_ = false;

  Network::PostIoAction action = doHandshakeNextDone(std::move(result));
  if (action == Network::PostIoAction::Close) {
    callbacks_->connection().close(Network::ConnectionCloseType::NoFlush);
  }
}

TsiSocketFactory::TsiSocketFactory(HandshakerFactory handshaker_factory)
    : handshaker_factory_(std::move(handshaker_factory)) {}

bool TsiSocketFactory::implementsSecureTransport() const { return true; }

Network::TransportSocketPtr TsiSocketFactory::createTransportSocket() const {
  return std::make_unique<TsiSocket>(handshaker_factory_);
}
}  // namespace Security
}  // namespace Envoy
