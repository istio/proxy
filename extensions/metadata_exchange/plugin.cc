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

#ifndef NULL_PLUGIN
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"

#include "plugin.h"

#include "base64.h"

#else
#include "common/common/base64.h"
#include "extensions/metadata_exchange/plugin.h"

namespace Envoy {
namespace Extensions {
namespace Wasm {
namespace MetadataExchange {
namespace Plugin {

using namespace ::Envoy::Extensions::Common::Wasm::Null::Plugin;
using NullPluginRootRegistry =
    ::Envoy::Extensions::Common::Wasm::Null::NullPluginRootRegistry;

NULL_PLUGIN_ROOT_REGISTRY;

#endif

namespace {

bool serializeToStringDeterministic(const google::protobuf::Value& metadata,
                                    std::string* metadata_bytes) {
  google::protobuf::io::StringOutputStream md(metadata_bytes);
  google::protobuf::io::CodedOutputStream mcs(&md);

  mcs.SetSerializationDeterministic(true);
  if (!metadata.SerializeToCodedStream(&mcs)) {
    logWarn("unable to serialize metadata");
    return false;
  }
  return true;
}

}  // namespace

static RegisterContextFactory register_MetadataExchange(
    CONTEXT_FACTORY(PluginContext), ROOT_FACTORY(PluginRootContext));

void PluginRootContext::updateMetadataValue() {
  google::protobuf::Struct node_metadata;
  if (!getStructValue({"node", "metadata"}, &node_metadata)) {
    logWarn("cannot get node metadata");
    return;
  }

  const auto key_it = node_metadata.fields().find("EXCHANGE_KEYS");
  if (key_it == node_metadata.fields().end()) {
    logWarn("metadata exchange key is missing");
    return;
  }

  const auto& keys_value = key_it->second;
  if (keys_value.kind_case() != google::protobuf::Value::kStringValue) {
    logWarn("metadata exchange key is not a string");
    return;
  }

  google::protobuf::Value metadata;

  // select keys from the metadata using the keys
  const std::set<std::string> keys =
      absl::StrSplit(keys_value.string_value(), ',', absl::SkipWhitespace());
  for (auto key : keys) {
    const auto entry_it = node_metadata.fields().find(key);
    if (entry_it == node_metadata.fields().end()) {
      logDebug(absl::StrCat("missing metadata exchange key: ", key));
      continue;
    }
    (*metadata.mutable_struct_value()->mutable_fields())[key] =
        entry_it->second;
  }

  // store serialized form
  std::string metadata_bytes;
  serializeToStringDeterministic(metadata, &metadata_bytes);
  metadata_value_ =
      Base64::encode(metadata_bytes.data(), metadata_bytes.size());
}

bool PluginRootContext::onConfigure(std::unique_ptr<WasmData>) {
  updateMetadataValue();
  if (!getStringValue({"node", "id"}, &node_id_)) {
    logDebug("cannot get node ID");
  }
  logDebug(absl::StrCat("metadata_value_ id:", id(), " value:", metadata_value_,
                        " node:", node_id_));
  return true;
}

FilterHeadersStatus PluginContext::onRequestHeaders() {
  // strip and store downstream peer metadata
  auto downstream_metadata_value = getRequestHeader(ExchangeMetadataHeader);
  if (downstream_metadata_value != nullptr &&
      !downstream_metadata_value->view().empty()) {
    removeRequestHeader(ExchangeMetadataHeader);
    auto downstream_metadata_bytes =
        Base64::decodeWithoutPadding(downstream_metadata_value->view());
    setFilterState(DownstreamMetadataKey, downstream_metadata_bytes);
  }

  auto downstream_metadata_id = getRequestHeader(ExchangeMetadataHeaderId);
  if (downstream_metadata_id != nullptr &&
      !downstream_metadata_id->view().empty()) {
    removeRequestHeader(ExchangeMetadataHeaderId);
    setFilterStateStringValue(DownstreamMetadataIdKey,
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

  return FilterHeadersStatus::Continue;
}

FilterHeadersStatus PluginContext::onResponseHeaders() {
  // strip and store upstream peer metadata
  auto upstream_metadata_value = getResponseHeader(ExchangeMetadataHeader);
  if (upstream_metadata_value != nullptr &&
      !upstream_metadata_value->view().empty()) {
    removeResponseHeader(ExchangeMetadataHeader);
    auto upstream_metadata_bytes =
        Base64::decodeWithoutPadding(upstream_metadata_value->view());
    setFilterState(UpstreamMetadataKey, upstream_metadata_bytes);
  }

  auto upstream_metadata_id = getResponseHeader(ExchangeMetadataHeaderId);
  if (upstream_metadata_id != nullptr &&
      !upstream_metadata_id->view().empty()) {
    removeRequestHeader(ExchangeMetadataHeaderId);
    setFilterStateStringValue(UpstreamMetadataIdKey,
                              upstream_metadata_id->view());
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

  return FilterHeadersStatus::Continue;
}

#ifdef NULL_PLUGIN
}  // namespace Plugin
}  // namespace MetadataExchange
}  // namespace Wasm
}  // namespace Extensions
}  // namespace Envoy
#endif
