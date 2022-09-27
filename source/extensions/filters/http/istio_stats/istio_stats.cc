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
#include "source/common/grpc/common.h"
#include "source/common/http/header_utility.h"
#include "source/common/stream_info/utility.h"
#include "source/extensions/filters/http/common/pass_through_filter.h"
#include "src/envoy/common/metadata_object.h"

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
        unknown_(pool_.add("unknown")),
        source_(pool_.add("source")),
        destination_(pool_.add("destination")),
        latest_(pool_.add("latest")),
        http_(pool_.add("http")),
        grpc_(pool_.add("grpc")),
        mtls_(pool_.add("mtls")),
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
        cluster_name_(pool_.add(extractString(node.metadata(), "CLUSTER_ID"))) {
  }

  Stats::StatNamePool pool_;

  // Metric names.
  const Stats::StatName stat_namespace_;
  const Stats::StatName requests_total_;
  const Stats::StatName request_duration_milliseconds_;
  const Stats::StatName request_bytes_;
  const Stats::StatName response_bytes_;

  // Constant names.
  const Stats::StatName unknown_;
  const Stats::StatName source_;
  const Stats::StatName destination_;
  const Stats::StatName latest_;
  const Stats::StatName http_;
  const Stats::StatName grpc_;
  const Stats::StatName mtls_;
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
};
using ContextSharedPtr = std::shared_ptr<Context>;

SINGLETON_MANAGER_REGISTRATION(Context)

enum class Reporter {
  ClientSidecar,
  ServerSidecar,
};

struct Config {
  Config(const stats::PluginConfig&,
         Server::Configuration::FactoryContext& factory_context)
      : context_(factory_context.singletonManager().getTyped<Context>(
            SINGLETON_MANAGER_REGISTERED_NAME(Context),
            [&factory_context] {
              return std::make_shared<Context>(
                  factory_context.serverScope().symbolTable(),
                  factory_context.localInfo().node());
            })),
        scope_(factory_context.scope()),
        pool_(scope_.symbolTable()) {
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
};

using ConfigSharedPtr = std::shared_ptr<Config>;

class IstioStatsFilter : public Http::PassThroughFilter {
 public:
  IstioStatsFilter(ConfigSharedPtr config)
      : config_(config), context_(*config->context_) {
    switch (config_->reporter()) {
      case Reporter::ServerSidecar:
        tags_.push_back({context_.reporter_, context_.destination_});
        break;
      case Reporter::ClientSidecar:
        tags_.push_back({context_.reporter_, context_.source_});
        break;
    }
  }

  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& headers,
                                          bool) override {
    if (Grpc::Common::isGrpcRequestHeaders(headers)) {
      tags_.push_back({context_.request_protocol_, context_.grpc_});
    } else {
      tags_.push_back({context_.request_protocol_, context_.http_});
    }
    std::shared_ptr<Envoy::Common::WorkloadMetadataObject> peer;
    auto baggage_result = Http::HeaderUtility::getAllOfHeaderAsString(
                              headers, Http::LowerCaseString("baggage"))
                              .result();
    if (baggage_result) {
      peer = Envoy::Common::WorkloadMetadataObject::fromBaggage(
          baggage_result.value());
    }
    switch (config_->reporter()) {
      case Reporter::ServerSidecar: {
        tags_.push_back({context_.source_workload_namespace_,
                         peer ? config_->resolve(peer->namespaceName())
                              : context_.unknown_});
        tags_.push_back({context_.source_workload_,
                         peer ? config_->resolve(peer->workloadName())
                              : context_.unknown_});
        tags_.push_back({context_.source_canonical_service_,
                         peer ? config_->resolve(peer->canonicalName())
                              : context_.unknown_});
        tags_.push_back({context_.source_canonical_revision_,
                         peer ? config_->resolve(peer->canonicalRevision())
                              : context_.unknown_});
        tags_.push_back(
            {context_.source_cluster_,
             peer ? config_->resolve(peer->clusterName()) : context_.unknown_});
        tags_.push_back(
            {context_.destination_workload_namespace_, context_.namespace_});
        tags_.push_back(
            {context_.destination_workload_, context_.workload_name_});
        tags_.push_back({context_.destination_canonical_service_,
                         context_.canonical_name_});
        tags_.push_back({context_.destination_canonical_revision_,
                         context_.canonical_revision_});

        const auto& info = decoder_callbacks_->streamInfo();
        const auto ssl_info = info.downstreamAddressProvider().sslConnection();
        tags_.push_back(
            {context_.source_principal_,
             ssl_info ? config_->resolve(absl::StrJoin(ssl_info->uriSanPeerCertificate(), ","))
                      : context_.unknown_});
        tags_.push_back(
            {context_.destination_principal_,
             ssl_info ? config_->resolve(absl::StrJoin(ssl_info->uriSanLocalCertificate(), ","))
                      : context_.unknown_});

        // Specific to reporter:
        const auto mtls =
            ssl_info != nullptr && ssl_info->peerCertificatePresented();
        tags_.push_back({context_.connection_security_policy_,
                         mtls ? context_.mtls_ : context_.none_});
        break;
      }
      case Reporter::ClientSidecar:
        tags_.push_back(
            {context_.source_workload_namespace_, context_.namespace_});
        tags_.push_back({context_.source_workload_, context_.workload_name_});
        tags_.push_back(
            {context_.source_canonical_service_, context_.canonical_name_});
        tags_.push_back({context_.source_canonical_revision_,
                         context_.canonical_revision_});
        tags_.push_back({context_.destination_workload_namespace_,
                         peer ? config_->resolve(peer->namespaceName())
                              : context_.unknown_});
        tags_.push_back({context_.destination_workload_,
                         peer ? config_->resolve(peer->workloadName())
                              : context_.unknown_});
        tags_.push_back({context_.destination_canonical_service_,
                         peer ? config_->resolve(peer->canonicalName())
                              : context_.unknown_});
        tags_.push_back({context_.destination_canonical_revision_,
                         peer ? config_->resolve(peer->canonicalRevision())
                              : context_.unknown_});
        tags_.push_back(
            {context_.destination_cluster_,
             peer ? config_->resolve(peer->clusterName()) : context_.unknown_});
        break;
    }
    // app, version
    return Http::FilterHeadersStatus::Continue;
  }

  void onStreamComplete() override {
    const auto& info = decoder_callbacks_->streamInfo();

    // TODO: copy Http::CodeStatsImpl version for status codes and flags.
    tags_.push_back(
        {context_.response_code_,
         config_->resolve(absl::StrCat(info.responseCode().value_or(0)))});
    tags_.push_back(
        {context_.response_flags_,
         config_->resolve(StreamInfo::ResponseFlagUtils::toShortString(info))});

    // Complete info from upstream connection if missing.
    switch (config_->reporter()) {
      case Reporter::ClientSidecar: {
        const auto upstream_info = info.upstreamInfo();
        const Ssl::ConnectionInfoConstSharedPtr ssl_info =
            upstream_info ? upstream_info->upstreamSslConnection() : nullptr;
        tags_.push_back(
            {context_.source_principal_,
             ssl_info ? config_->resolve(absl::StrJoin(ssl_info->uriSanLocalCertificate(), ","))
                      : context_.unknown_});
        tags_.push_back(
            {context_.destination_principal_,
             ssl_info ? config_->resolve(absl::StrJoin(ssl_info->uriSanPeerCertificate(), ","))
                      : context_.unknown_});
        break;
      }
      default:
        break;
    }

    Stats::Utility::counterFromElements(
        config_->scope_, {context_.stat_namespace_, context_.requests_total_},
        tags_)
        .inc();
    auto duration = info.requestComplete();
    if (duration.has_value()) {
      Stats::Utility::histogramFromElements(
          config_->scope_,
          {context_.stat_namespace_, context_.request_duration_milliseconds_},
          Stats::Histogram::Unit::Milliseconds, tags_)
          .recordValue(absl::FromChrono(duration.value()) /
                       absl::Milliseconds(1));
    }
    auto meter = info.getDownstreamBytesMeter();
    if (meter) {
      Stats::Utility::histogramFromElements(
          config_->scope_, {context_.stat_namespace_, context_.request_bytes_},
          Stats::Histogram::Unit::Bytes, tags_)
          .recordValue(meter->wireBytesReceived());
      Stats::Utility::histogramFromElements(
          config_->scope_, {context_.stat_namespace_, context_.response_bytes_},
          Stats::Histogram::Unit::Bytes, tags_)
          .recordValue(meter->wireBytesSent());
    }
  }

 private:
  ConfigSharedPtr config_;
  Context& context_;
  Stats::StatNameTagVector tags_;
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

}  // namespace IstioStats
}  // namespace HttpFilters
}  // namespace Extensions
}  // namespace Envoy
