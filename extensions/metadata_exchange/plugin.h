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

#include "include/proxy-wasm/null_plugin.h"

namespace proxy_wasm {
namespace null_plugin {
namespace MetadataExchange {
namespace Plugin {

#endif

constexpr std::string_view ExchangeMetadataHeader = "x-envoy-peer-metadata";
constexpr std::string_view ExchangeMetadataHeaderId = "x-envoy-peer-metadata-id";
const size_t DefaultNodeCacheMaxSize = 500;

// PluginRootContext is the root context for all streams processed by the
// thread. It has the same lifetime as the worker thread and acts as target for
// interactions that outlives individual stream, e.g. timer, async calls.
class PluginRootContext : public RootContext {
public:
  PluginRootContext(uint32_t id, std::string_view root_id) : RootContext(id, root_id) {}
  bool onConfigure(size_t) override;
  bool configure(size_t);

  std::string_view metadataValue() { return metadata_value_; };
  std::string_view nodeId() { return node_id_; };
  bool updatePeer(std::string_view key, std::string_view peer_id, std::string_view peer_header);

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

  FilterHeadersStatus onRequestHeaders(uint32_t, bool) override;
  FilterHeadersStatus onResponseHeaders(uint32_t, bool) override;

private:
  inline PluginRootContext* rootContext() {
    return dynamic_cast<PluginRootContext*>(this->root());
  };
  inline std::string_view metadataValue() { return rootContext()->metadataValue(); };
  inline std::string_view nodeId() { return rootContext()->nodeId(); }

  ::Wasm::Common::TrafficDirection direction_;
  bool metadata_received_{true};
  bool metadata_id_received_{true};
};

#ifdef NULL_PLUGIN
} // namespace Plugin
} // namespace MetadataExchange
} // namespace null_plugin
} // namespace proxy_wasm
#endif
