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

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "extensions/common/json_util.h"
#include "extensions/common/proto_util.h"

#ifndef NULL_PLUGIN

#include "base64.h"
#include "declare_property.pb.h"

#else

#include "common/common/base64.h"
#include "source/extensions/common/wasm/declare_property.pb.h"

namespace Envoy {
namespace Extensions {
namespace Wasm {
namespace MetadataExchange {
namespace Plugin {

using namespace ::Envoy::Extensions::Common::Wasm::Null::Plugin;
using NullPluginRegistry =
    ::Envoy::Extensions::Common::Wasm::Null::NullPluginRegistry;

NULL_PLUGIN_REGISTRY;

#endif

namespace {

bool serializeToStringDeterministic(const google::protobuf::Message& metadata,
                                    std::string* metadata_bytes) {
  google::protobuf::io::StringOutputStream md(metadata_bytes);
  google::protobuf::io::CodedOutputStream mcs(&md);

  mcs.SetSerializationDeterministic(true);
  if (!metadata.SerializeToCodedStream(&mcs)) {
    LOG_WARN("unable to serialize metadata");
    return false;
  }
  return true;
}

}  // namespace

static RegisterContextFactory register_MetadataExchange(
    CONTEXT_FACTORY(PluginContext), ROOT_FACTORY(PluginRootContext));

void PluginRootContext::updateMetadataValue() {
  google::protobuf::Struct node_metadata;
  if (!getMessageValue({"node", "metadata"}, &node_metadata)) {
    LOG_WARN("cannot get node metadata");
    return;
  }

  google::protobuf::Struct metadata;
  const auto status =
      ::Wasm::Common::extractNodeMetadataValue(node_metadata, &metadata);
  if (!status.ok()) {
    LOG_WARN(status.message().ToString());
    return;
  }

  // store serialized form
  std::string metadata_bytes;
  serializeToStringDeterministic(metadata, &metadata_bytes);
  metadata_value_ =
      Base64::encode(metadata_bytes.data(), metadata_bytes.size());
}

// Metadata exchange has sane defaults and therefore it will be fully
// functional even with configuration errors.
// A configuration error thrown here will cause the proxy to crash.
bool PluginRootContext::onConfigure(size_t size) {
  updateMetadataValue();
  if (!getValue({"node", "id"}, &node_id_)) {
    LOG_DEBUG("cannot get node ID");
  }
  LOG_DEBUG(absl::StrCat("metadata_value_ id:", id(),
                         " value:", metadata_value_, " node:", node_id_));

  // Parse configuration JSON string.
  if (size > 0 && !configure(size)) {
    LOG_WARN("configuration has errrors, but initialzation can continue.");
  }

  // Declare filter state property type.
  const std::string function = "declare_property";
  envoy::source::extensions::common::wasm::DeclarePropertyArguments args;
  args.set_type(envoy::source::extensions::common::wasm::WasmType::FlatBuffers);
  args.set_span(
      envoy::source::extensions::common::wasm::LifeSpan::DownstreamConnection);
  args.set_schema(::Wasm::Common::nodeInfoSchema().data(),
                  ::Wasm::Common::nodeInfoSchema().size());
  std::string in;
  args.set_name(std::string(::Wasm::Common::kUpstreamMetadataKey));
  args.SerializeToString(&in);
  proxy_call_foreign_function(function.data(), function.size(), in.data(),
                              in.size(), nullptr, nullptr);
  args.set_name(std::string(::Wasm::Common::kDownstreamMetadataKey));
  args.SerializeToString(&in);
  proxy_call_foreign_function(function.data(), function.size(), in.data(),
                              in.size(), nullptr, nullptr);

  return true;
}

bool PluginRootContext::configure(size_t) {
  // Parse configuration JSON string.
  std::unique_ptr<WasmData> configuration = getConfiguration();
  auto j = ::Wasm::Common::JsonParse(configuration->view());
  if (!j.is_object()) {
    LOG_WARN(absl::StrCat("cannot parse plugin configuration JSON string: ",
                          configuration->view(), j.dump()));
    return false;
  }

  auto max_peer_cache_size =
      ::Wasm::Common::JsonGetField<int64_t>(j, "max_peer_cache_size");
  if (max_peer_cache_size.has_value()) {
    max_peer_cache_size_ = max_peer_cache_size.value();
  }
  return true;
}

bool PluginRootContext::updatePeer(StringView key, StringView peer_id,
                                   StringView peer_header) {
  std::string id = std::string(peer_id);
  if (max_peer_cache_size_ > 0) {
    auto it = cache_.find(id);
    if (it != cache_.end()) {
      setFilterState(key, it->second);
      return true;
    }
  }

  auto bytes = Base64::decodeWithoutPadding(peer_header);
  google::protobuf::Struct metadata;
  if (!metadata.ParseFromString(bytes)) {
    return false;
  }

  flatbuffers::FlatBufferBuilder fbb;
  if (!::Wasm::Common::extractNodeFlatBuffer(metadata, fbb)) {
    return false;
  }
  StringView out(reinterpret_cast<const char*>(fbb.GetBufferPointer()),
                 fbb.GetSize());
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

FilterHeadersStatus PluginContext::onRequestHeaders(uint32_t) {
  // strip and store downstream peer metadata
  auto downstream_metadata_id = getRequestHeader(ExchangeMetadataHeaderId);
  if (downstream_metadata_id != nullptr &&
      !downstream_metadata_id->view().empty()) {
    removeRequestHeader(ExchangeMetadataHeaderId);
    setFilterState(::Wasm::Common::kDownstreamMetadataIdKey,
                   downstream_metadata_id->view());
  } else {
    metadata_id_received_ = false;
  }

  auto downstream_metadata_value = getRequestHeader(ExchangeMetadataHeader);
  if (downstream_metadata_value != nullptr &&
      !downstream_metadata_value->view().empty()) {
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

FilterHeadersStatus PluginContext::onResponseHeaders(uint32_t) {
  // strip and store upstream peer metadata
  auto upstream_metadata_id = getResponseHeader(ExchangeMetadataHeaderId);
  if (upstream_metadata_id != nullptr &&
      !upstream_metadata_id->view().empty()) {
    removeResponseHeader(ExchangeMetadataHeaderId);
    setFilterState(::Wasm::Common::kUpstreamMetadataIdKey,
                   upstream_metadata_id->view());
  }

  auto upstream_metadata_value = getResponseHeader(ExchangeMetadataHeader);
  if (upstream_metadata_value != nullptr &&
      !upstream_metadata_value->view().empty()) {
    removeResponseHeader(ExchangeMetadataHeader);
    if (!rootContext()->updatePeer(::Wasm::Common::kUpstreamMetadataKey,
                                   upstream_metadata_id->view(),
                                   upstream_metadata_value->view())) {
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
}  // namespace Plugin
}  // namespace MetadataExchange
}  // namespace Wasm
}  // namespace Extensions
}  // namespace Envoy
#endif
