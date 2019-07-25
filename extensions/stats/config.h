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

#include "absl/container/flat_hash_map.h"
#include "extensions/common/wasm/null/null.h"

namespace Envoy {
namespace Extensions {
namespace Wasm {
namespace Plugin {
namespace Stats {

constexpr absl::string_view ExchangeMetadataHeaderId =
    "x-envoy-peer-metadata-id";

// NodeMetadata key is the key in the node metadata struct that is passed
// between peers.
constexpr absl::string_view NodeMetadataKey = "istio.io/metadata";
constexpr absl::string_view NodeIdKey = "id";
constexpr absl::string_view WholeNodeKey = ".";

// DownstreamMetadataKey is the key in the request metadata for downstream peer
// metadata
constexpr absl::string_view DownstreamMetadataKey =
    "envoy.wasm.metadata_exchange.downstream";
constexpr absl::string_view DownstreamMetadataIdKey =
    "envoy.wasm.metadata_exchange.downstream_id";

// UpstreamMetadataKey is the key in the request metadata for downstream peer
// metadata
constexpr absl::string_view UpstreamMetadataKey =
    "envoy.wasm.metadata_exchange.upstream";
constexpr absl::string_view UpstreamMetadataIdKey =
    "envoy.wasm.metadata_exchange.upstream_id";

using StringView = absl::string_view;
using Common::Wasm::MetadataType;
using Common::Wasm::Null::NullVmPluginRootRegistry;
using Common::Wasm::Null::Plugin::Context;
using Common::Wasm::Null::Plugin::ContextFactory;
using Common::Wasm::Null::Plugin::Counter;
using Common::Wasm::Null::Plugin::Metric;
using Common::Wasm::Null::Plugin::MetricTag;
using Common::Wasm::Null::Plugin::MetricType;
using Common::Wasm::Null::Plugin::RootContext;
using Common::Wasm::Null::Plugin::RootFactory;
using Common::Wasm::Null::Plugin::SimpleCounter;
using Common::Wasm::Null::Plugin::WasmData;

// PluginRootContext is the root context for all streams processed by the
// thread. It has the same lifetime as the worker thread and acts as target for
// interactions that outlives individual stream, e.g. timer, async calls.
class PluginRootContext : public RootContext {
 public:
  PluginRootContext(uint32_t id, StringView root_id)
      : RootContext(id, root_id),
        istio_requests_total_metric_(
            Counter<std::string, std::string, std::string, std::string>::New(
                "istio_requests_total", "source_app", "source_version",
                "destination_app", "destination_version")) {}
  ~PluginRootContext() = default;

  void onConfigure(std::unique_ptr<WasmData>) override;
  void onStart() override{};
  void onTick() override{};

  void onLog() {
    std::string aa = "id1";

    auto counter_it = counter_map_.find(aa);
    if (counter_it == counter_map_.end()) {
      auto counter = istio_requests_total_metric_->resolve("SRC_A", "SRC_V",
                                                           "DEST_A", "DEST_B");
      counter_map_.emplace(aa, counter);
      counter++;
    } else {
      counter_it->second++;
    }
  }

 private:
  Counter<std::string, std::string, std::string, std::string>*
      istio_requests_total_metric_;
  absl::flat_hash_map<std::string, SimpleCounter> counter_map_;
};

// Per-stream context.
class PluginContext : public Context {
 public:
  explicit PluginContext(uint32_t id, RootContext* root) : Context(id, root) {}

  void onCreate() override{};
  void onLog() override { rootContext()->onLog(); };

 private:
  inline PluginRootContext* rootContext() {
    return dynamic_cast<PluginRootContext*>(this->root());
  };
};

NULL_PLUGIN_ROOT_REGISTRY;

static RegisterContextFactory register_Stats(CONTEXT_FACTORY(PluginContext),
                                             ROOT_FACTORY(PluginRootContext));

}  // namespace Stats
}  // namespace Plugin
}  // namespace Wasm
}  // namespace Extensions
}  // namespace Envoy
