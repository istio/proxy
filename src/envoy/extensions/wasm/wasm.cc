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
#include "source/extensions/common/wasm/wasm.h"

#include "source/common/stats/utility.h"
#include "source/common/version/version.h"
#include "source/server/admin/prometheus_stats.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Wasm {
namespace Istio {
namespace {

struct ConfigStats {
  ConfigStats(Stats::SymbolTable& symbol_table)
      : stat_name_pool_(symbol_table) {}
  Stats::StatNamePool stat_name_pool_;
  // NB: Use pointers because references must be initialized in the
  // initialization list which then would then require that all the component
  // stat names to be member variables because stat_name_pool_.add() does not
  // dedup so we can not create them as temporaries.
  Stats::Counter* permanent_read_error_;
  Stats::Counter* eventually_consistent_read_;
  Stats::Counter* invalid_module_;
  Stats::Counter* invalid_configuration_;
};

}  // namespace

class IstioWasmExtension : public EnvoyWasm {
 public:
  IstioWasmExtension() {
    ::Envoy::Server::PrometheusStatsFormatter::registerPrometheusNamespace(
        "istio");
  }
  ~IstioWasmExtension() override = default;
  void onEvent(WasmEvent event, const PluginSharedPtr& plugin) override;
  void onRemoteCacheEntriesChanged(int remote_cache_entries) override;
  void createStats(const Stats::ScopeSharedPtr& scope,
                   const PluginSharedPtr& plugin) override;
  void resetStats() override;

 private:
  std::map<std::string, std::unique_ptr<ConfigStats>> config_stats_;
};

static std::string statsKey(const PluginSharedPtr& plugin) {
  auto sep = std::string("\t");
  return plugin->name_ + sep + plugin->runtime_;
}

void IstioWasmExtension::onEvent(WasmEvent event,
                                 const PluginSharedPtr& plugin) {
  EnvoyWasm::onEvent(event, plugin);
  auto key = statsKey(plugin);
  auto& stats = config_stats_.at(key);
  switch (event) {
    case EnvoyWasm::WasmEvent::Ok:
    case EnvoyWasm::WasmEvent::RemoteLoadCacheHit:
      break;
    case EnvoyWasm::WasmEvent::RemoteLoadCacheNegativeHit:
      stats->permanent_read_error_->inc();
      break;
    case EnvoyWasm::WasmEvent::RemoteLoadCacheMiss:
      stats->eventually_consistent_read_->inc();
      break;
    case EnvoyWasm::WasmEvent::RemoteLoadCacheFetchSuccess:
      break;
    case EnvoyWasm::WasmEvent::RemoteLoadCacheFetchFailure:
      stats->permanent_read_error_->inc();
      break;
    case EnvoyWasm::WasmEvent::UnableToCreateVM:
    case EnvoyWasm::WasmEvent::UnableToCloneVM:
    case EnvoyWasm::WasmEvent::MissingFunction:
    case EnvoyWasm::WasmEvent::UnableToInitializeCode:
      stats->invalid_module_->inc();
      break;
    case EnvoyWasm::WasmEvent::StartFailed:
    case EnvoyWasm::WasmEvent::ConfigureFailed:
      stats->invalid_configuration_->inc();
      break;
    case EnvoyWasm::WasmEvent::RuntimeError:
      break;
  }
}

void IstioWasmExtension::onRemoteCacheEntriesChanged(int entries) {
  EnvoyWasm::onRemoteCacheEntriesChanged(entries);
}

// NB: the "scope" here is tied to the lifetime of the filter chain in many
// cases and may disappear. Code in envoy detects that and will call
// resetStats().
void IstioWasmExtension::createStats(const Stats::ScopeSharedPtr& scope,
                                     const PluginSharedPtr& plugin) {
  EnvoyWasm::createStats(scope, plugin);
  std::string istio_version = Envoy::VersionInfo::version();
  auto node_metadata_fields = plugin->localInfo().node().metadata().fields();
  auto istio_version_it = node_metadata_fields.find("ISTIO_VERSION");
  if (istio_version_it != node_metadata_fields.end()) {
    istio_version = istio_version_it->second.string_value();
  }
  auto key = statsKey(plugin);
  if (config_stats_.find(key) == config_stats_.end()) {
    auto new_stats = std::make_unique<ConfigStats>(scope->symbolTable());
    auto& pool = new_stats->stat_name_pool_;
    auto prefix = pool.add("istio_wasm_config_errors_total");
    auto error_type = pool.add("error_type");
    auto plugin_name = pool.add("plugin_name");
    auto name = pool.add(plugin->name_);
    auto proxy_version = pool.add("proxy_version");
    auto version = pool.add(istio_version);
    auto vm = pool.add("vm");
    auto runtime = pool.add(plugin->runtime_);
    new_stats->permanent_read_error_ = &Stats::Utility::counterFromElements(
        *scope, {prefix, error_type, pool.add("permanent_read_errors"),
                 plugin_name, name, proxy_version, version, vm, runtime});
    new_stats->eventually_consistent_read_ =
        &Stats::Utility::counterFromElements(
            *scope, {prefix, error_type, pool.add("eventually_consistent_read"),
                     plugin_name, name, proxy_version, version, vm, runtime});
    new_stats->invalid_module_ = &Stats::Utility::counterFromElements(
        *scope, {prefix, error_type, pool.add("invalid_module"), plugin_name,
                 name, proxy_version, version, vm, runtime});
    new_stats->invalid_configuration_ = &Stats::Utility::counterFromElements(
        *scope, {prefix, error_type, pool.add("invalid_configuration"),
                 plugin_name, name, proxy_version, version, vm, runtime});
    config_stats_[key] = std::move(new_stats);
  }
}

void IstioWasmExtension::resetStats() {
  EnvoyWasm::resetStats();
  config_stats_.clear();
}

REGISTER_WASM_EXTENSION(IstioWasmExtension);

}  // namespace Istio
}  // namespace Wasm
}  // namespace Common
}  // namespace Extensions
}  // namespace Envoy
