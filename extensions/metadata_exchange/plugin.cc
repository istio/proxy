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

#include "extensions/metadata_exchange/plugin.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "extensions/common/context.h"
#include "extensions/common/proto_util.h"
#include "extensions/common/util.h"
#include "extensions/common/wasm/json_util.h"

#ifndef NULL_PLUGIN

#include "extensions/common/wasm/base64.h"
#include "extensions/metadata_exchange/declare_property.pb.h"

#else

#include "source/common/common/base64.h"
#include "source/extensions/common/wasm/ext/declare_property.pb.h"

namespace proxy_wasm {
namespace null_plugin {
namespace MetadataExchange {
namespace Plugin {

PROXY_WASM_NULL_PLUGIN_REGISTRY;

using Base64 = Envoy::Base64;

#endif

static RegisterContextFactory register_MetadataExchange(CONTEXT_FACTORY(PluginContext),
                                                        ROOT_FACTORY(PluginRootContext));

void PluginRootContext::updateMetadataValue() {
  auto node_info = ::Wasm::Common::extractLocalNodeFlatBuffer();

  google::protobuf::Struct metadata;
  ::Wasm::Common::extractStructFromNodeFlatBuffer(
      *flatbuffers::GetRoot<::Wasm::Common::FlatNode>(node_info.data()), &metadata);

  std::string metadata_bytes;
  ::Wasm::Common::serializeToStringDeterministic(metadata, &metadata_bytes);
  metadata_value_ = Base64::encode(metadata_bytes.data(), metadata_bytes.size());
}

// Metadata exchange has sane defaults and therefore it will be fully
// functional even with configuration errors.
// A configuration error thrown here will cause the proxy to crash.
bool PluginRootContext::onConfigure(size_t size) {
  updateMetadataValue();
  if (!getValue({"node", "id"}, &node_id_)) {
    LOG_DEBUG("cannot get node ID");
  }
  LOG_DEBUG(
      absl::StrCat("metadata_value_ id:", id(), " value:", metadata_value_, " node:", node_id_));

  // Parse configuration JSON string.
  if (size > 0 && !configure(size)) {
    LOG_WARN("configuration has errrors, but initialzation can continue.");
  }

  // Declare filter state property type.
  const std::string function = "declare_property";
  envoy::source::extensions::common::wasm::DeclarePropertyArguments args;
  args.set_type(envoy::source::extensions::common::wasm::WasmType::FlatBuffers);
  args.set_span(envoy::source::extensions::common::wasm::LifeSpan::DownstreamRequest);
  args.set_schema(::Wasm::Common::nodeInfoSchema().data(), ::Wasm::Common::nodeInfoSchema().size());
  std::string in;
  args.set_name(std::string(::Wasm::Common::kUpstreamMetadataKey));
  args.SerializeToString(&in);
  proxy_call_foreign_function(function.data(), function.size(), in.data(), in.size(), nullptr,
                              nullptr);

  args.set_name(std::string(::Wasm::Common::kDownstreamMetadataKey));
  args.SerializeToString(&in);
  proxy_call_foreign_function(function.data(), function.size(), in.data(), in.size(), nullptr,
                              nullptr);

  return true;
}

bool PluginRootContext::configure(size_t configuration_size) {
  auto configuration_data =
      getBufferBytes(WasmBufferType::PluginConfiguration, 0, configuration_size);
  // Parse configuration JSON string.
  auto result = ::Wasm::Common::JsonParse(configuration_data->view());
  if (!result.has_value()) {
    LOG_WARN(absl::StrCat("cannot parse plugin configuration JSON string: ",
                          ::Wasm::Common::toAbslStringView(configuration_data->view())));
    return false;
  }

  auto j = result.value();
  auto max_peer_cache_size_field = ::Wasm::Common::JsonGetField<int64_t>(j, "max_peer_cache_size");
  if (max_peer_cache_size_field.detail() == Wasm::Common::JsonParserResultDetail::OK) {
    max_peer_cache_size_ = max_peer_cache_size_field.value();
  }
  return true;
}

bool PluginRootContext::updatePeer(std::string_view key, std::string_view peer_id,
                                   std::string_view peer_header) {
  std::string id = std::string(peer_id);
  if (max_peer_cache_size_ > 0) {
    auto it = cache_.find(id);
    if (it != cache_.end()) {
      setFilterState(key, it->second);
      return true;
    }
  }

#ifndef NULL_PLUGIN
  auto peer_header_view = peer_header;
#else
  auto peer_header_view = Wasm::Common::toAbslStringView(peer_header);
#endif

  auto bytes = Base64::decodeWithoutPadding(peer_header_view);
  google::protobuf::Struct metadata;
  if (!metadata.ParseFromString(bytes)) {
    return false;
  }

  auto fb = ::Wasm::Common::extractNodeFlatBufferFromStruct(metadata);
  std::string_view out(reinterpret_cast<const char*>(fb.data()), fb.size());
  setFilterState(key, out);

  if (max_peer_cache_size_ > 0) {
    // do not let the cache grow beyond max cache size.
    if (static_cast<uint32_t>(cache_.size()) > max_peer_cache_size_) {
      auto it = cache_.begin();
      cache_.erase(cache_.begin(), std::next(it, max_peer_cache_size_ / 4));
      LOG_DEBUG(absl::StrCat("cleaned cache, new cache_size:", cache_.size()));
    }
    cache_.emplace(std::move(id), out);
  }

  return true;
}

FilterHeadersStatus PluginContext::onRequestHeaders(uint32_t, bool) {
  // strip and store downstream peer metadata
  auto downstream_metadata_id = getRequestHeader(ExchangeMetadataHeaderId);
  if (downstream_metadata_id != nullptr && !downstream_metadata_id->view().empty()) {
    removeRequestHeader(ExchangeMetadataHeaderId);
    setFilterState(::Wasm::Common::kDownstreamMetadataIdKey, downstream_metadata_id->view());
  } else {
    metadata_id_received_ = false;
  }

  auto downstream_metadata_value = getRequestHeader(ExchangeMetadataHeader);
  if (downstream_metadata_value != nullptr && !downstream_metadata_value->view().empty()) {
    removeRequestHeader(ExchangeMetadataHeader);
    if (!rootContext()->updatePeer(::Wasm::Common::kDownstreamMetadataKey,
                                   downstream_metadata_id->view(),
                                   downstream_metadata_value->view())) {
      LOG_DEBUG("cannot set downstream peer node");
    }
  } else {
    metadata_received_ = false;
  }

  // do not send request internal headers to sidecar app if it is an inbound
  // proxy
  if (direction_ != ::Wasm::Common::TrafficDirection::Inbound) {
    auto metadata = metadataValue();
    // insert peer metadata struct for upstream
    if (!metadata.empty()) {
      replaceRequestHeader(ExchangeMetadataHeader, metadata);
    }

    auto nodeid = nodeId();
    if (!nodeid.empty()) {
      replaceRequestHeader(ExchangeMetadataHeaderId, nodeid);
    }
  }

  return FilterHeadersStatus::Continue;
}

FilterHeadersStatus PluginContext::onResponseHeaders(uint32_t, bool) {
  // strip and store upstream peer metadata
  auto upstream_metadata_id = getResponseHeader(ExchangeMetadataHeaderId);
  if (upstream_metadata_id != nullptr && !upstream_metadata_id->view().empty()) {
    removeResponseHeader(ExchangeMetadataHeaderId);
    setFilterState(::Wasm::Common::kUpstreamMetadataIdKey, upstream_metadata_id->view());
  }

  auto upstream_metadata_value = getResponseHeader(ExchangeMetadataHeader);
  if (upstream_metadata_value != nullptr && !upstream_metadata_value->view().empty()) {
    removeResponseHeader(ExchangeMetadataHeader);
    if (!rootContext()->updatePeer(::Wasm::Common::kUpstreamMetadataKey,
                                   upstream_metadata_id->view(), upstream_metadata_value->view())) {
      LOG_DEBUG("cannot set upstream peer node");
    }
  }

  // do not send response internal headers to sidecar app if it is an outbound
  // proxy
  if (direction_ != ::Wasm::Common::TrafficDirection::Outbound) {
    auto metadata = metadataValue();
    // insert peer metadata struct for downstream
    if (!metadata.empty() && metadata_received_) {
      replaceResponseHeader(ExchangeMetadataHeader, metadata);
    }

    auto nodeid = nodeId();
    if (!nodeid.empty() && metadata_id_received_) {
      replaceResponseHeader(ExchangeMetadataHeaderId, nodeid);
    }
  }

  return FilterHeadersStatus::Continue;
}

#ifdef NULL_PLUGIN
} // namespace Plugin
} // namespace MetadataExchange
} // namespace null_plugin
} // namespace proxy_wasm
#endif
