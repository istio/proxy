#pragma once

#include "extensions/common/wasm/null/null.h"

namespace Envoy {
namespace Extensions {
namespace Wasm {
namespace MetadataExchange {

// Node ID is expected to be a primary key for the node metadata
constexpr char ExchangeIDHeader[] = "x-envoy-peer-id";
constexpr char ExchangeMetadataHeader[] = "x-envoy-peer-metadata";

// NodeMetadata key is the key in the node metadata struct that is passed
// between peers.
constexpr char NodeMetadataKey[] = "istio.io/metadata";

// DownstreamMetadataKey is the key in the request metadata for downstream peer
// metadata
constexpr char DownstreamMetadataKey[] =
    "envoy.wasm.metadata_exchange.downstream";

// UpstreamMetadataKey is the key in the request metadata for downstream peer
// metadata
constexpr char UpstreamMetadataKey[] = "envoy.wasm.metadata_exchange.upstream";

using StringView = absl::string_view;
using Common::Wasm::Null::NullVmPluginRootRegistry;
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

  void onConfigure(std::unique_ptr<WasmData>) override{};
  void onStart() override{};
  void onTick() override{};
};

// Per-stream context.
class PluginContext : public Context {
 public:
  explicit PluginContext(uint32_t id, RootContext* root) : Context(id, root) {}

  void onCreate() override;
  Http::FilterHeadersStatus onRequestHeaders() override;
  Http::FilterHeadersStatus onResponseHeaders() override;

 private:
  // pre-populated serialized values for passing to peers
  std::string id_value_;
  std::string metadata_value_;
};

NULL_PLUGIN_ROOT_REGISTRY;

static RegisterContextFactory register_MetadataExchange(
    CONTEXT_FACTORY(PluginContext), ROOT_FACTORY(PluginRootContext));

}  // namespace MetadataExchange
}  // namespace Wasm
}  // namespace Extensions
}  // namespace Envoy
