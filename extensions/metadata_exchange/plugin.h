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

#include "extensions/common/context.h"

#ifndef NULL_PLUGIN

#include <assert.h>
#define ASSERT(_X) assert(_X)

#include "proxy_wasm_intrinsics.h"

static const std::string EMPTY_STRING;

#else

#include "extensions/common/wasm/null/null_plugin.h"

namespace Envoy {
namespace Extensions {
namespace Wasm {
namespace MetadataExchange {
namespace Plugin {

using namespace Envoy::Extensions::Common::Wasm::Null::Plugin;

// TODO(jplevyak): move these into the base envoy repo
using WasmResult = Envoy::Extensions::Common::Wasm::WasmResult;
using NullPluginRegistry =
    ::Envoy::Extensions::Common::Wasm::Null::NullPluginRegistry;

#endif

constexpr StringView ExchangeMetadataHeader = "x-envoy-peer-metadata";
constexpr StringView ExchangeMetadataHeaderId = "x-envoy-peer-metadata-id";
const size_t DefaultNodeCacheMaxSize = 500;

// PluginRootContext is the root context for all streams processed by the
// thread. It has the same lifetime as the worker thread and acts as target for
// interactions that outlives individual stream, e.g. timer, async calls.
class PluginRootContext : public RootContext {
 public:
  PluginRootContext(uint32_t id, StringView root_id)
      : RootContext(id, root_id) {}
  ~PluginRootContext() = default;

  bool onConfigure(size_t) override;
  bool configure(size_t);
  bool onStart(size_t) override { return true; };
  void onTick() override{};

  StringView metadataValue() { return metadata_value_; };
  StringView nodeId() { return node_id_; };
  bool updatePeer(StringView key, StringView peer_id, StringView peer_header);

 private:
  void updateMetadataValue();
  std::string metadata_value_;
  std::string node_id_;

  // maps peer ID to the decoded peer flat buffer
  std::unordered_map<std::string, std::string> cache_;
  int64_t max_peer_cache_size_{DefaultNodeCacheMaxSize};
};

// Per-stream context.
class PluginContext : public Context {
 public:
  explicit PluginContext(uint32_t id, RootContext* root) : Context(id, root) {
    direction_ = ::Wasm::Common::getTrafficDirection();
  }

  void onCreate() override{};
  FilterHeadersStatus onRequestHeaders(uint32_t) override;
  FilterHeadersStatus onResponseHeaders(uint32_t) override;

 private:
  inline PluginRootContext* rootContext() {
    return dynamic_cast<PluginRootContext*>(this->root());
  };
  inline StringView metadataValue() { return rootContext()->metadataValue(); };
  inline StringView nodeId() { return rootContext()->nodeId(); }

  ::Wasm::Common::TrafficDirection direction_;
  bool metadata_received_{true};
  bool metadata_id_received_{true};
};

#ifdef NULL_PLUGIN
}  // namespace Plugin
}  // namespace MetadataExchange
}  // namespace Wasm
}  // namespace Extensions
}  // namespace Envoy
#endif
