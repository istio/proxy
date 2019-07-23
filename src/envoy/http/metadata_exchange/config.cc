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

#include "src/envoy/http/metadata_exchange/config.h"
#include "common/common/base64.h"

namespace Envoy {
namespace Extensions {
namespace Wasm {
namespace MetadataExchange {

// imports from the low-level API
using Common::Wasm::Null::NullVmPluginFactory;
using Common::Wasm::Null::Plugin::getMetadataStringValue;
using Common::Wasm::Null::Plugin::getMetadataStruct;
using Common::Wasm::Null::Plugin::getMetadataValue;
using Common::Wasm::Null::Plugin::getRequestHeader;
using Common::Wasm::Null::Plugin::getResponseHeader;
using Common::Wasm::Null::Plugin::logDebug;
using Common::Wasm::Null::Plugin::logInfo;
using Common::Wasm::Null::Plugin::logWarn;
using Common::Wasm::Null::Plugin::removeRequestHeader;
using Common::Wasm::Null::Plugin::removeResponseHeader;
using Common::Wasm::Null::Plugin::replaceRequestHeader;
using Common::Wasm::Null::Plugin::replaceResponseHeader;
using Common::Wasm::Null::Plugin::setMetadataStringValue;

void PluginRootContext::onConfigure(
    std::unique_ptr<WasmData> ABSL_ATTRIBUTE_UNUSED configuration) {
  auto metadata =
      getMetadataValue(Common::Wasm::MetadataType::Node, NodeMetadataKey);
  if (metadata.kind_case() == google::protobuf::Value::kStructValue) {
    std::string metadata_bytes;
    google::protobuf::io::StringOutputStream md(&metadata_bytes);
    google::protobuf::io::CodedOutputStream mcs(&md);

    mcs.SetSerializationDeterministic(true);
    if (!metadata.struct_value().SerializeToCodedStream(&mcs)) {
      logWarn(absl::StrCat("unable to serialize Nodemetadata key=",
                           NodeMetadataKey));
    }
    mcs.Trim();  // trim buffer to the actual size needed.

    metadata_value_ =
        Base64::encode(metadata_bytes.data(), metadata_bytes.size());

    // magic "." to get the whole node.
    auto node =
        getMetadataStruct(Common::Wasm::MetadataType::Node, WholeNodeKey);
    for (const auto& f : node.fields()) {
      if (f.first == NodeIdKey &&
          f.second.kind_case() == google::protobuf::Value::kStringValue) {
        node_id_ = f.second.string_value();
        break;
      }
    }
  }

  logDebug(
      absl::StrCat("metadata_value_ id:", id(), " value:", metadata_value_));
}

Http::FilterHeadersStatus PluginContext::onRequestHeaders() {
  // strip and store downstream peer metadata
  auto downstream_metadata_value = getRequestHeader(ExchangeMetadataHeader);
  if (downstream_metadata_value != nullptr &&
      !downstream_metadata_value->view().empty()) {
    removeRequestHeader(ExchangeMetadataHeader);
    auto downstream_metadata_bytes =
        Base64::decodeWithoutPadding(downstream_metadata_value->view());
    setMetadataStruct(Common::Wasm::MetadataType::Request,
                      DownstreamMetadataKey, downstream_metadata_bytes);
  }

  auto downstream_metadata_id = getRequestHeader(ExchangeMetadataHeaderId);
  if (downstream_metadata_id != nullptr &&
      !downstream_metadata_id->view().empty()) {
    removeRequestHeader(ExchangeMetadataHeaderId);
    setMetadataStringValue(Common::Wasm::MetadataType::Request,
                           DownstreamMetadataIdKey,
                           downstream_metadata_id->view());
  }

  auto metadata = metadataValue();
  // insert peer metadata struct for upstream
  if (!metadata.empty()) {
    replaceRequestHeader(ExchangeMetadataHeader, metadata);
  }

  auto nodeid = nodeId();
  if (!nodeid.empty()) {
    replaceRequestHeader(ExchangeMetadataHeaderId, nodeid);
  }

  return Http::FilterHeadersStatus::Continue;
}

Http::FilterHeadersStatus PluginContext::onResponseHeaders() {
  // strip and store upstream peer metadata
  auto upstream_metadata_value = getResponseHeader(ExchangeMetadataHeader);
  if (upstream_metadata_value != nullptr &&
      !upstream_metadata_value->view().empty()) {
    removeResponseHeader(ExchangeMetadataHeader);
    auto upstream_metadata_bytes =
        Base64::decodeWithoutPadding(upstream_metadata_value->toString());
    setMetadataStruct(Common::Wasm::MetadataType::Request, UpstreamMetadataKey,
                      upstream_metadata_bytes);
  }

  auto upstream_metadata_id = getResponseHeader(ExchangeMetadataHeaderId);
  if (upstream_metadata_id != nullptr &&
      !upstream_metadata_id->view().empty()) {
    removeRequestHeader(ExchangeMetadataHeaderId);
    setMetadataStringValue(Common::Wasm::MetadataType::Request,
                           UpstreamMetadataIdKey, upstream_metadata_id->view());
  }

  auto metadata = metadataValue();
  // insert peer metadata struct for downstream
  if (!metadata.empty()) {
    replaceResponseHeader(ExchangeMetadataHeader, metadata);
  }

  auto nodeid = nodeId();
  if (!nodeid.empty()) {
    replaceResponseHeader(ExchangeMetadataHeaderId, nodeid);
  }

  return Http::FilterHeadersStatus::Continue;
}

inline void logRequestMetadata(StringView key) {
  auto val = getMetadataStringValue(Common::Wasm::MetadataType::Request, key);
  if (!val.empty()) {
    logDebug(absl::StrCat(key, "=", val));
  }
}

void PluginContext::onLog() {
  logRequestMetadata(UpstreamMetadataIdKey);
  logRequestMetadata(DownstreamMetadataIdKey);
};

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
