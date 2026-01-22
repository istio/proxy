/* Copyright 2026 Istio Authors. All Rights Reserved.
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

/**
 * PeerMetadata network and upstream network filters are used in one of ambient
 * peer metadata discovery mechanims. The peer metadata discovery mechanism
 * these filters are part of relies on peer reporting their own metadata in
 * HBONE CONNECT request and response headers.
 *
 * The purpose of these filters is to extract this metadata from the request/
 * response headers and propagate it to the Istio filters reporting telemetry
 * where this metadata will be used as labels.
 *
 * The filters in this folder are specifically concerned with extracting and
 * propagating upstream peer metadata. The working setup includes a combination
 * of several filters that together get the job done.
 *
 * A bit of background, here is a very simplified description of how Istio
 * waypoint processes a request:
 *
 * 1. connect_terminate listener recieves an incoming HBONE connection;
 *    * it uwraps HBONE tunnel and extracts the data passed inside it;
 *    * it passes the data inside the HBONE tunnel to a main_internal listener
 *      that performs the next stage of processing;
 * 2. main_internal listener is responsible for parsing the data as L7 data
 *    (HTTP/gRPC), applying configured L7 policies, picking the endpoint to
 *    route the request to and reports L7 stats
 *    * At this level we are processing the incoming request at L7 level and
 *      have access to things like status of the request and can report
 *      meaningful metrics;
 *    * To report in metrics where the request came from and where it went
 *      after we need to know the details of downstream and upstream peers -
 *      that's what we call peer metadata;
 *    * Once we've done with L7 processing of the request, we pass the request
 *      to the connect_originate (or inner_connect_originate in case of double
 *      HBONE) listener that will handle the next stage of processing;
 * 3. connect_originate - is responsible for wrapping processed L7 traffic into
 *    an HBONE tunnel and sending it out
 *    * This stage of processing treats data as a stream of bytes without any
 *      knowledge of L7 protocol details;
 *    * It takes the upstream peer address as input an establishes an HBONE
 *      tunnel to the destination and sends the data via that tunnel.
 *
 * With that picture in mind, what we want to do is in connect_originate (or
 * inner_connect_originate in case of double-HBONE) when we establish HBONE
 * tunnel, we want to extract peer metadata from the CONNECT response and
 * propagate it to the main_internal.
 *
 * To establish HBONE tunnel we rely on Envoy TCP Proxy filter, so we don't
 * handle HTTP2 CONNECT responses or requests directly, instead we rely on the
 * TCP Proxy filter to extract required information from the response and save
 * it in the filter state. We then use the custom network filter to take filter
 * state proved by TCP Proxy filter, encode it, and send it to main_internal
 * *as data* before any actual response data. This is what the network filter
 * defined here is responsible for.
 *
 * In main_internal we use a custom upstream network filter to extract and
 * remove the metadata from the data stream and populate filter state that
 * could be used by Istio telemetry filters. That's what the upstream network
 * filter defined here is responsible for.
 *
 * Why do we do it this way? Generally in Envoy we use filter state and dynamic
 * metadata to communicate additional information between filters. While it's
 * possible to propagate filter state from downstream to upstream, i.e., we
 * could set filter state in connect_terminate and propagate it to
 * main_internal and then to connect_originate, it's not possible to propagate
 * filter state from upstream to downstream, i.e., we cannot make filter state
 * set in connect_originate available to main_internal directly. That's why we
 * push that metadata with the data instead.
 */

#include <optional>

#include "envoy/network/filter.h"
#include "envoy/server/filter_config.h"
#include "extensions/common/metadata_object.h"
#include "source/common/common/logger.h"
#include "source/common/router/string_accessor_impl.h"
#include "source/common/singleton/const_singleton.h"
#include "source/common/stream_info/bool_accessor_impl.h"
#include "source/common/tcp_proxy/tcp_proxy.h"
#include "source/extensions/filters/common/expr/cel_state.h"
#include "source/extensions/filters/network/common/factory_base.h"
#include "source/extensions/filters/network/peer_metadata/proto/peer_metadata.pb.h"
#include "source/extensions/filters/network/peer_metadata/proto/peer_metadata.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace PeerMetadata {

namespace {

using Config = ::envoy::extensions::network_filters::peer_metadata::proto::Config;
using UpstreamConfig = ::envoy::extensions::network_filters::peer_metadata::proto::UpstreamConfig;

using CelState = ::Envoy::Extensions::Filters::Common::Expr::CelState;
using CelStatePrototype = ::Envoy::Extensions::Filters::Common::Expr::CelStatePrototype;
using CelStateType = ::Envoy::Extensions::Filters::Common::Expr::CelStateType;

PACKED_STRUCT(struct PeerMetadataHeader {
  uint32_t magic;
  static const uint32_t magic_number = 0xabcd1234;
  uint32_t data_size;
});

struct HeaderValues {
  const Http::LowerCaseString Baggage{"baggage"};
};

using Headers = ConstSingleton<HeaderValues>;

enum class PeerMetadataState {
  WaitingForData,
  PassThrough,
};

std::string baggageValue(const Server::Configuration::ServerFactoryContext& context) {
  const auto obj =
      ::Istio::Common::convertStructToWorkloadMetadata(context.localInfo().node().metadata());
  return obj->baggage();
}

/**
 * This is a regular network filter that will be installed in the
 * connect_originate or inner_connect_originate filter chains. It will take
 * baggage header information from filter state (we expect TCP Proxy to
 * populate it), collect other details that are missing from the baggage, i.e.
 * the upstream peer principle, encode those details into a sequence of bytes
 * and will inject it dowstream.
 */
class Filter : public Network::Filter, Logger::Loggable<Logger::Id::filter> {
public:
  Filter(const Config& config, Server::Configuration::ServerFactoryContext& context)
      : config_(config), baggage_(baggageValue(context)) {}

  // Network::ReadFilter
  Network::FilterStatus onData(Buffer::Instance&, bool) override {
    return Network::FilterStatus::Continue;
  }

  Network::FilterStatus onNewConnection() override {
    ENVOY_LOG(trace, "New connection from downstream");
    populateBaggage();
    return Network::FilterStatus::Continue;
  }

  void initializeReadFilterCallbacks(Network::ReadFilterCallbacks& callbacks) override {
    read_callbacks_ = &callbacks;
  }

  // Network::WriteFilter
  Network::FilterStatus onWrite(Buffer::Instance& buffer, bool) override {
    ENVOY_LOG(trace, "Writing {} bytes to the downstream connection", buffer.length());
    switch (state_) {
    case PeerMetadataState::WaitingForData: {
      // If we are receiving data for downstream - there is no point in waiting
      // for peer metadata anymore, if the upstream sent it, we'd have it by
      // now. So we can check if the peer metadata is available or not, and if
      // no peer metadata available, we can give up waiting for it.
      std::optional<google::protobuf::Any> peer_metadata = discoverPeerMetadata();
      if (peer_metadata) {
        propagatePeerMetadata(*peer_metadata);
      } else {
        propagateNoPeerMetadata();
      }
      state_ = PeerMetadataState::PassThrough;
      break;
    }
    default:
      break;
    }
    return Network::FilterStatus::Continue;
  }

  void initializeWriteFilterCallbacks(Network::WriteFilterCallbacks& callbacks) override {
    write_callbacks_ = &callbacks;
  }

private:
  void populateBaggage() {
    if (config_.baggage_key().empty()) {
      ENVOY_LOG(trace, "Not populating baggage filter state because baggage key is not set");
      return;
    }

    ENVOY_LOG(trace, "Populating baggage value {} in the filter state with key {}", baggage_,
              config_.baggage_key());
    ASSERT(read_callbacks_);
    read_callbacks_->connection().streamInfo().filterState()->setData(
        config_.baggage_key(), std::make_shared<Router::StringAccessorImpl>(baggage_),
        StreamInfo::FilterState::StateType::ReadOnly,
        StreamInfo::FilterState::LifeSpan::FilterChain);
  }

  // discoveryPeerMetadata is called to check if the baggage HTTP2 CONNECT
  // response headers have been populated already in the filter state.
  //
  // NOTE: It's safe to call this function during any step of processing - it
  // will not do anything if the filter is not in the right state.
  std::optional<google::protobuf::Any> discoverPeerMetadata() {
    ENVOY_LOG(trace, "Trying to discovery peer metadata from filter state set by TCP Proxy");
    ASSERT(write_callbacks_);

    const Network::Connection& conn = write_callbacks_->connection();
    const StreamInfo::StreamInfo& stream = conn.streamInfo();
    const TcpProxy::TunnelResponseHeaders* state =
        stream.filterState().getDataReadOnly<TcpProxy::TunnelResponseHeaders>(
            TcpProxy::TunnelResponseHeaders::key());
    if (!state) {
      ENVOY_LOG(trace, "TCP Proxy didn't set expected filter state");
      return std::nullopt;
    }

    const Http::HeaderMap& headers = state->value();
    const auto baggage = headers.get(Headers::get().Baggage);
    if (baggage.empty()) {
      ENVOY_LOG(
          trace,
          "TCP Proxy saved response headers to the filter state, but there is no baggage header");
      return std::nullopt;
    }

    ENVOY_LOG(trace,
              "Successfully discovered peer metadata from the baggage header saved by TCP Proxy");

    std::string identity{};
    const auto upstream = write_callbacks_->connection().streamInfo().upstreamInfo();
    if (upstream) {
      const auto conn = upstream->upstreamSslConnection();
      if (conn) {
        identity = absl::StrJoin(conn->uriSanPeerCertificate(), ",");
        ENVOY_LOG(trace, "Discovered upstream peer identity to be {}", identity);
      }
    }

    std::unique_ptr<::Istio::Common::WorkloadMetadataObject> metadata =
        ::Istio::Common::convertBaggageToWorkloadMetadata(baggage[0]->value().getStringView(),
                                                          identity);

    google::protobuf::Struct data = convertWorkloadMetadataToStruct(*metadata);
    google::protobuf::Any wrapped;
    wrapped.PackFrom(data);
    return wrapped;
  }

  void propagatePeerMetadata(const google::protobuf::Any& peer_metadata) {
    ENVOY_LOG(trace, "Sending peer metadata downstream with the data stream");
    ASSERT(write_callbacks_);

    if (state_ != PeerMetadataState::WaitingForData) {
      // It's only safe and correct to send the peer metadata downstream with
      // the data if we haven't done that already, otherwise the downstream
      // could be very confused by the data they received.
      ENVOY_LOG(trace, "Filter has already sent the peer metadat downstream");
      return;
    }

    std::string data = peer_metadata.SerializeAsString();
    PeerMetadataHeader header{PeerMetadataHeader::magic_number, static_cast<uint32_t>(data.size())};

    Buffer::OwnedImpl buffer{
        std::string_view(reinterpret_cast<const char*>(&header), sizeof(header))};
    buffer.add(data);
    write_callbacks_->injectWriteDataToFilterChain(buffer, false);
  }

  void propagateNoPeerMetadata() {
    ENVOY_LOG(trace, "Sending no peer metadata downstream with the data");
    ASSERT(write_callbacks_);

    PeerMetadataHeader header{PeerMetadataHeader::magic_number, 0};
    Buffer::OwnedImpl buffer{
        std::string_view(reinterpret_cast<const char*>(&header), sizeof(header))};
    write_callbacks_->injectWriteDataToFilterChain(buffer, false);
  }

  PeerMetadataState state_ = PeerMetadataState::WaitingForData;
  Network::WriteFilterCallbacks* write_callbacks_{};
  Network::ReadFilterCallbacks* read_callbacks_{};
  const Config& config_;
  std::string baggage_;
};

/**
 * This is an upstream network filter complementing the filter above. It will
 * be installed in all the service clusters that may use HBONE (or double
 * HBONE) to communicate with the upstream peers and it will parse and remove
 * the data injected by the filter above. The parsed peer metadata details will
 * be saved in the filter state.
 *
 * NOTE: This filter has built-in safety checks that would prevent it from
 * trying to interpret the actual connection data as peer metadata injected
 * by the filter above. However, those checks are rather shallow and rely on a
 * bunch of implicit assumptions (i.e., the magic number does not match
 * accidentally, the upstream host actually sends back some data that we can
 * check, etc). What I'm trying to say is that in correct setup we don't need
 * to rely on those checks for correctness and if it's not the case, then we
 * definitely have a bug.
 */
class UpstreamFilter : public Network::ReadFilter, Logger::Loggable<Logger::Id::filter> {
public:
  UpstreamFilter() {}

  // Network::ReadFilter
  Network::FilterStatus onData(Buffer::Instance& buffer, bool end_stream) override {
    ENVOY_LOG(trace, "Read {} bytes from the upstream connection", buffer.length());

    switch (state_) {
    case PeerMetadataState::WaitingForData:
      if (!isUpstreamHBONE()) {
        state_ = PeerMetadataState::PassThrough;
        break;
      }
      if (consumePeerMetadata(buffer, end_stream)) {
        state_ = PeerMetadataState::PassThrough;
      } else {
        // If we got here it means that we are waiting for more data to arrive.
        // NOTE: if error happened, we will not get here, consumePeerMetadata
        // will just return true and we will enter PassThrough state.
        return Network::FilterStatus::StopIteration;
      }
      break;
    default:
      break;
    }

    return Network::FilterStatus::Continue;
  }

  Network::FilterStatus onNewConnection() override { return Network::FilterStatus::Continue; }

  void initializeReadFilterCallbacks(Network::ReadFilterCallbacks& callbacks) override {
    callbacks_ = &callbacks;
  }

private:
  // TODO: This is a rather shallow check - it only verifies that the upstream is an internal
  // listener and therefore could have peer metadata filter that will send peer metadata with
  // the data stream.
  //
  // We can be more explicit than that and check the name of the internal listener to only
  // trigger the logic when we talk to connect_originate or inner_connect_originate listeners.
  // A more clean approach would be to add endpoint metadata that will let this filter know
  // that it should not trigger for the connection (or should trigger on the connection).
  //
  // Another potential benefit here is that we can trigger baggage-based peer metadata
  // discovery only for double-HBONE connections, if we let this filter skip all regular
  // endpoints that don't communicate with the E/W gateway.
  //
  // We could also consider dropping this check alltogether, because we only need this filter
  // in waypoints and all waypoint clusters contain either HBONE or double-HBONE endpoints.
  bool isUpstreamHBONE() const {
    ASSERT(callbacks_);

    const auto upstream = callbacks_->connection().streamInfo().upstreamInfo();
    if (!upstream) {
      ENVOY_LOG(trace, "No upstream information, cannot confirm that upstream uses HBONE");
      return false;
    }

    const auto host = upstream->upstreamHost();
    if (!host) {
      ENVOY_LOG(trace, "No upstream host, cannot confirm that upstream host uses HBONE");
      return false;
    }

    if (host->address()->type() != Network::Address::Type::EnvoyInternal) {
      ENVOY_LOG(trace,
                "Upstream host is not an internal listener - upstream host does not use HBONE");
      return false;
    }

    ENVOY_LOG(trace,
              "Upstream host is an internal listener - concluding that upstream host uses HBONE");
    return true;
  }

  bool consumePeerMetadata(Buffer::Instance& buffer, bool end_stream) {
    ENVOY_LOG(trace, "Trying to consume peer metadata from the data stream");
    using namespace ::Istio::Common;

    ASSERT(callbacks_);

    if (state_ != PeerMetadataState::WaitingForData) {
      ENVOY_LOG(trace, "The filter already consumed peer metadata from the data stream");
      return true;
    }

    if (buffer.length() < sizeof(PeerMetadataHeader)) {
      if (end_stream) {
        ENVOY_LOG(trace, "Not enough data in the data stream for peer metadata header and no more "
                         "data is coming");
        populateNoPeerMetadata();
        return true;
      }
      ENVOY_LOG(
          trace,
          "Not enough data in the data stream for peer metadata header, waiting for more data");
      return false;
    }

    PeerMetadataHeader header;
    buffer.copyOut(0, sizeof(PeerMetadataHeader), &header);

    if (header.magic != PeerMetadataHeader::magic_number) {
      ENVOY_LOG(trace, "Magic number in the peer metadata header didn't match expected value");
      populateNoPeerMetadata();
      return true;
    }

    if (header.data_size == 0) {
      ENVOY_LOG(trace, "Peer metadata is empty");
      populateNoPeerMetadata();
      buffer.drain(sizeof(PeerMetadataHeader));
      return true;
    }

    const size_t peer_metadata_size = sizeof(PeerMetadataHeader) + header.data_size;

    if (buffer.length() < peer_metadata_size) {
      if (end_stream) {
        ENVOY_LOG(
            trace,
            "Not enough data in the data stream for peer metadata and no more data is coming");
        populateNoPeerMetadata();
        return true;
      }
      ENVOY_LOG(trace,
                "Not enough data in the data stream for peer metadata, waiting for more data");
      return false;
    }

    std::string_view data{static_cast<const char*>(buffer.linearize(peer_metadata_size)),
                          peer_metadata_size};
    data = data.substr(sizeof(PeerMetadataHeader));
    google::protobuf::Any any;
    if (!any.ParseFromArray(data.data(), data.size())) {
      ENVOY_LOG(trace, "Failed to parse peer metadata proto from the data stream");
      populateNoPeerMetadata();
      return true;
    }

    google::protobuf::Struct peer_metadata;
    if (!any.UnpackTo(&peer_metadata)) {
      ENVOY_LOG(trace, "Failed to unpack peer metadata struct");
      populateNoPeerMetadata();
      return true;
    }

    std::unique_ptr<WorkloadMetadataObject> workload =
        convertStructToWorkloadMetadata(peer_metadata);
    populatePeerMetadata(*workload);
    buffer.drain(peer_metadata_size);
    ENVOY_LOG(trace, "Successfully consumed peer metadata from the data stream");
    return true;
  }

  static const CelStatePrototype& peerInfoPrototype() {
    static const CelStatePrototype* const prototype = new CelStatePrototype(
        true, CelStateType::Protobuf, "type.googleapis.com/google.protobuf.Struct",
        StreamInfo::FilterState::LifeSpan::Connection);
    return *prototype;
  }

  void populatePeerMetadata(const ::Istio::Common::WorkloadMetadataObject& peer) {
    ENVOY_LOG(trace, "Populating peer metadata in the upstream filter state");
    ASSERT(callbacks_);

    auto proto = ::Istio::Common::convertWorkloadMetadataToStruct(peer);
    auto cel = std::make_shared<CelState>(peerInfoPrototype());
    cel->setValue(std::string_view(proto.SerializeAsString()));
    callbacks_->connection().streamInfo().filterState()->setData(
        ::Istio::Common::UpstreamPeer, std::move(cel), StreamInfo::FilterState::StateType::ReadOnly,
        StreamInfo::FilterState::LifeSpan::Connection);
  }

  void populateNoPeerMetadata() {
    ENVOY_LOG(trace, "Populating no peer metadata in the upstream filter state");
    ASSERT(callbacks_);

    callbacks_->connection().streamInfo().filterState()->setData(
        ::Istio::Common::NoPeer, std::make_shared<StreamInfo::BoolAccessorImpl>(true),
        StreamInfo::FilterState::StateType::ReadOnly,
        StreamInfo::FilterState::LifeSpan::Connection);
  }

  PeerMetadataState state_ = PeerMetadataState::WaitingForData;
  Network::ReadFilterCallbacks* callbacks_{};
};

/**
 * PeerMetadata network filter factory.
 *
 * This filter is responsible for collecting peer metadata from filter state
 * and other sources, encoding it and passing it downstream before the actual
 * data.
 */
class ConfigFactory : public Common::ExceptionFreeFactoryBase<Config> {
public:
  ConfigFactory()
      : Common::ExceptionFreeFactoryBase<Config>("envoy.filters.network.peer_metadata",
                                                 /*is_termnial*/ false) {}

private:
  absl::StatusOr<Network::FilterFactoryCb>
  createFilterFactoryFromProtoTyped(const Config& config,
                                    Server::Configuration::FactoryContext& context) override {
    return [config, &context](Network::FilterManager& filter_manager) -> void {
      filter_manager.addFilter(std::make_shared<Filter>(config, context.serverFactoryContext()));
    };
  }
};

/**
 * PeerMetadata upstream network filter factory.
 *
 * This filter is responsible for detecting the peer metadata passed in the
 * data stream, parsing it, populating filter state based on that and finally
 * removing it from the data stream, so that downstream filters can process
 * the data as usual.
 */
class UpstreamConfigFactory
    : public Server::Configuration::NamedUpstreamNetworkFilterConfigFactory {
public:
  Network::FilterFactoryCb
  createFilterFactoryFromProto(const Protobuf::Message& config,
                               Server::Configuration::UpstreamFactoryContext&) override {
    return createFilterFactory(dynamic_cast<const UpstreamConfig&>(config));
  }

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<UpstreamConfig>();
  }

  std::string name() const override { return "envoy.filters.network.upstream.peer_metadata"; }

  bool isTerminalFilterByProto(const Protobuf::Message&,
                               Server::Configuration::ServerFactoryContext&) override {
    // This filter must be last filter in the upstream filter chain, so that
    // it'd be the first filter to see and process the data coming back,
    // because it has to remove the preamble set by the network filter.
    return true;
  }

private:
  Network::FilterFactoryCb createFilterFactory(const UpstreamConfig&) {
    return [](Network::FilterManager& filter_manager) -> void {
      filter_manager.addReadFilter(std::make_shared<UpstreamFilter>());
    };
  }
};

REGISTER_FACTORY(ConfigFactory, Server::Configuration::NamedNetworkFilterConfigFactory);
REGISTER_FACTORY(UpstreamConfigFactory,
                 Server::Configuration::NamedUpstreamNetworkFilterConfigFactory);

} // namespace

} // namespace PeerMetadata
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
