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

absl::string_view extractString(const envoy::config::core::v3::Node& node,
                                const std::string& key) {
  const auto& it = node.metadata().fields().find(key);
  if (it == node.metadata().fields().end()) {
    return {};
  }
  return it->second.string_value();
}

// Process-wide context shared with all filter instances.
struct Context : public Singleton::Instance {
  explicit Context(Stats::SymbolTable& symbol_table,
                   const envoy::config::core::v3::Node& node)
      : pool_(symbol_table),
        stat_namespace_(pool_.add(CustomStatNamespace)),
        requests_total_(pool_.add("istio_requests_total")),
        unknown_(pool_.add("unknown")),
        source_(pool_.add("source")),
        destination_(pool_.add("destination")),
        latest_(pool_.add("latest")),
        http_(pool_.add("http")),
        grpc_(pool_.add("grpc")),
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
        workload_name_(pool_.add(extractString(node, "WORKLOAD_NAME"))),
        namespace_(pool_.add(extractString(node, "NAMESPACE"))) {}

  Stats::StatNamePool pool_;

  // Metric names.
  const Stats::StatName stat_namespace_;
  const Stats::StatName requests_total_;

  // Constant names.
  const Stats::StatName unknown_;
  const Stats::StatName source_;
  const Stats::StatName destination_;
  const Stats::StatName latest_;
  const Stats::StatName http_;
  const Stats::StatName grpc_;

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
      case Reporter::ServerSidecar:
        tags_.push_back({context_.source_workload_namespace_,
                         peer ? config_->resolve(peer->namespaceName())
                              : context_.unknown_});
        tags_.push_back({context_.source_workload_,
                         peer ? config_->resolve(peer->workloadName())
                              : context_.unknown_});
        tags_.push_back(
            {context_.destination_workload_namespace_, context_.namespace_});
        tags_.push_back(
            {context_.destination_workload_, context_.workload_name_});
        break;
      case Reporter::ClientSidecar:
        tags_.push_back(
            {context_.source_workload_namespace_, context_.namespace_});
        tags_.push_back({context_.source_workload_, context_.workload_name_});
        tags_.push_back({context_.destination_workload_namespace_,
                         peer ? config_->resolve(peer->namespaceName())
                              : context_.unknown_});
        tags_.push_back({context_.destination_workload_,
                         peer ? config_->resolve(peer->workloadName())
                              : context_.unknown_});
        break;
    }
    return Http::FilterHeadersStatus::Continue;
  }

  void onStreamComplete() override {
    Stats::Utility::counterFromElements(
        config_->scope_, {context_.stat_namespace_, context_.requests_total_},
        tags_)
        .inc();
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
