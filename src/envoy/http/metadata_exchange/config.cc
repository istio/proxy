#include "src/envoy/http/metadata_exchange/config.h"
#include "common/common/base64.h"

namespace Envoy {
namespace Extensions {
namespace Wasm {
namespace MetadataExchange {

// imports from the low-level API
using Common::Wasm::Null::NullVmPluginFactory;
using Common::Wasm::Null::Plugin::getMetadataValue;
using Common::Wasm::Null::Plugin::getRequestHeader;
using Common::Wasm::Null::Plugin::getResponseHeader;
using Common::Wasm::Null::Plugin::logDebug;
using Common::Wasm::Null::Plugin::logInfo;
using Common::Wasm::Null::Plugin::proxy_setMetadataStruct;
using Common::Wasm::Null::Plugin::removeRequestHeader;
using Common::Wasm::Null::Plugin::removeResponseHeader;
using Common::Wasm::Null::Plugin::replaceRequestHeader;
using Common::Wasm::Null::Plugin::replaceResponseHeader;

void PluginContext::onCreate() {
  // TODO: populate and encode node ID

  // populate and encode node metadata
  auto metadata =
      getMetadataValue(Common::Wasm::MetadataType::Node, NodeMetadataKey);
  if (metadata.kind_case() == google::protobuf::Value::kStructValue) {
    std::string metadata_bytes;
    metadata.struct_value().SerializeToString(&metadata_bytes);
    metadata_value_ =
        Base64::encode(metadata_bytes.data(), metadata_bytes.size());
  }

  logDebug("metadata_value_ id:" + std::to_string(id()) +
           " value:" + metadata_value_);
}

Http::FilterHeadersStatus PluginContext::onRequestHeaders() {
  // strip and store downstream peer metadata
  auto downstream_metadata_value = getRequestHeader(ExchangeMetadataHeader);
  if (downstream_metadata_value != nullptr &&
      downstream_metadata_value->view().size() > 0) {
    removeRequestHeader(ExchangeMetadataHeader);
    auto downstream_metadata_bytes =
        Base64::decode(downstream_metadata_value->toString());
    proxy_setMetadataStruct(
        Common::Wasm::MetadataType::Request, DownstreamMetadataKey,
        strlen(DownstreamMetadataKey), downstream_metadata_bytes.data(),
        downstream_metadata_bytes.size());
  }

  // insert peer metadata struct for upstream
  if (metadata_value_.size() > 0) {
    replaceRequestHeader(ExchangeMetadataHeader, metadata_value_);
  }

  return Http::FilterHeadersStatus::Continue;
}

Http::FilterHeadersStatus PluginContext::onResponseHeaders() {
  // strip and store upstream peer metadata
  auto upstream_metadata_value = getResponseHeader(ExchangeMetadataHeader);
  if (upstream_metadata_value != nullptr &&
      upstream_metadata_value->view().size() > 0) {
    removeResponseHeader(ExchangeMetadataHeader);
    auto upstream_metadata_bytes =
        Base64::decode(upstream_metadata_value->toString());
    proxy_setMetadataStruct(Common::Wasm::MetadataType::Request,
                            UpstreamMetadataKey, strlen(UpstreamMetadataKey),
                            upstream_metadata_bytes.data(),
                            upstream_metadata_bytes.size());
  }

  // insert peer metadata struct for downstream
  if (metadata_value_.size() > 0) {
    replaceResponseHeader(ExchangeMetadataHeader, metadata_value_);
  }

  return Http::FilterHeadersStatus::Continue;
}

// Registration glue

Common::Wasm::Null::NullVmPluginRootRegistry* context_registry_{};

class MetadataExchangeFactory : public Common::Wasm::Null::NullVmPluginFactory {
 public:
  const std::string name() const override {
    return "envoy.wasm.metadata_exchange";
  }
  std::unique_ptr<Common::Wasm::Null::NullVmPlugin> create() const override {
    return std::make_unique<Common::Wasm::Null::NullVmPlugin>(
        Envoy::Extensions::Wasm::MetadataExchange::context_registry_);
  }
};

static Registry::RegisterFactory<MetadataExchangeFactory, NullVmPluginFactory>
    register_;

}  // namespace MetadataExchange
}  // namespace Wasm
}  // namespace Extensions
}  // namespace Envoy
