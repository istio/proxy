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

#include "source/extensions/filters/network/metadata_exchange/metadata_exchange.h"

#include <cstdint>
#include <string>

#include "absl/base/internal/endian.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "envoy/network/connection.h"
#include "envoy/stats/scope.h"
#include "source/common/buffer/buffer_impl.h"
#include "source/common/protobuf/utility.h"
#include "source/extensions/filters/network/metadata_exchange/metadata_exchange_initial_header.h"

namespace Envoy {
namespace Tcp {
namespace MetadataExchange {
namespace {

std::unique_ptr<::Envoy::Buffer::OwnedImpl>
constructProxyHeaderData(const Envoy::ProtobufWkt::Any& proxy_data) {
  MetadataExchangeInitialHeader initial_header;
  std::string proxy_data_str = proxy_data.SerializeAsString();
  // Converting from host to network byte order so that most significant byte is
  // placed first.
  initial_header.magic = absl::ghtonl(MetadataExchangeInitialHeader::magic_number);
  initial_header.data_size = absl::ghtonl(proxy_data_str.length());

  ::Envoy::Buffer::OwnedImpl initial_header_buffer{absl::string_view(
      reinterpret_cast<const char*>(&initial_header), sizeof(MetadataExchangeInitialHeader))};
  auto proxy_data_buffer = std::make_unique<::Envoy::Buffer::OwnedImpl>(proxy_data_str);
  proxy_data_buffer->prepend(initial_header_buffer);
  return proxy_data_buffer;
}

bool serializeToStringDeterministic(const google::protobuf::Struct& metadata,
                                    std::string* metadata_bytes) {
  google::protobuf::io::StringOutputStream md(metadata_bytes);
  google::protobuf::io::CodedOutputStream mcs(&md);

  mcs.SetSerializationDeterministic(true);
  if (!metadata.SerializeToCodedStream(&mcs)) {
    return false;
  }
  return true;
}

} // namespace

MetadataExchangeConfig::MetadataExchangeConfig(const std::string& stat_prefix,
                                               const std::string& protocol,
                                               const FilterDirection filter_direction,
                                               Stats::Scope& scope)
    : scope_(scope), stat_prefix_(stat_prefix), protocol_(protocol),
      filter_direction_(filter_direction), stats_(generateStats(stat_prefix, scope)) {}

Network::FilterStatus MetadataExchangeFilter::onData(Buffer::Instance& data, bool end_stream) {
  switch (conn_state_) {
  case Invalid:
    FALLTHRU;
  case Done:
    // No work needed if connection state is Done or Invalid.
    return Network::FilterStatus::Continue;
  case ConnProtocolNotRead: {
    // If Alpn protocol is not the expected one, then return.
    // Else find and write node metadata.
    if (read_callbacks_->connection().nextProtocol() != config_->protocol_) {
      ENVOY_LOG(trace, "Alpn Protocol Not Found. Expected {}, Got {}", config_->protocol_,
                read_callbacks_->connection().nextProtocol());
      setMetadataNotFoundFilterState();
      conn_state_ = Invalid;
      config_->stats().alpn_protocol_not_found_.inc();
      return Network::FilterStatus::Continue;
    }
    conn_state_ = WriteMetadata;
    config_->stats().alpn_protocol_found_.inc();
    FALLTHRU;
  }
  case WriteMetadata: {
    // TODO(gargnupur): Try to move this just after alpn protocol is
    // determined and first onData is called in Downstream filter.
    // If downstream filter, write metadata.
    // Otherwise, go ahead and try to read initial header and proxy data.
    writeNodeMetadata();
    FALLTHRU;
  }
  case ReadingInitialHeader:
  case NeedMoreDataInitialHeader: {
    tryReadInitialProxyHeader(data);
    if (conn_state_ == NeedMoreDataInitialHeader) {
      if (end_stream) {
        // Upstream has entered a half-closed state, and will be sending no more data.
        // Since this plugin would expect additional headers, but none is forthcoming,
        // do not block the tcp_proxy downstream of us from draining the buffer.
        ENVOY_LOG(debug, "Upstream closed early, aborting istio-peer-exchange");
        conn_state_ = Invalid;
        return Network::FilterStatus::Continue;
      }
      return Network::FilterStatus::StopIteration;
    }
    if (conn_state_ == Invalid) {
      return Network::FilterStatus::Continue;
    }
    FALLTHRU;
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
    FALLTHRU;
  }
  default:
    conn_state_ = Done;
    return Network::FilterStatus::Continue;
  }

  return Network::FilterStatus::Continue;
}

Network::FilterStatus MetadataExchangeFilter::onNewConnection() {
  return Network::FilterStatus::Continue;
}

Network::FilterStatus MetadataExchangeFilter::onWrite(Buffer::Instance&, bool) {
  switch (conn_state_) {
  case Invalid:
  case Done:
    // No work needed if connection state is Done or Invalid.
    return Network::FilterStatus::Continue;
  case ConnProtocolNotRead: {
    if (read_callbacks_->connection().nextProtocol() != config_->protocol_) {
      ENVOY_LOG(trace, "Alpn Protocol Not Found. Expected {}, Got {}", config_->protocol_,
                read_callbacks_->connection().nextProtocol());
      setMetadataNotFoundFilterState();
      conn_state_ = Invalid;
      config_->stats().alpn_protocol_not_found_.inc();
      return Network::FilterStatus::Continue;
    } else {
      conn_state_ = WriteMetadata;
      config_->stats().alpn_protocol_found_.inc();
    }
    FALLTHRU;
  }
  case WriteMetadata: {
    // TODO(gargnupur): Try to move this just after alpn protocol is
    // determined and first onWrite is called in Upstream filter.
    writeNodeMetadata();
    FALLTHRU;
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

void MetadataExchangeFilter::writeNodeMetadata() {
  if (conn_state_ != WriteMetadata) {
    return;
  }

  Envoy::ProtobufWkt::Struct data;
  Envoy::ProtobufWkt::Struct* metadata =
      (*data.mutable_fields())[ExchangeMetadataHeader].mutable_struct_value();
  getMetadata(metadata);
  std::string metadata_id = getMetadataId();
  if (!metadata_id.empty()) {
    (*data.mutable_fields())[ExchangeMetadataHeaderId].set_string_value(metadata_id);
  }
  if (data.fields_size() > 0) {
    Envoy::ProtobufWkt::Any metadata_any_value;
    *metadata_any_value.mutable_type_url() = StructTypeUrl;
    std::string serialized_data;
    serializeToStringDeterministic(data, &serialized_data);
    *metadata_any_value.mutable_value() = serialized_data;
    std::unique_ptr<::Envoy::Buffer::OwnedImpl> buf = constructProxyHeaderData(metadata_any_value);
    write_callbacks_->injectWriteDataToFilterChain(*buf, false);
    config_->stats().metadata_added_.inc();
  }

  conn_state_ = ReadingInitialHeader;
}

void MetadataExchangeFilter::tryReadInitialProxyHeader(Buffer::Instance& data) {
  if (conn_state_ != ReadingInitialHeader && conn_state_ != NeedMoreDataInitialHeader) {
    return;
  }
  const uint32_t initial_header_length = sizeof(MetadataExchangeInitialHeader);
  if (data.length() < initial_header_length) {
    config_->stats().initial_header_not_found_.inc();
    // Not enough data to read. Wait for it to come.
    ENVOY_LOG(debug, "Alpn Protocol matched. Waiting to read more initial header.");
    conn_state_ = NeedMoreDataInitialHeader;
    return;
  }
  MetadataExchangeInitialHeader initial_header;
  data.copyOut(0, initial_header_length, &initial_header);
  if (absl::gntohl(initial_header.magic) != MetadataExchangeInitialHeader::magic_number) {
    config_->stats().initial_header_not_found_.inc();
    setMetadataNotFoundFilterState();
    ENVOY_LOG(warn, "Incorrect istio-peer-exchange ALPN magic. Peer missing TCP "
                    "MetadataExchange filter.");
    conn_state_ = Invalid;
    return;
  }
  proxy_data_length_ = absl::gntohl(initial_header.data_size);
  // Drain the initial header length bytes read.
  data.drain(initial_header_length);
  conn_state_ = ReadingProxyHeader;
}

void MetadataExchangeFilter::tryReadProxyData(Buffer::Instance& data) {
  if (conn_state_ != ReadingProxyHeader && conn_state_ != NeedMoreDataProxyHeader) {
    return;
  }
  if (data.length() < proxy_data_length_) {
    // Not enough data to read. Wait for it to come.
    ENVOY_LOG(debug, "Alpn Protocol matched. Waiting to read more metadata.");
    conn_state_ = NeedMoreDataProxyHeader;
    return;
  }
  std::string proxy_data_buf =
      std::string(static_cast<const char*>(data.linearize(proxy_data_length_)), proxy_data_length_);
  Envoy::ProtobufWkt::Any proxy_data;
  if (!proxy_data.ParseFromString(proxy_data_buf)) {
    config_->stats().header_not_found_.inc();
    setMetadataNotFoundFilterState();
    ENVOY_LOG(warn, "Alpn protocol matched. Magic matched. Metadata Not found.");
    conn_state_ = Invalid;
    return;
  }
  data.drain(proxy_data_length_);

  // Set Metadata
  Envoy::ProtobufWkt::Struct value_struct =
      Envoy::MessageUtil::anyConvert<Envoy::ProtobufWkt::Struct>(proxy_data);
  auto key_metadata_it = value_struct.fields().find(ExchangeMetadataHeader);
  if (key_metadata_it != value_struct.fields().end()) {
    updatePeer(key_metadata_it->second.struct_value());
  }
  const auto key_metadata_id_it = value_struct.fields().find(ExchangeMetadataHeaderId);
  if (key_metadata_id_it != value_struct.fields().end()) {
    Envoy::ProtobufWkt::Value val = key_metadata_id_it->second;
    updatePeerId(toAbslStringView(config_->filter_direction_ == FilterDirection::Downstream
                                      ? ::Wasm::Common::kDownstreamMetadataIdKey
                                      : ::Wasm::Common::kUpstreamMetadataIdKey),
                 val.string_value());
  }
}

void MetadataExchangeFilter::updatePeer(const Envoy::ProtobufWkt::Struct& struct_value) {
  const auto fb = ::Wasm::Common::extractNodeFlatBufferFromStruct(struct_value);

  // Filter object captures schema by view, hence the global singleton for the
  // prototype.
  auto state = std::make_unique<::Envoy::Extensions::Filters::Common::Expr::CelState>(
      MetadataExchangeConfig::nodeInfoPrototype());
  state->setValue(absl::string_view(reinterpret_cast<const char*>(fb.data()), fb.size()));

  auto key = config_->filter_direction_ == FilterDirection::Downstream
                 ? ::Wasm::Common::kDownstreamMetadataKey
                 : ::Wasm::Common::kUpstreamMetadataKey;
  read_callbacks_->connection().streamInfo().filterState()->setData(
      absl::StrCat("wasm.", toAbslStringView(key)), std::move(state),
      StreamInfo::FilterState::StateType::Mutable, StreamInfo::FilterState::LifeSpan::Connection);
}

void MetadataExchangeFilter::updatePeerId(absl::string_view key, absl::string_view value) {
  CelStatePrototype prototype(
      /* read_only = */ false, ::Envoy::Extensions::Filters::Common::Expr::CelStateType::String,
      absl::string_view(), StreamInfo::FilterState::LifeSpan::Connection);
  auto state = std::make_unique<::Envoy::Extensions::Filters::Common::Expr::CelState>(prototype);
  state->setValue(value);
  read_callbacks_->connection().streamInfo().filterState()->setData(
      absl::StrCat("wasm.", key), std::move(state), StreamInfo::FilterState::StateType::Mutable,
      prototype.life_span_);
}

void MetadataExchangeFilter::getMetadata(google::protobuf::Struct* metadata) {
  if (local_info_.node().has_metadata()) {
    const auto fb = ::Wasm::Common::extractNodeFlatBufferFromStruct(local_info_.node().metadata());
    ::Wasm::Common::extractStructFromNodeFlatBuffer(
        *flatbuffers::GetRoot<::Wasm::Common::FlatNode>(fb.data()), metadata);
  }
}

std::string MetadataExchangeFilter::getMetadataId() { return local_info_.node().id(); }

void MetadataExchangeFilter::setMetadataNotFoundFilterState() {
  updatePeerId(::Wasm::Common::kMetadataNotFoundValue, ::Wasm::Common::kMetadataNotFoundValue);
}

} // namespace MetadataExchange
} // namespace Tcp
} // namespace Envoy
