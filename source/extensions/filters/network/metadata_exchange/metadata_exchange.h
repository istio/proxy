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

#include <string>

#include "envoy/local_info/local_info.h"
#include "envoy/network/filter.h"
#include "envoy/runtime/runtime.h"
#include "envoy/stats/scope.h"
#include "envoy/stats/stats_macros.h"
#include "envoy/stream_info/filter_state.h"
#include "extensions/common/context.h"
#include "extensions/common/node_info_bfbs_generated.h"
#include "extensions/common/proto_util.h"
#include "source/common/common/stl_helpers.h"
#include "source/common/protobuf/protobuf.h"
#include "source/extensions/filters/common/expr/cel_state.h"
#include "source/extensions/filters/network/metadata_exchange/config/metadata_exchange.pb.h"
#include "source/extensions/common/workload_discovery/api.h"

namespace Envoy {
namespace Tcp {
namespace MetadataExchange {

using ::Envoy::Extensions::Filters::Common::Expr::CelStatePrototype;

/**
 * All MetadataExchange filter stats. @see stats_macros.h
 */
#define ALL_METADATA_EXCHANGE_STATS(COUNTER)                                                       \
  COUNTER(alpn_protocol_not_found)                                                                 \
  COUNTER(alpn_protocol_found)                                                                     \
  COUNTER(initial_header_not_found)                                                                \
  COUNTER(header_not_found)                                                                        \
  COUNTER(metadata_added)

/**
 * Struct definition for all MetadataExchange stats. @see stats_macros.h
 */
struct MetadataExchangeStats {
  ALL_METADATA_EXCHANGE_STATS(GENERATE_COUNTER_STRUCT)
};

/**
 * Direction of the flow of traffic in which this this MetadataExchange filter
 * is placed.
 */
enum class FilterDirection { Downstream, Upstream };

/**
 * Configuration for the MetadataExchange filter.
 */
class MetadataExchangeConfig {
public:
  MetadataExchangeConfig(const std::string& stat_prefix, const std::string& protocol,
                         const FilterDirection filter_direction, bool enable_discovery,
                         Server::Configuration::ServerFactoryContext& factory_context,
                         Stats::Scope& scope);

  const MetadataExchangeStats& stats() { return stats_; }

  // Scope for the stats.
  Stats::Scope& scope_;
  // Stat prefix.
  const std::string stat_prefix_;
  // Expected Alpn Protocol.
  const std::string protocol_;
  // Direction of filter.
  const FilterDirection filter_direction_;
  // Set if WDS is enabled.
  Extensions::Common::WorkloadDiscovery::WorkloadMetadataProviderSharedPtr metadata_provider_;
  // Stats for MetadataExchange Filter.
  MetadataExchangeStats stats_;

  static const CelStatePrototype& nodeInfoPrototype() {
    static const CelStatePrototype* const prototype = new CelStatePrototype(
        true, ::Envoy::Extensions::Filters::Common::Expr::CelStateType::FlatBuffers,
        ::Wasm::Common::nodeInfoSchema(), StreamInfo::FilterState::LifeSpan::Connection);
    return *prototype;
  }

private:
  MetadataExchangeStats generateStats(const std::string& prefix, Stats::Scope& scope) {
    return MetadataExchangeStats{ALL_METADATA_EXCHANGE_STATS(POOL_COUNTER_PREFIX(scope, prefix))};
  }
};

using MetadataExchangeConfigSharedPtr = std::shared_ptr<MetadataExchangeConfig>;

/**
 * A MetadataExchange filter instance. One per connection.
 */
class MetadataExchangeFilter : public Network::Filter,
                               protected Logger::Loggable<Logger::Id::filter> {
public:
  MetadataExchangeFilter(MetadataExchangeConfigSharedPtr config,
                         const LocalInfo::LocalInfo& local_info)
      : config_(config), local_info_(local_info), conn_state_(ConnProtocolNotRead) {}

  // Network::ReadFilter
  Network::FilterStatus onData(Buffer::Instance& data, bool end_stream) override;
  Network::FilterStatus onNewConnection() override;
  Network::FilterStatus onWrite(Buffer::Instance& data, bool end_stream) override;
  void initializeReadFilterCallbacks(Network::ReadFilterCallbacks& callbacks) override {
    read_callbacks_ = &callbacks;
    // read_callbacks_->connection().addConnectionCallbacks(*this);
  }
  void initializeWriteFilterCallbacks(Network::WriteFilterCallbacks& callbacks) override {
    write_callbacks_ = &callbacks;
  }

private:
  // Writes node metadata in write pipeline of the filter chain.
  // Also, sets node metadata in Dynamic Metadata to be available for subsequent
  // filters.
  void writeNodeMetadata();

  // Tries to read inital proxy header in the data bytes.
  void tryReadInitialProxyHeader(Buffer::Instance& data);

  // Tries to read data after initial proxy header. This is currently in the
  // form of google::protobuf::any which encapsulates google::protobuf::struct.
  void tryReadProxyData(Buffer::Instance& data);

  // Helper function to share the metadata with other filters.
  void updatePeer(const std::string& fb);
  void updatePeerId(absl::string_view key, absl::string_view value);

  // Helper function to get Dynamic metadata.
  void getMetadata(google::protobuf::Struct* metadata);

  // Helper function to get metadata id.
  std::string getMetadataId();

  // Helper function to set filterstate when no client mxc found.
  void setMetadataNotFoundFilterState();

  // Config for MetadataExchange filter.
  MetadataExchangeConfigSharedPtr config_;
  // LocalInfo instance.
  const LocalInfo::LocalInfo& local_info_;
  // Read callback instance.
  Network::ReadFilterCallbacks* read_callbacks_{};
  // Write callback instance.
  Network::WriteFilterCallbacks* write_callbacks_{};
  // Stores the length of proxy data that contains node metadata.
  uint64_t proxy_data_length_{0};

  const std::string ExchangeMetadataHeader = "x-envoy-peer-metadata";
  const std::string ExchangeMetadataHeaderId = "x-envoy-peer-metadata-id";

  // Type url of google::protobug::struct.
  const std::string StructTypeUrl = "type.googleapis.com/google.protobuf.Struct";

  // Captures the state machine of what is going on in the filter.
  enum {
    ConnProtocolNotRead,       // Connection Protocol has not been read yet
    WriteMetadata,             // Write node metadata
    ReadingInitialHeader,      // MetadataExchangeInitialHeader is being read
    ReadingProxyHeader,        // Proxy Header is being read
    NeedMoreDataInitialHeader, // Need more data to be read
    NeedMoreDataProxyHeader,   // Need more data to be read
    Done,                      // Alpn Protocol Found and all the read is done
    Invalid,                   // Invalid state, all operations fail
  } conn_state_;
};

} // namespace MetadataExchange
} // namespace Tcp
} // namespace Envoy
