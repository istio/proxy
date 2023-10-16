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

#include <atomic>

#include "envoy/registry/registry.h"
#include "envoy/server/factory_context.h"
#include "envoy/singleton/manager.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "extensions/common/metadata_object.h"
#include "parser/parser.h"
#include "source/common/grpc/common.h"
#include "source/common/http/header_map_impl.h"
#include "source/common/http/header_utility.h"
#include "source/common/network/utility.h"
#include "source/common/stream_info/utility.h"
#include "source/extensions/common/utils.h"
#include "source/extensions/common/filter_objects.h"
#include "source/extensions/filters/common/expr/cel_state.h"
#include "source/extensions/filters/common/expr/context.h"
#include "source/extensions/filters/common/expr/evaluator.h"
#include "source/extensions/filters/http/common/pass_through_filter.h"
#include "source/extensions/filters/http/grpc_stats/grpc_stats_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace IstioStats {

namespace {
constexpr absl::string_view CustomStatNamespace = "istiocustom";

absl::string_view extractString(const ProtobufWkt::Struct& metadata, const std::string& key) {
  const auto& it = metadata.fields().find(key);
  if (it == metadata.fields().end()) {
    return {};
  }
  return it->second.string_value();
}

absl::string_view extractMapString(const ProtobufWkt::Struct& metadata, const std::string& map_key,
                                   const std::string& key) {
  const auto& it = metadata.fields().find(map_key);
  if (it == metadata.fields().end()) {
    return {};
  }
  return extractString(it->second.struct_value(), key);
}

absl::optional<Istio::Common::WorkloadMetadataObject>
extractEndpointMetadata(const StreamInfo::StreamInfo& info) {
  auto upstream_info = info.upstreamInfo();
  auto upstream_host = upstream_info ? upstream_info->upstreamHost() : nullptr;
  if (upstream_host && upstream_host->metadata()) {
    const auto& filter_metadata = upstream_host->metadata()->filter_metadata();
    const auto& it = filter_metadata.find("istio");
    if (it != filter_metadata.end()) {
      const auto& workload_it = it->second.fields().find("workload");
      if (workload_it != it->second.fields().end()) {
        return Istio::Common::convertEndpointMetadata(workload_it->second.string_value());
      }
    }
  }
  return {};
}

enum class Reporter {
  // Regular outbound listener on a sidecar.
  ClientSidecar,
  // Regular inbound listener on a sidecar.
  ServerSidecar,
  // Gateway listener for a set of destination workloads.
  ServerGateway,
};

// Detect if peer info read is completed by TCP metadata exchange.
bool peerInfoRead(Reporter reporter, const StreamInfo::FilterState& filter_state) {
  const auto& filter_state_key =
      reporter == Reporter::ServerSidecar || reporter == Reporter::ServerGateway
          ? "wasm.downstream_peer_id"
          : "wasm.upstream_peer_id";
  return filter_state.hasDataWithName(filter_state_key) ||
         filter_state.hasDataWithName("envoy.wasm.metadata_exchange.peer_unknown");
}

const Wasm::Common::FlatNode* peerInfo(Reporter reporter,
                                       const StreamInfo::FilterState& filter_state) {
  const auto& filter_state_key =
      reporter == Reporter::ServerSidecar || reporter == Reporter::ServerGateway
          ? "wasm.downstream_peer"
          : "wasm.upstream_peer";
  const auto* object =
      filter_state.getDataReadOnly<Envoy::Extensions::Filters::Common::Expr::CelState>(
          filter_state_key);
  return object ? flatbuffers::GetRoot<Wasm::Common::FlatNode>(object->value().data()) : nullptr;
}

// Process-wide context shared with all filter instances.
struct Context : public Singleton::Instance {
  explicit Context(Stats::SymbolTable& symbol_table, const envoy::config::core::v3::Node& node)
      : pool_(symbol_table), node_(node), stat_namespace_(pool_.add(CustomStatNamespace)),
        requests_total_(pool_.add("istio_requests_total")),
        request_duration_milliseconds_(pool_.add("istio_request_duration_milliseconds")),
        request_bytes_(pool_.add("istio_request_bytes")),
        response_bytes_(pool_.add("istio_response_bytes")),
        request_messages_total_(pool_.add("istio_request_messages_total")),
        response_messages_total_(pool_.add("istio_response_messages_total")),
        tcp_connections_opened_total_(pool_.add("istio_tcp_connections_opened_total")),
        tcp_connections_closed_total_(pool_.add("istio_tcp_connections_closed_total")),
        tcp_sent_bytes_total_(pool_.add("istio_tcp_sent_bytes_total")),
        tcp_received_bytes_total_(pool_.add("istio_tcp_received_bytes_total")),
        empty_(pool_.add("")), unknown_(pool_.add("unknown")), source_(pool_.add("source")),
        destination_(pool_.add("destination")), latest_(pool_.add("latest")),
        http_(pool_.add("http")), grpc_(pool_.add("grpc")), tcp_(pool_.add("tcp")),
        mutual_tls_(pool_.add("mutual_tls")), none_(pool_.add("none")),
        reporter_(pool_.add("reporter")), source_workload_(pool_.add("source_workload")),
        source_workload_namespace_(pool_.add("source_workload_namespace")),
        source_principal_(pool_.add("source_principal")), source_app_(pool_.add("source_app")),
        source_version_(pool_.add("source_version")),
        source_canonical_service_(pool_.add("source_canonical_service")),
        source_canonical_revision_(pool_.add("source_canonical_revision")),
        source_cluster_(pool_.add("source_cluster")),
        destination_workload_(pool_.add("destination_workload")),
        destination_workload_namespace_(pool_.add("destination_workload_namespace")),
        destination_principal_(pool_.add("destination_principal")),
        destination_app_(pool_.add("destination_app")),
        destination_version_(pool_.add("destination_version")),
        destination_service_(pool_.add("destination_service")),
        destination_service_name_(pool_.add("destination_service_name")),
        destination_service_namespace_(pool_.add("destination_service_namespace")),
        destination_canonical_service_(pool_.add("destination_canonical_service")),
        destination_canonical_revision_(pool_.add("destination_canonical_revision")),
        destination_cluster_(pool_.add("destination_cluster")),
        request_protocol_(pool_.add("request_protocol")),
        response_flags_(pool_.add("response_flags")),
        connection_security_policy_(pool_.add("connection_security_policy")),
        response_code_(pool_.add("response_code")),
        grpc_response_status_(pool_.add("grpc_response_status")),
        workload_name_(pool_.add(extractString(node.metadata(), "WORKLOAD_NAME"))),
        namespace_(pool_.add(extractString(node.metadata(), "NAMESPACE"))),
        canonical_name_(pool_.add(
            extractMapString(node.metadata(), "LABELS", "service.istio.io/canonical-name"))),
        canonical_revision_(pool_.add(
            extractMapString(node.metadata(), "LABELS", "service.istio.io/canonical-revision"))),
        cluster_name_(pool_.add(extractString(node.metadata(), "CLUSTER_ID"))),
        app_name_(pool_.add(extractMapString(node.metadata(), "LABELS", "app"))),
        app_version_(pool_.add(extractMapString(node.metadata(), "LABELS", "version"))),
        istio_build_(pool_.add("istio_build")), component_(pool_.add("component")),
        proxy_(pool_.add("proxy")), tag_(pool_.add("tag")),
        istio_version_(pool_.add(extractString(node.metadata(), "ISTIO_VERSION"))) {
    all_metrics_ = {
        {"requests_total", requests_total_},
        {"request_duration_milliseconds", request_duration_milliseconds_},
        {"request_bytes", request_bytes_},
        {"response_bytes", response_bytes_},
        {"request_messages_total", request_messages_total_},
        {"response_messages_total", response_messages_total_},
        {"tcp_connections_opened_total", tcp_connections_opened_total_},
        {"tcp_connections_closed_total", tcp_connections_closed_total_},
        {"tcp_sent_bytes_total", tcp_sent_bytes_total_},
        {"tcp_received_bytes_total", tcp_received_bytes_total_},
    };
    all_tags_ = {
        {"reporter", reporter_},
        {"source_workload", source_workload_},
        {"source_workload_namespace", source_workload_namespace_},
        {"source_principal", source_principal_},
        {"source_app", source_app_},
        {"source_version", source_version_},
        {"source_canonical_service", source_canonical_service_},
        {"source_canonical_revision", source_canonical_revision_},
        {"source_cluster", source_cluster_},
        {"destination_workload", destination_workload_},
        {"destination_workload_namespace", destination_workload_namespace_},
        {"destination_principal", destination_principal_},
        {"destination_app", destination_app_},
        {"destination_version", destination_version_},
        {"destination_service", destination_service_},
        {"destination_service_name", destination_service_name_},
        {"destination_service_namespace", destination_service_namespace_},
        {"destination_canonical_service", destination_canonical_service_},
        {"destination_canonical_revision", destination_canonical_revision_},
        {"destination_cluster", destination_cluster_},
        {"request_protocol", request_protocol_},
        {"response_flags", response_flags_},
        {"connection_security_policy", connection_security_policy_},
        {"response_code", response_code_},
        {"grpc_response_status", grpc_response_status_},
    };
  }

  Stats::StatNamePool pool_;
  const envoy::config::core::v3::Node& node_;
  absl::flat_hash_map<std::string, Stats::StatName> all_metrics_;
  absl::flat_hash_map<std::string, Stats::StatName> all_tags_;

  // Metric names.
  const Stats::StatName stat_namespace_;
  const Stats::StatName requests_total_;
  const Stats::StatName request_duration_milliseconds_;
  const Stats::StatName request_bytes_;
  const Stats::StatName response_bytes_;
  const Stats::StatName request_messages_total_;
  const Stats::StatName response_messages_total_;
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

  // Tag names.
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

  // istio_build metric:
  // Publishes Istio version for the proxy as a gauge, sample data:
  // testdata/metric/istio_build.yaml
  // Sample value for istio_version: "1.17.0"
  const Stats::StatName istio_build_;
  const Stats::StatName component_;
  const Stats::StatName proxy_;
  const Stats::StatName tag_;
  const Stats::StatName istio_version_;
}; // namespace

using ContextSharedPtr = std::shared_ptr<Context>;

SINGLETON_MANAGER_REGISTRATION(Context)

using google::api::expr::runtime::CelValue;

// Instructions on dropping, creating, and overriding labels.
// This is not the "hot path" of the metrics system and thus, fairly
// unoptimized.
struct MetricOverrides : public Logger::Loggable<Logger::Id::filter> {
  MetricOverrides(ContextSharedPtr& context, Stats::SymbolTable& symbol_table)
      : context_(context), pool_(symbol_table) {}
  ContextSharedPtr context_;
  Stats::StatNameDynamicPool pool_;

  enum class MetricType {
    Counter,
    Gauge,
    Histogram,
  };
  struct CustomMetric {
    Stats::StatName name_;
    uint32_t expr_;
    MetricType type_;
    explicit CustomMetric(Stats::StatName name, uint32_t expr, MetricType type)
        : name_(name), expr_(expr), type_(type) {}
  };
  absl::flat_hash_map<std::string, CustomMetric> custom_metrics_;
  // Initial transformation: metrics dropped.
  absl::flat_hash_set<Stats::StatName> drop_;
  // Second transformation: tags changed.
  using TagOverrides = absl::flat_hash_map<Stats::StatName, absl::optional<uint32_t>>;
  absl::flat_hash_map<Stats::StatName, TagOverrides> tag_overrides_;
  // Third transformation: tags added.
  using TagAdditions = std::vector<std::pair<Stats::StatName, uint32_t>>;
  absl::flat_hash_map<Stats::StatName, TagAdditions> tag_additions_;

  Stats::StatNameTagVector
  overrideTags(Stats::StatName metric, const Stats::StatNameTagVector& tags,
               const std::vector<std::pair<Stats::StatName, uint64_t>>& expr_values) {
    Stats::StatNameTagVector out;
    out.reserve(tags.size());
    const auto& tag_overrides_it = tag_overrides_.find(metric);
    if (tag_overrides_it == tag_overrides_.end()) {
      out = tags;
    } else
      for (const auto& [key, val] : tags) {
        const auto& it = tag_overrides_it->second.find(key);
        if (it != tag_overrides_it->second.end()) {
          if (it->second.has_value()) {
            out.push_back({key, expr_values[it->second.value()].first});
          } else {
            // Skip dropped tags.
          }
        } else {
          out.push_back({key, val});
        }
      }
    const auto& tag_additions_it = tag_additions_.find(metric);
    if (tag_additions_it != tag_additions_.end()) {
      for (const auto& [tag, id] : tag_additions_it->second) {
        out.push_back({tag, expr_values[id].first});
      }
    }
    return out;
  }
  absl::optional<uint32_t> getOrCreateExpression(const std::string& expr, bool int_expr) {
    const auto& it = expression_ids_.find(expr);
    if (it != expression_ids_.end()) {
      return {it->second};
    }
    auto parse_status = google::api::expr::parser::Parse(expr);
    if (!parse_status.ok()) {
      return {};
    }
    if (expr_builder_ == nullptr) {
      google::api::expr::runtime::InterpreterOptions options;
      expr_builder_ = google::api::expr::runtime::CreateCelExpressionBuilder(options);
      auto register_status = google::api::expr::runtime::RegisterBuiltinFunctions(
          expr_builder_->GetRegistry(), options);
      if (!register_status.ok()) {
        throw Extensions::Filters::Common::Expr::CelException(
            absl::StrCat("failed to register built-in functions: ", register_status.message()));
      }
    }
    parsed_exprs_.push_back(parse_status.value().expr());
    compiled_exprs_.push_back(std::make_pair(
        Extensions::Filters::Common::Expr::createExpression(*expr_builder_, parsed_exprs_.back()),
        int_expr));
    uint32_t id = compiled_exprs_.size() - 1;
    expression_ids_.emplace(expr, id);
    return {id};
  }
  Filters::Common::Expr::BuilderPtr expr_builder_;
  std::vector<google::api::expr::v1alpha1::Expr> parsed_exprs_;
  std::vector<std::pair<Filters::Common::Expr::ExpressionPtr, bool>> compiled_exprs_;
  absl::flat_hash_map<std::string, uint32_t> expression_ids_;
};

// Self-managed scope with active rotation. Envoy stats scope controls the
// lifetime of the individual metrics. Because the scope is attached to xDS
// resources, metrics with data derived from the requests can accumulate and
// grow indefinitely for long-living xDS resources. To limit this growth,
// this class implements a rotation mechanism, whereas a new scope is created
// periodically to replace the current scope.
//
// The replaced stats scope is deleted gracefully after a minimum of 1s delay
// for two reasons:
//
// 1. Stats flushing is asynchronous and the data may be lost if not flushed
// before the deletion (see stats_flush_interval).
//
// 2. The implementation avoids locking by releasing a raw pointer to workers.
// When the rotation happens on the main, the raw pointer may still be in-use
// by workers for a short duration.
class RotatingScope : public Logger::Loggable<Logger::Id::filter> {
public:
  RotatingScope(Server::Configuration::FactoryContext& factory_context, uint64_t rotate_interval_ms,
                uint64_t delete_interval_ms)
      : parent_scope_(factory_context.scope()), active_scope_(parent_scope_.createScope("")),
        raw_scope_(active_scope_.get()), rotate_interval_ms_(rotate_interval_ms),
        delete_interval_ms_(delete_interval_ms) {
    if (rotate_interval_ms_ > 0) {
      ASSERT(delete_interval_ms_ < rotate_interval_ms_);
      ASSERT(delete_interval_ms_ >= 1000);
      Event::Dispatcher& dispatcher = factory_context.mainThreadDispatcher();
      rotate_timer_ = dispatcher.createTimer([this] { onRotate(); });
      delete_timer_ = dispatcher.createTimer([this] { onDelete(); });
      rotate_timer_->enableTimer(std::chrono::milliseconds(rotate_interval_ms_));
    }
  }
  ~RotatingScope() {
    if (rotate_timer_) {
      rotate_timer_->disableTimer();
      rotate_timer_.reset();
    }
    if (delete_timer_) {
      delete_timer_->disableTimer();
      delete_timer_.reset();
    }
  }
  Stats::Scope* scope() { return raw_scope_.load(); }

private:
  void onRotate() {
    ENVOY_LOG(info, "Rotating active Istio stats scope after {}ms.", rotate_interval_ms_);
    draining_scope_ = active_scope_;
    delete_timer_->enableTimer(std::chrono::milliseconds(delete_interval_ms_));
    active_scope_ = parent_scope_.createScope("");
    raw_scope_.store(active_scope_.get());
    rotate_timer_->enableTimer(std::chrono::milliseconds(rotate_interval_ms_));
  }
  void onDelete() {
    ENVOY_LOG(info, "Deleting draining Istio stats scope after {}ms.", delete_interval_ms_);
    draining_scope_.reset();
  }
  Stats::Scope& parent_scope_;
  Stats::ScopeSharedPtr active_scope_;
  std::atomic<Stats::Scope*> raw_scope_;
  Stats::ScopeSharedPtr draining_scope_{nullptr};
  const uint64_t rotate_interval_ms_;
  const uint64_t delete_interval_ms_;
  Event::TimerPtr rotate_timer_{nullptr};
  Event::TimerPtr delete_timer_{nullptr};
};

struct Config : public Logger::Loggable<Logger::Id::filter> {
  Config(const stats::PluginConfig& proto_config,
         Server::Configuration::FactoryContext& factory_context)
      : context_(factory_context.singletonManager().getTyped<Context>(
            SINGLETON_MANAGER_REGISTERED_NAME(Context),
            [&factory_context] {
              return std::make_shared<Context>(factory_context.serverScope().symbolTable(),
                                               factory_context.localInfo().node());
            })),
        scope_(factory_context, PROTOBUF_GET_MS_OR_DEFAULT(proto_config, rotation_interval, 0),
               PROTOBUF_GET_MS_OR_DEFAULT(proto_config, graceful_deletion_interval,
                                          /* 5m */ 1000 * 60 * 5)),
        disable_host_header_fallback_(proto_config.disable_host_header_fallback()),
        report_duration_(
            PROTOBUF_GET_MS_OR_DEFAULT(proto_config, tcp_reporting_duration, /* 5s */ 5000)) {
    reporter_ = Reporter::ClientSidecar;
    switch (proto_config.reporter()) {
    case stats::Reporter::UNSPECIFIED:
      switch (factory_context.direction()) {
      case envoy::config::core::v3::TrafficDirection::INBOUND:
        reporter_ = Reporter::ServerSidecar;
        break;
      case envoy::config::core::v3::TrafficDirection::OUTBOUND:
        reporter_ = Reporter::ClientSidecar;
        break;
      default:
        break;
      }
      break;
    case stats::Reporter::SERVER_GATEWAY:
      reporter_ = Reporter::ServerGateway;
      break;
    default:
      break;
    }
    if (proto_config.metrics_size() > 0 || proto_config.definitions_size() > 0) {
      metric_overrides_ = std::make_unique<MetricOverrides>(context_, scope()->symbolTable());
      for (const auto& definition : proto_config.definitions()) {
        const auto& it = context_->all_metrics_.find(definition.name());
        if (it != context_->all_metrics_.end()) {
          ENVOY_LOG(info, "Re-defining standard metric not allowed:: {}", definition.name());
          continue;
        }
        auto id = metric_overrides_->getOrCreateExpression(definition.value(), true);
        if (!id.has_value()) {
          ENVOY_LOG(info, "Failed to parse metric value expression: {}", definition.value());
          continue;
        }
        auto metric_type = MetricOverrides::MetricType::Counter;
        switch (definition.type()) {
        case stats::MetricType::GAUGE:
          metric_type = MetricOverrides::MetricType::Gauge;
          break;
        case stats::MetricType::HISTOGRAM:
          metric_type = MetricOverrides::MetricType::Histogram;
          break;
        default:
          break;
        }
        metric_overrides_->custom_metrics_.try_emplace(
            definition.name(),
            metric_overrides_->pool_.add(absl::StrCat("istio_", definition.name())), id.value(),
            metric_type);
      }
      for (const auto& metric : proto_config.metrics()) {
        if (metric.drop()) {
          const auto& it = context_->all_metrics_.find(metric.name());
          if (it != context_->all_metrics_.end()) {
            metric_overrides_->drop_.insert(it->second);
          }
          continue;
        }
        for (const auto& tag : metric.tags_to_remove()) {
          const auto& tag_it = context_->all_tags_.find(tag);
          if (tag_it == context_->all_tags_.end()) {
            ENVOY_LOG(info, "Tag is not standard: {}", tag);
            continue;
          }
          if (!metric.name().empty()) {
            const auto& it = context_->all_metrics_.find(metric.name());
            if (it != context_->all_metrics_.end()) {
              metric_overrides_->tag_overrides_[it->second][tag_it->second] = {};
            }
          } else
            for (const auto& [_, metric] : context_->all_metrics_) {
              metric_overrides_->tag_overrides_[metric][tag_it->second] = {};
            }
        }
        // Make order of tags deterministic.
        std::vector<std::string> tags;
        tags.reserve(metric.dimensions().size());
        for (const auto& [tag, _] : metric.dimensions()) {
          tags.push_back(tag);
        }
        std::sort(tags.begin(), tags.end());
        for (const auto& tag : tags) {
          const std::string& expr = metric.dimensions().at(tag);
          auto id = metric_overrides_->getOrCreateExpression(expr, false);
          if (!id.has_value()) {
            ENVOY_LOG(info, "Failed to parse expression: {}", expr);
          }
          const auto& tag_it = context_->all_tags_.find(tag);
          if (tag_it == context_->all_tags_.end()) {
            if (!id.has_value()) {
              continue;
            }
            const auto tag_name = metric_overrides_->pool_.add(tag);
            if (!metric.name().empty()) {
              const auto& it = context_->all_metrics_.find(metric.name());
              if (it != context_->all_metrics_.end()) {
                metric_overrides_->tag_additions_[it->second].push_back({tag_name, id.value()});
              }
              const auto& custom_it = metric_overrides_->custom_metrics_.find(metric.name());
              if (custom_it != metric_overrides_->custom_metrics_.end()) {
                metric_overrides_->tag_additions_[custom_it->second.name_].push_back(
                    {tag_name, id.value()});
              }
            } else {
              for (const auto& [_, metric] : context_->all_metrics_) {
                metric_overrides_->tag_additions_[metric].push_back({tag_name, id.value()});
              }
              for (const auto& [_, metric] : metric_overrides_->custom_metrics_) {
                metric_overrides_->tag_additions_[metric.name_].push_back({tag_name, id.value()});
              }
            }
          } else {
            const auto tag_name = tag_it->second;
            if (!metric.name().empty()) {
              const auto& it = context_->all_metrics_.find(metric.name());
              if (it != context_->all_metrics_.end()) {
                metric_overrides_->tag_overrides_[it->second][tag_name] = id;
              }
              const auto& custom_it = metric_overrides_->custom_metrics_.find(metric.name());
              if (custom_it != metric_overrides_->custom_metrics_.end()) {
                metric_overrides_->tag_additions_[custom_it->second.name_].push_back(
                    {tag_name, id.value()});
              }
            } else {
              for (const auto& [_, metric] : context_->all_metrics_) {
                metric_overrides_->tag_overrides_[metric][tag_name] = id;
              }
              for (const auto& [_, metric] : metric_overrides_->custom_metrics_) {
                metric_overrides_->tag_additions_[metric.name_].push_back({tag_name, id.value()});
              }
            }
          }
        }
      }
    }
  }

  // RAII for stream context propagation.
  struct StreamOverrides : public Filters::Common::Expr::StreamActivation {
    StreamOverrides(Config& parent, Stats::StatNameDynamicPool& pool,
                    const StreamInfo::StreamInfo& info,
                    const Http::RequestHeaderMap* request_headers = nullptr,
                    const Http::ResponseHeaderMap* response_headers = nullptr,
                    const Http::ResponseTrailerMap* response_trailers = nullptr)
        : parent_(parent) {
      if (parent_.metric_overrides_) {
        activation_info_ = &info;
        activation_request_headers_ = request_headers;
        activation_response_headers_ = response_headers;
        activation_response_trailers_ = response_trailers;
        const auto& compiled_exprs = parent_.metric_overrides_->compiled_exprs_;
        expr_values_.reserve(compiled_exprs.size());
        for (size_t id = 0; id < compiled_exprs.size(); id++) {
          Protobuf::Arena arena;
          auto eval_status = compiled_exprs[id].first->Evaluate(*this, &arena);
          if (!eval_status.ok() || eval_status.value().IsError()) {
            expr_values_.push_back(std::make_pair(parent_.context_->unknown_, 0));
          } else {
            const auto string_value = Filters::Common::Expr::print(eval_status.value());
            if (compiled_exprs[id].second) {
              uint64_t amount = 0;
              if (!absl::SimpleAtoi(string_value, &amount)) {
                ENVOY_LOG(trace, "Failed to get metric value: {}", string_value);
              }
              expr_values_.push_back(std::make_pair(Stats::StatName(), amount));
            } else {
              expr_values_.push_back(std::make_pair(pool.add(string_value), 0));
            }
          }
        }
        resetActivation();
      }
    }

    absl::optional<CelValue> FindValue(absl::string_view name,
                                       Protobuf::Arena* arena) const override {
      auto obj = StreamActivation::FindValue(name, arena);
      if (obj) {
        return obj;
      }
      if (name == "node") {
        return Filters::Common::Expr::CelProtoWrapper::CreateMessage(&parent_.context_->node_,
                                                                     arena);
      }
      if (activation_info_) {
        const auto* obj = activation_info_->filterState()
                              .getDataReadOnly<Envoy::Extensions::Filters::Common::Expr::CelState>(
                                  absl::StrCat("wasm.", name));
        if (obj) {
          return obj->exprValue(arena, false);
        }
      }
      return {};
    }

    void addCounter(Stats::StatName metric, const Stats::StatNameTagVector& tags,
                    uint64_t amount = 1) {
      if (parent_.metric_overrides_) {
        if (parent_.metric_overrides_->drop_.contains(metric)) {
          return;
        }
        auto new_tags = parent_.metric_overrides_->overrideTags(metric, tags, expr_values_);
        Stats::Utility::counterFromStatNames(*parent_.scope(),
                                             {parent_.context_->stat_namespace_, metric}, new_tags)
            .add(amount);
        return;
      }
      Stats::Utility::counterFromStatNames(*parent_.scope(),
                                           {parent_.context_->stat_namespace_, metric}, tags)
          .add(amount);
    }

    void recordHistogram(Stats::StatName metric, Stats::Histogram::Unit unit,
                         const Stats::StatNameTagVector& tags, uint64_t value) {
      if (parent_.metric_overrides_) {
        if (parent_.metric_overrides_->drop_.contains(metric)) {
          return;
        }
        auto new_tags = parent_.metric_overrides_->overrideTags(metric, tags, expr_values_);
        Stats::Utility::histogramFromStatNames(
            *parent_.scope(), {parent_.context_->stat_namespace_, metric}, unit, new_tags)
            .recordValue(value);
        return;
      }
      Stats::Utility::histogramFromStatNames(
          *parent_.scope(), {parent_.context_->stat_namespace_, metric}, unit, tags)
          .recordValue(value);
    }

    void recordCustomMetrics() {
      if (parent_.metric_overrides_) {
        for (const auto& [_, metric] : parent_.metric_overrides_->custom_metrics_) {
          const auto tags = parent_.metric_overrides_->overrideTags(metric.name_, {}, expr_values_);
          uint64_t amount = expr_values_[metric.expr_].second;
          switch (metric.type_) {
          case MetricOverrides::MetricType::Counter:
            Stats::Utility::counterFromStatNames(
                *parent_.scope(), {parent_.context_->stat_namespace_, metric.name_}, tags)
                .add(amount);
            break;
          case MetricOverrides::MetricType::Histogram:
            Stats::Utility::histogramFromStatNames(
                *parent_.scope(), {parent_.context_->stat_namespace_, metric.name_},
                Stats::Histogram::Unit::Bytes, tags)
                .recordValue(amount);
            break;
          case MetricOverrides::MetricType::Gauge:
            Stats::Utility::gaugeFromStatNames(*parent_.scope(),
                                               {parent_.context_->stat_namespace_, metric.name_},
                                               Stats::Gauge::ImportMode::Accumulate, tags)
                .set(amount);
            break;
          default:
            break;
          }
        }
      }
    }

    Config& parent_;
    std::vector<std::pair<Stats::StatName, uint64_t>> expr_values_;
  };

  void recordVersion() {
    Stats::StatNameTagVector tags;
    tags.push_back({context_->component_, context_->proxy_});
    tags.push_back({context_->tag_, context_->istio_version_.empty() ? context_->unknown_
                                                                     : context_->istio_version_});

    Stats::Utility::gaugeFromStatNames(*scope(),
                                       {context_->stat_namespace_, context_->istio_build_},
                                       Stats::Gauge::ImportMode::Accumulate, tags)
        .set(1);
  }

  Reporter reporter() const { return reporter_; }
  Stats::Scope* scope() { return scope_.scope(); }

  ContextSharedPtr context_;
  RotatingScope scope_;
  Reporter reporter_;

  const bool disable_host_header_fallback_;
  const std::chrono::milliseconds report_duration_;
  std::unique_ptr<MetricOverrides> metric_overrides_;
};

using ConfigSharedPtr = std::shared_ptr<Config>;

class IstioStatsFilter : public Http::PassThroughFilter,
                         public AccessLog::Instance,
                         public Network::ReadFilter,
                         public Network::ConnectionCallbacks {
public:
  IstioStatsFilter(ConfigSharedPtr config)
      : config_(config), context_(*config->context_), pool_(config->scope()->symbolTable()) {
    tags_.reserve(25);
    switch (config_->reporter()) {
    case Reporter::ServerSidecar:
    case Reporter::ServerGateway:
      tags_.push_back({context_.reporter_, context_.destination_});
      break;
    case Reporter::ClientSidecar:
      tags_.push_back({context_.reporter_, context_.source_});
      break;
    }
  }
  ~IstioStatsFilter() { ASSERT(report_timer_ == nullptr); }

  // Http::StreamDecoderFilter
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& request_headers, bool) override {
    is_grpc_ = Grpc::Common::isGrpcRequestHeaders(request_headers);
    if (is_grpc_) {
      report_timer_ = decoder_callbacks_->dispatcher().createTimer([this] { onReportTimer(); });
      report_timer_->enableTimer(config_->report_duration_);
    }
    return Http::FilterHeadersStatus::Continue;
  }

  // AccessLog::Instance
  void log(const Http::RequestHeaderMap* request_headers,
           const Http::ResponseHeaderMap* response_headers,
           const Http::ResponseTrailerMap* response_trailers, const StreamInfo::StreamInfo& info,
           AccessLog::AccessLogType) override {
    reportHelper(true);
    if (is_grpc_) {
      tags_.push_back({context_.request_protocol_, context_.grpc_});
    } else {
      tags_.push_back({context_.request_protocol_, context_.http_});
    }

    // TODO: copy Http::CodeStatsImpl version for status codes and flags.
    tags_.push_back(
        {context_.response_code_, pool_.add(absl::StrCat(info.responseCode().value_or(0)))});
    if (is_grpc_) {
      auto const& optional_status = Grpc::Common::getGrpcStatus(
          response_trailers ? *response_trailers
                            : *Http::StaticEmptyHeaders::get().response_trailers,
          response_headers ? *response_headers : *Http::StaticEmptyHeaders::get().response_headers,
          info);
      tags_.push_back(
          {context_.grpc_response_status_,
           optional_status ? pool_.add(absl::StrCat(optional_status.value())) : context_.empty_});
    } else {
      tags_.push_back({context_.grpc_response_status_, context_.empty_});
    }
    populateFlagsAndConnectionSecurity(info);

    Config::StreamOverrides stream(*config_, pool_, info, request_headers, response_headers,
                                   response_trailers);
    stream.addCounter(context_.requests_total_, tags_);
    auto duration = info.requestComplete();
    if (duration.has_value()) {
      stream.recordHistogram(context_.request_duration_milliseconds_,
                             Stats::Histogram::Unit::Milliseconds, tags_,
                             absl::FromChrono(duration.value()) / absl::Milliseconds(1));
    }
    auto meter = info.getDownstreamBytesMeter();
    if (meter) {
      stream.recordHistogram(context_.request_bytes_, Stats::Histogram::Unit::Bytes, tags_,
                             meter->wireBytesReceived());
      stream.recordHistogram(context_.response_bytes_, Stats::Histogram::Unit::Bytes, tags_,
                             meter->wireBytesSent());
    }
    stream.recordCustomMetrics();
  }

  // Network::ReadFilter
  Network::FilterStatus onData(Buffer::Instance&, bool) override {
    return Network::FilterStatus::Continue;
  }
  Network::FilterStatus onNewConnection() override {
    if (config_->report_duration_ > std::chrono::milliseconds(0)) {
      report_timer_ = network_read_callbacks_->connection().dispatcher().createTimer(
          [this] { onReportTimer(); });
      report_timer_->enableTimer(config_->report_duration_);
    }
    return Network::FilterStatus::Continue;
  }
  void initializeReadFilterCallbacks(Network::ReadFilterCallbacks& callbacks) override {
    network_read_callbacks_ = &callbacks;
    network_read_callbacks_->connection().addConnectionCallbacks(*this);
  }
  // Network::ConnectionCallbacks
  void onEvent(Network::ConnectionEvent event) override {
    switch (event) {
    case Network::ConnectionEvent::LocalClose:
    case Network::ConnectionEvent::RemoteClose:
      reportHelper(true);
      break;
    default:
      break;
    }
  }
  void onAboveWriteBufferHighWatermark() override {}
  void onBelowWriteBufferLowWatermark() override {}

private:
  // Invoked periodically for streams.
  void reportHelper(bool end_stream) {
    if (end_stream && report_timer_) {
      report_timer_->disableTimer();
      report_timer_.reset();
    }
    // HTTP handled first.
    if (decoder_callbacks_) {
      if (!peer_read_) {
        const auto& info = decoder_callbacks_->streamInfo();
        peer_read_ = peerInfoRead(config_->reporter(), info.filterState());
        if (peer_read_ || end_stream) {
          populatePeerInfo(info, info.filterState());
        }
      }
      if (is_grpc_ && (peer_read_ || end_stream)) {
        const auto* counters =
            decoder_callbacks_->streamInfo()
                .filterState()
                ->getDataReadOnly<GrpcStats::GrpcStatsObject>("envoy.filters.http.grpc_stats");
        if (counters) {
          Config::StreamOverrides stream(*config_, pool_, decoder_callbacks_->streamInfo());
          stream.addCounter(context_.request_messages_total_, tags_,
                            counters->request_message_count - request_message_count_);
          stream.addCounter(context_.response_messages_total_, tags_,
                            counters->response_message_count - response_message_count_);
          request_message_count_ = counters->request_message_count;
          response_message_count_ = counters->response_message_count;
        }
      }
      return;
    }
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

    Config::StreamOverrides stream(*config_, pool_, info);
    if (!peer_read_) {
      peer_read_ = peerInfoRead(config_->reporter(), filter_state);
      // Report connection open once peer info is read or connection is closed.
      if (peer_read_ || end_stream) {
        populatePeerInfo(info, filter_state);
        tags_.push_back({context_.request_protocol_, context_.tcp_});
        populateFlagsAndConnectionSecurity(info);
        stream.addCounter(context_.tcp_connections_opened_total_, tags_);
      }
    }
    if (peer_read_ || end_stream) {
      auto meter = info.getDownstreamBytesMeter();
      if (meter) {
        stream.addCounter(context_.tcp_sent_bytes_total_, tags_,
                          meter->wireBytesSent() - bytes_sent_);
        bytes_sent_ = meter->wireBytesSent();
        stream.addCounter(context_.tcp_received_bytes_total_, tags_,
                          meter->wireBytesReceived() - bytes_received_);
        bytes_received_ = meter->wireBytesReceived();
      }
    }
    if (end_stream) {
      stream.addCounter(context_.tcp_connections_closed_total_, tags_);
      stream.recordCustomMetrics();
    }
  }
  void onReportTimer() {
    reportHelper(false);
    report_timer_->enableTimer(config_->report_duration_);
  }

  void populateFlagsAndConnectionSecurity(const StreamInfo::StreamInfo& info) {
    tags_.push_back(
        {context_.response_flags_, pool_.add(StreamInfo::ResponseFlagUtils::toShortString(info))});
    tags_.push_back({context_.connection_security_policy_,
                     mutual_tls_.has_value()
                         ? (*mutual_tls_ ? context_.mutual_tls_ : context_.none_)
                         : context_.unknown_});
  }

  // Peer metadata is populated after encode/decodeHeaders by MX HTTP filter,
  // and after initial bytes read/written by MX TCP filter.
  void populatePeerInfo(const StreamInfo::StreamInfo& info,
                        const StreamInfo::FilterState& filter_state) {
    // Compute peer info with client-side fallbacks.
    absl::optional<Istio::Common::WorkloadMetadataObject> peer;
    const auto* object = peerInfo(config_->reporter(), filter_state);
    if (object) {
      peer.emplace(Istio::Common::convertFlatNodeToWorkloadMetadata(*object));
    } else if (config_->reporter() == Reporter::ClientSidecar) {
      if (auto label_obj = extractEndpointMetadata(info); label_obj) {
        peer.emplace(label_obj.value());
      }
    }

    // Compute destination service with client-side fallbacks.
    absl::string_view service_host;
    absl::string_view service_host_name;
    if (!config_->disable_host_header_fallback_) {
      const auto* headers = info.getRequestHeaders();
      if (headers && headers->Host()) {
        service_host = headers->Host()->value().getStringView();
        service_host_name = service_host;
      }
    }
    if (info.getRouteName() == "block_all") {
      service_host_name = "BlackHoleCluster";
    } else if (info.getRouteName() == "allow_any") {
      service_host_name = "PassthroughCluster";
    } else {
      const auto cluster_info = info.upstreamClusterInfo();
      if (cluster_info && cluster_info.value()) {
        const auto& cluster_name = cluster_info.value()->name();
        if (cluster_name == "BlackHoleCluster" || cluster_name == "PassthroughCluster" ||
            cluster_name == "InboundPassthroughClusterIpv4" ||
            cluster_name == "InboundPassthroughClusterIpv6") {
          service_host_name = cluster_name;
        } else {
          const auto& filter_metadata = cluster_info.value()->metadata().filter_metadata();
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
                }
                const auto& name_it = service.find("name");
                if (name_it != service.end()) {
                  service_host_name = name_it->second.string_value();
                } else {
                  service_host_name = service_host.substr(0, service_host.find_first_of('.'));
                }
              }
            }
          }
        }
      }
    }

    absl::string_view peer_san;
    absl::string_view local_san;
    switch (config_->reporter()) {
    case Reporter::ServerSidecar:
    case Reporter::ServerGateway: {
      auto peer_principal = info.filterState().getDataReadOnly<Router::StringAccessor>(
          Extensions::Common::PeerPrincipalKey);
      auto local_principal = info.filterState().getDataReadOnly<Router::StringAccessor>(
          Extensions::Common::LocalPrincipalKey);
      peer_san = peer_principal ? peer_principal->asString() : "";
      local_san = local_principal ? local_principal->asString() : "";

      // This fallback should be deleted once istio_authn is globally enabled.
      if (peer_san.empty() && local_san.empty()) {
        const Ssl::ConnectionInfoConstSharedPtr ssl_info =
            info.downstreamAddressProvider().sslConnection();
        if (ssl_info && !ssl_info->uriSanPeerCertificate().empty()) {
          peer_san = ssl_info->uriSanPeerCertificate()[0];
        }
        if (ssl_info && !ssl_info->uriSanLocalCertificate().empty()) {
          local_san = ssl_info->uriSanLocalCertificate()[0];
        }
      }

      // Save the connection security policy for a tag added later.
      mutual_tls_ = !peer_san.empty() && !local_san.empty();
      break;
    }
    case Reporter::ClientSidecar: {
      const Ssl::ConnectionInfoConstSharedPtr ssl_info =
          info.upstreamInfo() ? info.upstreamInfo()->upstreamSslConnection() : nullptr;
      if (ssl_info && !ssl_info->uriSanPeerCertificate().empty()) {
        peer_san = ssl_info->uriSanPeerCertificate()[0];
      }
      if (ssl_info && !ssl_info->uriSanLocalCertificate().empty()) {
        local_san = ssl_info->uriSanLocalCertificate()[0];
      }
      break;
    }
    }
    // Implements fallback from using the namespace from SAN if available to
    // using peer metadata, otherwise.
    absl::string_view peer_namespace;
    if (!peer_san.empty()) {
      const auto san_namespace = Utils::GetNamespace(peer_san);
      if (san_namespace) {
        peer_namespace = san_namespace.value();
      }
    }
    if (peer_namespace.empty() && peer) {
      peer_namespace = peer->namespace_name_;
    }
    switch (config_->reporter()) {
    case Reporter::ServerSidecar:
    case Reporter::ServerGateway: {
      tags_.push_back({context_.source_workload_, peer && !peer->workload_name_.empty()
                                                      ? pool_.add(peer->workload_name_)
                                                      : context_.unknown_});
      tags_.push_back({context_.source_canonical_service_, peer && !peer->canonical_name_.empty()
                                                               ? pool_.add(peer->canonical_name_)
                                                               : context_.unknown_});
      tags_.push_back(
          {context_.source_canonical_revision_, peer && !peer->canonical_revision_.empty()
                                                    ? pool_.add(peer->canonical_revision_)
                                                    : context_.latest_});
      tags_.push_back({context_.source_workload_namespace_,
                       !peer_namespace.empty() ? pool_.add(peer_namespace) : context_.unknown_});
      tags_.push_back({context_.source_principal_,
                       !peer_san.empty() ? pool_.add(peer_san) : context_.unknown_});
      tags_.push_back({context_.source_app_, peer && !peer->app_name_.empty()
                                                 ? pool_.add(peer->app_name_)
                                                 : context_.unknown_});
      tags_.push_back({context_.source_version_, peer && !peer->app_version_.empty()
                                                     ? pool_.add(peer->app_version_)
                                                     : context_.unknown_});
      tags_.push_back({context_.source_cluster_, peer && !peer->cluster_name_.empty()
                                                     ? pool_.add(peer->cluster_name_)
                                                     : context_.unknown_});
      switch (config_->reporter()) {
      case Reporter::ServerGateway: {
        std::optional<Istio::Common::WorkloadMetadataObject> endpoint_peer;
        const auto* endpoint_object = peerInfo(Reporter::ClientSidecar, filter_state);
        if (endpoint_object) {
          endpoint_peer.emplace(Istio::Common::convertFlatNodeToWorkloadMetadata(*endpoint_object));
        }
        tags_.push_back(
            {context_.destination_workload_,
             endpoint_peer ? pool_.add(endpoint_peer->workload_name_) : context_.unknown_});
        tags_.push_back({context_.destination_workload_namespace_, context_.namespace_});
        tags_.push_back({context_.destination_principal_,
                         !local_san.empty() ? pool_.add(local_san) : context_.unknown_});
        // Endpoint encoding does not have app and version.
        tags_.push_back({context_.destination_app_, context_.unknown_});
        tags_.push_back({context_.destination_version_, context_.unknown_});
        auto canonical_name =
            endpoint_peer ? pool_.add(endpoint_peer->canonical_name_) : context_.unknown_;
        tags_.push_back({context_.destination_service_,
                         service_host.empty() ? canonical_name : pool_.add(service_host)});
        tags_.push_back({context_.destination_canonical_service_, canonical_name});
        tags_.push_back(
            {context_.destination_canonical_revision_,
             endpoint_peer ? pool_.add(endpoint_peer->canonical_revision_) : context_.unknown_});
        tags_.push_back({context_.destination_service_name_, service_host_name.empty()
                                                                 ? canonical_name
                                                                 : pool_.add(service_host_name)});
        break;
      }
      default:
        tags_.push_back({context_.destination_workload_, context_.workload_name_});
        tags_.push_back({context_.destination_workload_namespace_, context_.namespace_});
        tags_.push_back({context_.destination_principal_,
                         !local_san.empty() ? pool_.add(local_san) : context_.unknown_});
        tags_.push_back({context_.destination_app_, context_.app_name_});
        tags_.push_back({context_.destination_version_, context_.app_version_});
        tags_.push_back({context_.destination_service_, service_host.empty()
                                                            ? context_.canonical_name_
                                                            : pool_.add(service_host)});
        tags_.push_back({context_.destination_canonical_service_, context_.canonical_name_});
        tags_.push_back({context_.destination_canonical_revision_, context_.canonical_revision_});
        tags_.push_back({context_.destination_service_name_, service_host_name.empty()
                                                                 ? context_.canonical_name_
                                                                 : pool_.add(service_host_name)});
        break;
      }
      tags_.push_back({context_.destination_service_namespace_, context_.namespace_});
      tags_.push_back({context_.destination_cluster_, context_.cluster_name_});

      break;
    }
    case Reporter::ClientSidecar: {
      tags_.push_back({context_.source_workload_, context_.workload_name_});
      tags_.push_back({context_.source_canonical_service_, context_.canonical_name_});
      tags_.push_back({context_.source_canonical_revision_, context_.canonical_revision_});
      tags_.push_back({context_.source_workload_namespace_, context_.namespace_});
      tags_.push_back({context_.source_principal_,
                       !local_san.empty() ? pool_.add(local_san) : context_.unknown_});
      tags_.push_back({context_.source_app_, context_.app_name_});
      tags_.push_back({context_.source_version_, context_.app_version_});
      tags_.push_back({context_.source_cluster_, context_.cluster_name_});
      tags_.push_back({context_.destination_workload_, peer && !peer->workload_name_.empty()
                                                           ? pool_.add(peer->workload_name_)
                                                           : context_.unknown_});
      tags_.push_back({context_.destination_workload_namespace_,
                       !peer_namespace.empty() ? pool_.add(peer_namespace) : context_.unknown_});
      tags_.push_back({context_.destination_principal_,
                       !peer_san.empty() ? pool_.add(peer_san) : context_.unknown_});
      tags_.push_back({context_.destination_app_, peer && !peer->app_name_.empty()
                                                      ? pool_.add(peer->app_name_)
                                                      : context_.unknown_});
      tags_.push_back({context_.destination_version_, peer && !peer->app_version_.empty()
                                                          ? pool_.add(peer->app_version_)
                                                          : context_.unknown_});
      tags_.push_back({context_.destination_service_,
                       service_host.empty() ? context_.unknown_ : pool_.add(service_host)});
      tags_.push_back({context_.destination_canonical_service_,
                       peer && !peer->canonical_name_.empty() ? pool_.add(peer->canonical_name_)
                                                              : context_.unknown_});
      tags_.push_back(
          {context_.destination_canonical_revision_, peer && !peer->canonical_revision_.empty()
                                                         ? pool_.add(peer->canonical_revision_)
                                                         : context_.latest_});
      tags_.push_back({context_.destination_service_name_, service_host_name.empty()
                                                               ? context_.unknown_
                                                               : pool_.add(service_host_name)});
      tags_.push_back({context_.destination_service_namespace_,
                       !peer_namespace.empty() ? pool_.add(peer_namespace) : context_.unknown_});
      tags_.push_back({context_.destination_cluster_, peer && !peer->cluster_name_.empty()
                                                          ? pool_.add(peer->cluster_name_)
                                                          : context_.unknown_});
      break;
    }
    default:
      break;
    }
  }

  ConfigSharedPtr config_;
  Context& context_;
  Stats::StatNameDynamicPool pool_;
  Stats::StatNameTagVector tags_;
  Event::TimerPtr report_timer_{nullptr};
  Network::ReadFilterCallbacks* network_read_callbacks_;
  bool peer_read_{false};
  uint64_t bytes_sent_{0};
  uint64_t bytes_received_{0};
  absl::optional<bool> mutual_tls_;
  bool is_grpc_{false};
  uint64_t request_message_count_{0};
  uint64_t response_message_count_{0};
};

} // namespace

Http::FilterFactoryCb IstioStatsFilterConfigFactory::createFilterFactoryFromProtoTyped(
    const stats::PluginConfig& proto_config, const std::string&,
    Server::Configuration::FactoryContext& factory_context) {
  factory_context.api().customStatNamespaces().registerStatNamespace(CustomStatNamespace);
  ConfigSharedPtr config = std::make_shared<Config>(proto_config, factory_context);
  config->recordVersion();
  return [config](Http::FilterChainFactoryCallbacks& callbacks) {
    auto filter = std::make_shared<IstioStatsFilter>(config);
    callbacks.addStreamFilter(filter);
    // Wasm filters inject filter state in access log handlers, which are called
    // after onStreamComplete.
    callbacks.addAccessLogHandler(filter);
  };
}

REGISTER_FACTORY(IstioStatsFilterConfigFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory);

Network::FilterFactoryCb IstioStatsNetworkFilterConfigFactory::createFilterFactoryFromProtoTyped(
    const stats::PluginConfig& proto_config,
    Server::Configuration::FactoryContext& factory_context) {
  factory_context.api().customStatNamespaces().registerStatNamespace(CustomStatNamespace);
  ConfigSharedPtr config = std::make_shared<Config>(proto_config, factory_context);
  config->recordVersion();
  return [config](Network::FilterManager& filter_manager) {
    filter_manager.addReadFilter(std::make_shared<IstioStatsFilter>(config));
  };
}

REGISTER_FACTORY(IstioStatsNetworkFilterConfigFactory,
                 Server::Configuration::NamedNetworkFilterConfigFactory);

} // namespace IstioStats
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
