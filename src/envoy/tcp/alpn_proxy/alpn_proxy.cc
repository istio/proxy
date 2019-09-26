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

#include <cstdint>
#include <string>

#include "absl/base/internal/endian.h"
#include "absl/strings/string_view.h"
#include "common/buffer/buffer_impl.h"
#include "common/protobuf/utility.h"
#include "envoy/network/connection.h"
#include "envoy/stats/scope.h"
#include "src/envoy/tcp/alpn_proxy/alpn_proxy.h"
#include "src/envoy/tcp/alpn_proxy/alpn_proxy_initial_header.h"

namespace Envoy {
namespace Tcp {
namespace AlpnProxy {
namespace {

std::unique_ptr<::Envoy::Buffer::OwnedImpl> constructProxyHeaderData(
    const Envoy::ProtobufWkt::Any& proxy_data) {
  AlpnProxyInitialHeader initial_header;
  std::string proxy_data_str = proxy_data.SerializeAsString();
  // Converting from host to network byte order so that most significant byte is
  // placed first.
  initial_header.magic = absl::ghtonl(AlpnProxyInitialHeader::magic_number);
  initial_header.data_size = absl::ghtonl(proxy_data_str.length());

  ::Envoy::Buffer::OwnedImpl initial_header_buffer{
      absl::string_view(reinterpret_cast<const char*>(&initial_header),
                        sizeof(AlpnProxyInitialHeader))};
  auto proxy_data_buffer =
      std::make_unique<::Envoy::Buffer::OwnedImpl>(proxy_data_str);
  proxy_data_buffer->prepend(initial_header_buffer);
  return proxy_data_buffer;
}

}  // namespace

AlpnProxyConfig::AlpnProxyConfig(const std::string& stat_prefix,
                                 const std::string& protocol,
                                 const std::string& node_metadata_id,
                                 const FilterDirection filter_direction,
                                 Stats::Scope& scope)
    : scope_(scope),
      stat_prefix_(stat_prefix),
      protocol_(protocol),
      node_metadata_id_(node_metadata_id),
      filter_direction_(filter_direction),
      stats_(generateStats(stat_prefix, scope)) {}

Network::FilterStatus AlpnProxyFilter::onData(Buffer::Instance& data, bool) {
  switch (conn_state_) {
    case Invalid:
    case Done:
      // No work needed if connection state is Done or Invalid.
      return Network::FilterStatus::Continue;
    case ConnProtocolNotRead: {
      if (read_callbacks_->connection().nextProtocol() != config_->protocol_) {
        conn_state_ = Invalid;
        config_->stats().alpn_protocol_not_found_.inc();
        return Network::FilterStatus::Continue;
      } else {
        conn_state_ = WriteMetadata;
        config_->stats().alpn_protocol_found_.inc();
      }
    }
    case WriteMetadata: {
      // TODO(gargnupur): Try to move this just after alpn protocol is
      // determined and first onData is called in Downstream filter.
      if (config_->filter_direction_ == FilterDirection::Downstream) {
        writeNodeMetadata();
      }
    }
    case ReadingInitialHeader:
    case NeedMoreDataInitialHeader: {
      tryReadInitialProxyHeader(data);
      if (conn_state_ == NeedMoreDataInitialHeader) {
        return Network::FilterStatus::StopIteration;
      }
      if (conn_state_ == Invalid) {
        return Network::FilterStatus::Continue;
      }
    }
    case ReadingProxyHeader:
    case NeedMoreDataProxyHeader: {
      tryReadProxyData(data);
      if (conn_state_ == NeedMoreDataProxyHeader) {
        return Network::FilterStatus::StopIteration;
      }
      if (conn_state_ == Invalid) {
        return Network::FilterStatus::Continue;
      }
    }
    default:
      conn_state_ = Done;
      return Network::FilterStatus::Continue;
  }

  return Network::FilterStatus::Continue;
}

Network::FilterStatus AlpnProxyFilter::onNewConnection() {
  return Network::FilterStatus::Continue;
}

Network::FilterStatus AlpnProxyFilter::onWrite(Buffer::Instance&, bool) {
  switch (conn_state_) {
    case Invalid:
    case Done:
      // No work needed if connection state is Done or Invalid.
      return Network::FilterStatus::Continue;
    case ConnProtocolNotRead: {
      if (read_callbacks_->connection().nextProtocol() != config_->protocol_) {
        conn_state_ = Invalid;
        config_->stats().alpn_protocol_not_found_.inc();
        return Network::FilterStatus::Continue;
      } else {
        conn_state_ = WriteMetadata;
        config_->stats().alpn_protocol_found_.inc();
      }
    }
    case WriteMetadata: {
      // TODO(gargnupur): Try to move this just after alpn protocol is
      // determined and first onWrite is called in Upstream filter.
      if (config_->filter_direction_ == FilterDirection::Upstream) {
        writeNodeMetadata();
      }
    }
    case ReadingInitialHeader:
    case ReadingProxyHeader:
    case NeedMoreDataInitialHeader:
    case NeedMoreDataProxyHeader:
      // These are to be handled in Reading Pipeline.
      return Network::FilterStatus::Continue;
  }

  return Network::FilterStatus::Continue;
}

void AlpnProxyFilter::writeNodeMetadata() {
  if (conn_state_ != WriteMetadata) {
    return;
  }

  std::unique_ptr<const google::protobuf::Struct> metadata =
      readMetadata(config_->node_metadata_id_);
  if (metadata != nullptr) {
    Envoy::ProtobufWkt::Any metadata_any_value;
    *metadata_any_value.mutable_type_url() = StructTypeUrl;
    *metadata_any_value.mutable_value() = metadata->SerializeAsString();
    std::unique_ptr<::Envoy::Buffer::OwnedImpl> buf =
        constructProxyHeaderData(metadata_any_value);
    write_callbacks_->injectWriteDataToFilterChain(*buf, false);

    if (config_->filter_direction_ == FilterDirection::Downstream) {
      writeMetadata(DownstreamDynamicDataKey, *metadata);
    } else {
      writeMetadata(UpstreamDynamicDataKey, *metadata);
    }
    config_->stats().metadata_added_.inc();
  }

  conn_state_ = ReadingInitialHeader;
}

void AlpnProxyFilter::tryReadInitialProxyHeader(Buffer::Instance& data) {
  if (conn_state_ != ReadingInitialHeader &&
      conn_state_ != NeedMoreDataInitialHeader) {
    return;
  }
  const uint32_t initial_header_length = sizeof(AlpnProxyInitialHeader);
  if (data.length() < initial_header_length) {
    config_->stats().initial_header_not_found_.inc();
    // Not enough data to read. Wait for it to come.
    conn_state_ = NeedMoreDataInitialHeader;
    return;
  }
  std::string initial_header_buf = std::string(
      static_cast<const char*>(data.linearize(initial_header_length)),
      initial_header_length);
  const AlpnProxyInitialHeader* initial_header =
      reinterpret_cast<const AlpnProxyInitialHeader*>(
          initial_header_buf.c_str());
  if (absl::gntohl(initial_header->magic) !=
      AlpnProxyInitialHeader::magic_number) {
    config_->stats().initial_header_not_found_.inc();
    conn_state_ = Invalid;
    return;
  }
  proxy_data_length_ = absl::gntohl(initial_header->data_size);
  // Drain the initial header length bytes read.
  data.drain(initial_header_length);
  conn_state_ = ReadingProxyHeader;
}

void AlpnProxyFilter::tryReadProxyData(Buffer::Instance& data) {
  if (conn_state_ != ReadingProxyHeader &&
      conn_state_ != NeedMoreDataProxyHeader) {
    return;
  }
  if (data.length() < proxy_data_length_) {
    // Not enough data to read. Wait for it to come.
    conn_state_ = NeedMoreDataProxyHeader;
    return;
  }
  std::string proxy_data_buf =
      std::string(static_cast<const char*>(data.linearize(proxy_data_length_)),
                  proxy_data_length_);
  Envoy::ProtobufWkt::Any proxy_data;
  if (!proxy_data.ParseFromString(proxy_data_buf)) {
    config_->stats().header_not_found_.inc();
    conn_state_ = Invalid;
    return;
  }
  data.drain(proxy_data_length_);

  Envoy::ProtobufWkt::Struct struct_metadata =
      Envoy::MessageUtil::anyConvert<Envoy::ProtobufWkt::Struct>(proxy_data);
  if (config_->filter_direction_ == FilterDirection::Downstream) {
    writeMetadata(UpstreamDynamicDataKey, struct_metadata);
  } else {
    writeMetadata(DownstreamDynamicDataKey, struct_metadata);
  }
}

void AlpnProxyFilter::writeMetadata(const std::string key,
                                    const ProtobufWkt::Struct& value) {
  read_callbacks_->connection().streamInfo().setDynamicMetadata(key, value);
}

std::unique_ptr<const google::protobuf::Struct> AlpnProxyFilter::readMetadata(
    const std::string& key) {
  if (local_info_.node().has_metadata()) {
    auto metadata_fields = local_info_.node().metadata().fields();
    auto node_metadata = metadata_fields.find(key);
    if (node_metadata != metadata_fields.end()) {
      return std::make_unique<const google::protobuf::Struct>(
          node_metadata->second.struct_value());
    }
  }
  return nullptr;
}

}  // namespace AlpnProxy
}  // namespace Tcp
}  // namespace Envoy