// Copyright Istio Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "source/extensions/filters/http/istio_stats/istio_stats.h"

#include "envoy/registry/registry.h"
#include "envoy/server/factory_context.h"
#include "envoy/singleton/manager.h"
#include "extensions/common/metadata_object.h"
#include "source/common/grpc/common.h"
#include "source/common/http/header_map_impl.h"
#include "source/common/http/header_utility.h"
#include "source/common/stream_info/utility.h"
#include "source/extensions/filters/common/expr/cel_state.h"
#include "source/extensions/filters/http/common/pass_through_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace IstioStats {

namespace {
constexpr absl::string_view CustomStatNamespace = "istiocustom";

absl::string_view extractString(const ProtobufWkt::Struct& metadata,
                                const std::string& key) {
  const auto& it = metadata.fields().find(key);
  if (it == metadata.fields().end()) {
    return {};
  }
  return it->second.string_value();
}

absl::string_view extractMapString(const ProtobufWkt::Struct& metadata,
                                   const std::string& map_key,
                                   const std::string& key) {
  const auto& it = metadata.fields().find(map_key);
  if (it == metadata.fields().end()) {
    return {};
  }
  return extractString(it->second.struct_value(), key);
}

// Process-wide context shared with all filter instances.
struct Context : public Singleton::Instance {
  explicit Context(Stats::SymbolTable& symbol_table,
                   const envoy::config::core::v3::Node& node)
      : pool_(symbol_table),
        stat_namespace_(pool_.add(CustomStatNamespace)),
        requests_total_(pool_.add("istio_requests_total")),
        request_duration_milliseconds_(
            pool_.add("istio_request_duration_milliseconds")),
        request_bytes_(pool_.add("istio_request_bytes")),
        response_bytes_(pool_.add("istio_response_bytes")),
        tcp_connections_opened_total_(
            pool_.add("istio_tcp_connections_opened_total")),
        tcp_connections_closed_total_(
            pool_.add("istio_tcp_connections_closed_total")),
        tcp_sent_bytes_total_(pool_.add("istio_tcp_sent_bytes_total")),
        tcp_received_bytes_total_(pool_.add("istio_tcp_received_bytes_total")),
        empty_(pool_.add("")),
        unknown_(pool_.add("unknown")),
        source_(pool_.add("source")),
        destination_(pool_.add("destination")),
        latest_(pool_.add("latest")),
        http_(pool_.add("http")),
        grpc_(pool_.add("grpc")),
        tcp_(pool_.add("tcp")),
        mutual_tls_(pool_.add("mutual_tls")),
        none_(pool_.add("none")),
        reporter_(pool_.add("reporter")),
        source_workload_(pool_.add("source_workload")),
        source_workload_namespace_(pool_.add("source_workload_namespace")),
        source_principal_(pool_.add("source_principal")),
        source_app_(pool_.add("source_app")),
        source_version_(pool_.add("source_version")),
        source_canonical_service_(pool_.add("source_canonical_service")),
        source_canonical_revision_(pool_.add("source_canonical_revision")),
        source_cluster_(pool_.add("source_cluster")),
        destination_workload_(pool_.add("destination_workload")),
        destination_workload_namespace_(
            pool_.add("destination_workload_namespace")),
        destination_principal_(pool_.add("destination_principal")),
        destination_app_(pool_.add("destination_app")),
        destination_version_(pool_.add("destination_version")),
        destination_service_(pool_.add("destination_service")),
        destination_service_name_(pool_.add("destination_service_name")),
        destination_service_namespace_(
            pool_.add("destination_service_namespace")),
        destination_canonical_service_(
            pool_.add("destination_canonical_service")),
        destination_canonical_revision_(
            pool_.add("destination_canonical_revision")),
        destination_cluster_(pool_.add("destination_cluster")),
        request_protocol_(pool_.add("request_protocol")),
        response_flags_(pool_.add("response_flags")),
        connection_security_policy_(pool_.add("connection_security_policy")),
        response_code_(pool_.add("response_code")),
        grpc_response_status_(pool_.add("grpc_response_status")),
        workload_name_(
            pool_.add(extractString(node.metadata(), "WORKLOAD_NAME"))),
        namespace_(pool_.add(extractString(node.metadata(), "NAMESPACE"))),
        canonical_name_(pool_.add(extractMapString(
            node.metadata(), "LABELS", "service.istio.io/canonical-name"))),
        canonical_revision_(pool_.add(extractMapString(
            node.metadata(), "LABELS", "service.istio.io/canonical-revision"))),
        cluster_name_(pool_.add(extractString(node.metadata(), "CLUSTER_ID"))),
        app_name_(
            pool_.add(extractMapString(node.metadata(), "LABELS", "app"))),
        app_version_(
            pool_.add(extractMapString(node.metadata(), "LABELS", "version"))) {
  }

  Stats::StatNamePool pool_;

  // Metric names.
  const Stats::StatName stat_namespace_;
  const Stats::StatName requests_total_;
  const Stats::StatName request_duration_milliseconds_;
  const Stats::StatName request_bytes_;
  const Stats::StatName response_bytes_;
  const Stats::StatName tcp_connections_opened_total_;
  const Stats::StatName tcp_connections_closed_total_;
  const Stats::StatName tcp_sent_bytes_total_;
  const Stats::StatName tcp_received_bytes_total_;

  // Constant names.
  const Stats::StatName empty_;
  const Stats::StatName unknown_;
  const Stats::StatName source_;
  const Stats::StatName destination_;
  const Stats::StatName latest_;
  const Stats::StatName http_;
  const Stats::StatName grpc_;
  const Stats::StatName tcp_;
  const Stats::StatName mutual_tls_;
  const Stats::StatName none_;

  // Dimension names.
  const Stats::StatName reporter_;

  const Stats::StatName source_workload_;
  const Stats::StatName source_workload_namespace_;
  const Stats::StatName source_principal_;
  const Stats::StatName source_app_;
  const Stats::StatName source_version_;
  const Stats::StatName source_canonical_service_;
  const Stats::StatName source_canonical_revision_;
  const Stats::StatName source_cluster_;

  const Stats::StatName destination_workload_;
  const Stats::StatName destination_workload_namespace_;
  const Stats::StatName destination_principal_;
  const Stats::StatName destination_app_;
  const Stats::StatName destination_version_;
  const Stats::StatName destination_service_;
  const Stats::StatName destination_service_name_;
  const Stats::StatName destination_service_namespace_;
  const Stats::StatName destination_canonical_service_;
  const Stats::StatName destination_canonical_revision_;
  const Stats::StatName destination_cluster_;

  const Stats::StatName request_protocol_;
  const Stats::StatName response_flags_;
  const Stats::StatName connection_security_policy_;
  const Stats::StatName response_code_;
  const Stats::StatName grpc_response_status_;

  // Per-process constants.
  const Stats::StatName workload_name_;
  const Stats::StatName namespace_;
  const Stats::StatName canonical_name_;
  const Stats::StatName canonical_revision_;
  const Stats::StatName cluster_name_;
  const Stats::StatName app_name_;
  const Stats::StatName app_version_;
};
using ContextSharedPtr = std::shared_ptr<Context>;

SINGLETON_MANAGER_REGISTRATION(Context)

enum class Reporter {
  ClientSidecar,
  ServerSidecar,
};

struct Config {
  Config(const stats::PluginConfig& proto_config,
         Server::Configuration::FactoryContext& factory_context)
      : context_(factory_context.singletonManager().getTyped<Context>(
            SINGLETON_MANAGER_REGISTERED_NAME(Context),
            [&factory_context] {
              return std::make_shared<Context>(
                  factory_context.serverScope().symbolTable(),
                  factory_context.localInfo().node());
            })),
        scope_(factory_context.scope()),
        pool_(scope_.symbolTable()),
        disable_host_header_fallback_(
            proto_config.disable_host_header_fallback()),
        report_duration_(PROTOBUF_GET_MS_OR_DEFAULT(
            proto_config, tcp_reporting_duration, /* 15s */ 15000)) {
    switch (factory_context.direction()) {
      case envoy::config::core::v3::TrafficDirection::INBOUND:
        reporter_ = Reporter::ServerSidecar;
        break;
      case envoy::config::core::v3::TrafficDirection::OUTBOUND:
        reporter_ = Reporter::ClientSidecar;
        break;
      default:
        reporter_ = Reporter::ClientSidecar;
    }
  }

  Stats::StatName resolve(absl::string_view symbol) {
    const auto& it = request_names_.find(symbol);
    if (it != request_names_.end()) {
      return it->second;
    }
    Stats::StatName name = pool_.add(symbol);
    request_names_.emplace(symbol, name);
    return name;
  }

  Reporter reporter() const { return reporter_; }

  ContextSharedPtr context_;
  Stats::Scope& scope_;
  Reporter reporter_;
  // Backing storage for request strings (lock-free).
  Stats::StatNameDynamicPool pool_;
  absl::flat_hash_map<std::string, Stats::StatName> request_names_;

  const bool disable_host_header_fallback_;
  const std::chrono::milliseconds report_duration_;
};

using ConfigSharedPtr = std::shared_ptr<Config>;

class IstioStatsFilter : public Http::PassThroughFilter,
                         public Network::ReadFilter,
                         public Network::ConnectionCallbacks {
 public:
  IstioStatsFilter(ConfigSharedPtr config)
      : config_(config), context_(*config->context_) {
    tags_.reserve(25);
    switch (config_->reporter()) {
      case Reporter::ServerSidecar:
        tags_.push_back({context_.reporter_, context_.destination_});
        break;
      case Reporter::ClientSidecar:
        tags_.push_back({context_.reporter_, context_.source_});
        break;
    }
  }
  ~IstioStatsFilter() { ASSERT(report_timer_ == nullptr); }

  // Http::StreamFilter
  void onStreamComplete() override {
    const auto& info = decoder_callbacks_->streamInfo();
    populatePeerInfo(info, info.filterState());
    const auto* headers = info.getRequestHeaders();
    const bool is_grpc =
        headers && Grpc::Common::isGrpcRequestHeaders(*headers);
    if (is_grpc) {
      tags_.push_back({context_.request_protocol_, context_.grpc_});
    } else {
      tags_.push_back({context_.request_protocol_, context_.http_});
    }

    // TODO: copy Http::CodeStatsImpl version for status codes and flags.
    tags_.push_back(
        {context_.response_code_,
         config_->resolve(absl::StrCat(info.responseCode().value_or(0)))});
    if (is_grpc) {
      auto response_headers = decoder_callbacks_->responseHeaders();
      auto response_trailers = decoder_callbacks_->responseTrailers();
      auto const& optional_status = Grpc::Common::getGrpcStatus(
          response_trailers
              ? response_trailers.ref()
              : *Http::StaticEmptyHeaders::get().response_trailers,
          response_headers ? response_headers.ref()
                           : *Http::StaticEmptyHeaders::get().response_headers,
          info);
      tags_.push_back({context_.grpc_response_status_,
                       optional_status ? config_->resolve(absl::StrCat(
                                             optional_status.value()))
                                       : context_.empty_});
    } else {
      tags_.push_back({context_.grpc_response_status_, context_.empty_});
    }
    populateFlagsAndConnectionSecurity(info);

    Stats::Utility::counterFromStatNames(
        config_->scope_, {context_.stat_namespace_, context_.requests_total_},
        tags_)
        .inc();
    auto duration = info.requestComplete();
    if (duration.has_value()) {
      Stats::Utility::histogramFromStatNames(
          config_->scope_,
          {context_.stat_namespace_, context_.request_duration_milliseconds_},
          Stats::Histogram::Unit::Milliseconds, tags_)
          .recordValue(absl::FromChrono(duration.value()) /
                       absl::Milliseconds(1));
    }
    auto meter = info.getDownstreamBytesMeter();
    if (meter) {
      Stats::Utility::histogramFromStatNames(
          config_->scope_, {context_.stat_namespace_, context_.request_bytes_},
          Stats::Histogram::Unit::Bytes, tags_)
          .recordValue(meter->wireBytesReceived());
      Stats::Utility::histogramFromStatNames(
          config_->scope_, {context_.stat_namespace_, context_.response_bytes_},
          Stats::Histogram::Unit::Bytes, tags_)
          .recordValue(meter->wireBytesSent());
    }
  }

  // Network::ReadFilter
  Network::FilterStatus onData(Buffer::Instance&, bool) override {
    return Network::FilterStatus::Continue;
  }
  Network::FilterStatus onNewConnection() override {
    if (config_->report_duration_ > std::chrono::milliseconds(0)) {
      report_timer_ =
          network_read_callbacks_->connection().dispatcher().createTimer(
              [this] { onReportTimer(); });
      report_timer_->enableTimer(config_->report_duration_);
    }
    return Network::FilterStatus::Continue;
  }
  void initializeReadFilterCallbacks(
      Network::ReadFilterCallbacks& callbacks) override {
    network_read_callbacks_ = &callbacks;
    network_read_callbacks_->connection().addConnectionCallbacks(*this);
  }
  // Network::ConnectionCallbacks
  void onEvent(Network::ConnectionEvent event) override {
    switch (event) {
      case Network::ConnectionEvent::LocalClose:
      case Network::ConnectionEvent::RemoteClose:
        reportHelper(true);
        if (report_timer_) {
          report_timer_->disableTimer();
          report_timer_.reset();
        }
        break;
      default:
        break;
    }
  }
  void onAboveWriteBufferHighWatermark() override {}
  void onBelowWriteBufferLowWatermark() override {}

 private:
  // Invoked periodically for TCP streams.
  void reportHelper(bool end_stream) {
    const auto& info = network_read_callbacks_->connection().streamInfo();
    // TCP MX writes to upstream stream info instead.
    OptRef<const StreamInfo::UpstreamInfo> upstream_info;
    if (config_->reporter() == Reporter::ClientSidecar) {
      upstream_info = info.upstreamInfo();
    }
    const StreamInfo::FilterState& filter_state =
        upstream_info && upstream_info->upstreamFilterState()
            ? *upstream_info->upstreamFilterState()
            : info.filterState();

    if (!network_peer_read_) {
      network_peer_read_ = peerInfoRead(filter_state);
      // Report connection open once peer info is read or connection is closed.
      if (network_peer_read_ || end_stream) {
        populatePeerInfo(info, filter_state);
        tags_.push_back({context_.request_protocol_, context_.tcp_});
        populateFlagsAndConnectionSecurity(info);
        Stats::Utility::counterFromStatNames(
            config_->scope_,
            {context_.stat_namespace_, context_.tcp_connections_opened_total_},
            tags_)
            .inc();
      }
    }
    if (network_peer_read_ || end_stream) {
      auto meter = info.getDownstreamBytesMeter();
      if (meter) {
        Stats::Utility::counterFromStatNames(
            config_->scope_,
            {context_.stat_namespace_, context_.tcp_sent_bytes_total_}, tags_)
            .add(meter->wireBytesSent() - bytes_sent_);
        bytes_sent_ = meter->wireBytesSent();
        Stats::Utility::counterFromStatNames(
            config_->scope_,
            {context_.stat_namespace_, context_.tcp_received_bytes_total_},
            tags_)
            .add(meter->wireBytesReceived() - bytes_received_);
        bytes_received_ = meter->wireBytesReceived();
      }
    }
    if (end_stream) {
      Stats::Utility::counterFromStatNames(
          config_->scope_,
          {context_.stat_namespace_, context_.tcp_connections_closed_total_},
          tags_)
          .inc();
    }
  }
  void onReportTimer() {
    reportHelper(false);
    report_timer_->enableTimer(config_->report_duration_);
  }

  // Detect if peer info is read by TCP metadata exchange.
  bool peerInfoRead(const StreamInfo::FilterState& filter_state) {
    const auto& filter_state_key =
        config_->reporter() == Reporter::ServerSidecar
            ? "wasm.downstream_peer_id"
            : "wasm.upstream_peer_id";
    const auto* object = filter_state.getDataReadOnly<
        Envoy::Extensions::Filters::Common::Expr::CelState>(filter_state_key);
    return object != nullptr;
  }

  void populateFlagsAndConnectionSecurity(const StreamInfo::StreamInfo& info) {
    tags_.push_back(
        {context_.response_flags_,
         config_->resolve(StreamInfo::ResponseFlagUtils::toShortString(info))});
    switch (config_->reporter()) {
      case Reporter::ServerSidecar: {
        const auto ssl_info = info.downstreamAddressProvider().sslConnection();
        const auto mtls =
            ssl_info != nullptr && ssl_info->peerCertificatePresented();
        tags_.push_back({context_.connection_security_policy_,
                         mtls ? context_.mutual_tls_ : context_.none_});
        break;
      }
      default:
        tags_.push_back(
            {context_.connection_security_policy_, context_.unknown_});
        break;
    }
  }

  // Peer metadata is populated after encode/decodeHeaders by MX HTTP filter,
  // and after initial bytes read/written by MX TCP filter.
  void populatePeerInfo(const StreamInfo::StreamInfo& info,
                        const StreamInfo::FilterState& filter_state) {
    const auto& filter_state_key =
        config_->reporter() == Reporter::ServerSidecar ? "wasm.downstream_peer"
                                                       : "wasm.upstream_peer";
    const auto* object = filter_state.getDataReadOnly<
        Envoy::Extensions::Filters::Common::Expr::CelState>(filter_state_key);
    absl::optional<Istio::Common::WorkloadMetadataObject> peer;
    if (object) {
      const auto& node =
          *flatbuffers::GetRoot<Wasm::Common::FlatNode>(object->value().data());
      peer.emplace(Istio::Common::convertFlatNodeToWorkloadMetadata(node));
    }
    // Compute destination service with fallbacks.
    absl::string_view service_host;
    absl::string_view service_host_name;
    const auto cluster_info = info.upstreamClusterInfo();
    if (cluster_info && cluster_info.value()) {
      const auto& filter_metadata =
          cluster_info.value()->metadata().filter_metadata();
      const auto& it = filter_metadata.find("istio");
      if (it != filter_metadata.end()) {
        const auto& services_it = it->second.fields().find("services");
        if (services_it != it->second.fields().end()) {
          const auto& services = services_it->second.list_value();
          if (services.values_size() > 0) {
            const auto& service = services.values(0).struct_value().fields();
            const auto& host_it = service.find("host");
            if (host_it != service.end()) {
              service_host = host_it->second.string_value();
              service_host_name =
                  service_host.substr(0, service_host.find_first_of('.'));
            }
          }
        }
      }
    }
    if (service_host.empty() && !config_->disable_host_header_fallback_) {
      const auto* headers = info.getRequestHeaders();
      if (headers && headers->Host()) {
        service_host = headers->Host()->value().getStringView();
        service_host_name = service_host;
      }
    }
    switch (config_->reporter()) {
      case Reporter::ServerSidecar: {
        tags_.push_back({context_.source_workload_,
                         peer ? config_->resolve(peer->workload_name_)
                              : context_.unknown_});
        tags_.push_back({context_.source_canonical_service_,
                         peer ? config_->resolve(peer->canonical_name_)
                              : context_.unknown_});
        tags_.push_back({context_.source_canonical_revision_,
                         peer ? config_->resolve(peer->canonical_revision_)
                              : context_.latest_});
        tags_.push_back({context_.source_workload_namespace_,
                         peer ? config_->resolve(peer->namespace_name_)
                              : context_.unknown_});
        const auto ssl_info = info.downstreamAddressProvider().sslConnection();
        tags_.push_back(
            {context_.source_principal_,
             ssl_info && !ssl_info->uriSanPeerCertificate().empty()
                 ? config_->resolve(ssl_info->uriSanPeerCertificate()[0])
                 : context_.unknown_});
        tags_.push_back(
            {context_.source_app_,
             peer ? config_->resolve(peer->app_name_) : context_.unknown_});
        tags_.push_back(
            {context_.source_version_,
             peer ? config_->resolve(peer->app_version_) : context_.unknown_});
        tags_.push_back(
            {context_.source_cluster_,
             peer ? config_->resolve(peer->cluster_name_) : context_.unknown_});
        tags_.push_back(
            {context_.destination_workload_, context_.workload_name_});
        tags_.push_back(
            {context_.destination_workload_namespace_, context_.namespace_});
        tags_.push_back(
            {context_.destination_principal_,
             ssl_info && !ssl_info->uriSanLocalCertificate().empty()
                 ? config_->resolve(ssl_info->uriSanLocalCertificate()[0])
                 : context_.unknown_});
        tags_.push_back({context_.destination_app_, context_.app_name_});
        tags_.push_back({context_.destination_version_, context_.app_version_});
        tags_.push_back({context_.destination_service_,
                         service_host.empty()
                             ? context_.canonical_name_
                             : config_->resolve(service_host)});
        tags_.push_back({context_.destination_canonical_service_,
                         context_.canonical_name_});
        tags_.push_back({context_.destination_canonical_revision_,
                         context_.canonical_revision_});
        tags_.push_back({context_.destination_service_name_,
                         service_host_name.empty()
                             ? context_.canonical_name_
                             : config_->resolve(service_host_name)});
        tags_.push_back(
            {context_.destination_service_namespace_, context_.namespace_});
        tags_.push_back(
            {context_.destination_cluster_, context_.cluster_name_});

        break;
      }
      case Reporter::ClientSidecar: {
        tags_.push_back({context_.source_workload_, context_.workload_name_});
        tags_.push_back(
            {context_.source_canonical_service_, context_.canonical_name_});
        tags_.push_back({context_.source_canonical_revision_,
                         context_.canonical_revision_});
        tags_.push_back(
            {context_.source_workload_namespace_, context_.namespace_});
        const auto upstream_info = info.upstreamInfo();
        const Ssl::ConnectionInfoConstSharedPtr ssl_info =
            upstream_info ? upstream_info->upstreamSslConnection() : nullptr;
        tags_.push_back(
            {context_.source_principal_,
             ssl_info && !ssl_info->uriSanLocalCertificate().empty()
                 ? config_->resolve(ssl_info->uriSanLocalCertificate()[0])
                 : context_.unknown_});
        tags_.push_back({context_.source_app_, context_.app_name_});
        tags_.push_back({context_.source_version_, context_.app_version_});
        tags_.push_back({context_.source_cluster_, context_.cluster_name_});
        tags_.push_back({context_.destination_workload_,
                         peer ? config_->resolve(peer->workload_name_)
                              : context_.unknown_});
        tags_.push_back({context_.destination_workload_namespace_,
                         peer ? config_->resolve(peer->namespace_name_)
                              : context_.unknown_});
        tags_.push_back(
            {context_.destination_principal_,
             ssl_info && !ssl_info->uriSanPeerCertificate().empty()
                 ? config_->resolve(ssl_info->uriSanPeerCertificate()[0])
                 : context_.unknown_});
        tags_.push_back(
            {context_.destination_app_,
             peer ? config_->resolve(peer->app_name_) : context_.unknown_});
        tags_.push_back(
            {context_.destination_version_,
             peer ? config_->resolve(peer->app_version_) : context_.unknown_});
        tags_.push_back({context_.destination_service_,
                         service_host.empty()
                             ? context_.unknown_
                             : config_->resolve(service_host)});
        tags_.push_back({context_.destination_canonical_service_,
                         peer ? config_->resolve(peer->canonical_name_)
                              : context_.unknown_});
        tags_.push_back({context_.destination_canonical_revision_,
                         peer ? config_->resolve(peer->canonical_revision_)
                              : context_.latest_});
        tags_.push_back({context_.destination_service_name_,
                         service_host_name.empty()
                             ? context_.unknown_
                             : config_->resolve(service_host_name)});
        tags_.push_back({context_.destination_service_namespace_,
                         peer ? config_->resolve(peer->namespace_name_)
                              : context_.unknown_});
        tags_.push_back(
            {context_.destination_cluster_,
             peer ? config_->resolve(peer->cluster_name_) : context_.unknown_});
        break;
      }
      default:
        break;
    }
  }

  ConfigSharedPtr config_;
  Context& context_;
  Stats::StatNameTagVector tags_;
  Event::TimerPtr report_timer_{nullptr};
  Network::ReadFilterCallbacks* network_read_callbacks_;
  bool network_peer_read_{false};
  uint64_t bytes_sent_{0};
  uint64_t bytes_received_{0};
};

}  // namespace

Http::FilterFactoryCb
IstioStatsFilterConfigFactory::createFilterFactoryFromProtoTyped(
    const stats::PluginConfig& proto_config, const std::string&,
    Server::Configuration::FactoryContext& factory_context) {
  factory_context.api().customStatNamespaces().registerStatNamespace(
      CustomStatNamespace);
  ConfigSharedPtr config =
      std::make_shared<Config>(proto_config, factory_context);
  return [config](Http::FilterChainFactoryCallbacks& callbacks) {
    callbacks.addStreamFilter(std::make_shared<IstioStatsFilter>(config));
  };
}

REGISTER_FACTORY(IstioStatsFilterConfigFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory);

Network::FilterFactoryCb
IstioStatsNetworkFilterConfigFactory::createFilterFactoryFromProtoTyped(
    const stats::PluginConfig& proto_config,
    Server::Configuration::FactoryContext& factory_context) {
  factory_context.api().customStatNamespaces().registerStatNamespace(
      CustomStatNamespace);
  ConfigSharedPtr config =
      std::make_shared<Config>(proto_config, factory_context);
  return [config](Network::FilterManager& filter_manager) {
    filter_manager.addReadFilter(std::make_shared<IstioStatsFilter>(config));
  };
}

REGISTER_FACTORY(IstioStatsNetworkFilterConfigFactory,
                 Server::Configuration::NamedNetworkFilterConfigFactory);

}  // namespace IstioStats
}  // namespace HttpFilters
}  // namespace Extensions
}  // namespace Envoy
