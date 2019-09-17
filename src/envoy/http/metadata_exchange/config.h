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

#include "extensions/common/wasm/null/null_plugin.h"

namespace Envoy {
namespace Extensions {
namespace Wasm {
namespace MetadataExchange {

constexpr absl::string_view ExchangeMetadataHeader = "x-envoy-peer-metadata";
constexpr absl::string_view ExchangeMetadataHeaderId =
    "x-envoy-peer-metadata-id";

constexpr absl::string_view NodeMetadataExchangeKeys = "EXCHANGE_KEYS";
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
using Common::Wasm::WasmResult;
using Common::Wasm::Null::NullPluginRootRegistry;
using Common::Wasm::Null::Plugin::Context;
using Common::Wasm::Null::Plugin::ContextFactory;
using Common::Wasm::Null::Plugin::RootContext;
using Common::Wasm::Null::Plugin::RootFactory;
using Common::Wasm::Null::Plugin::WasmData;

// PluginRootContext is the root context for all streams processed by the
// thread. It has the same lifetime as the worker thread and acts as target for
// interactions that outlives individual stream, e.g. timer, async calls.
class PluginRootContext : public RootContext {
 public:
  PluginRootContext(uint32_t id, StringView root_id)
      : RootContext(id, root_id) {}
  ~PluginRootContext() = default;

  void onConfigure(std::unique_ptr<WasmData>) override;
  void onStart(std::unique_ptr<WasmData>) override{};
  void onTick() override{};

  StringView metadataValue() { return metadata_value_; };
  StringView nodeId() { return node_id_; };

 private:
  void updateMetadataValue();
  std::string metadata_value_;
  std::string node_id_;
};

// Per-stream context.
class PluginContext : public Context {
 public:
  explicit PluginContext(uint32_t id, RootContext* root) : Context(id, root) {}

  void onCreate() override{};
  Http::FilterHeadersStatus onRequestHeaders() override;
  Http::FilterHeadersStatus onResponseHeaders() override;

 private:
  inline PluginRootContext* rootContext() {
    return dynamic_cast<PluginRootContext*>(this->root());
  };
  inline StringView metadataValue() { return rootContext()->metadataValue(); };
  inline StringView nodeId() { return rootContext()->nodeId(); }
};

NULL_PLUGIN_ROOT_REGISTRY;

static RegisterContextFactory register_MetadataExchange(
    CONTEXT_FACTORY(PluginContext), ROOT_FACTORY(PluginRootContext));

}  // namespace MetadataExchange
}  // namespace Wasm
}  // namespace Extensions
}  // namespace Envoy
