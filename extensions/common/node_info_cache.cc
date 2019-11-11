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

#include "extensions/common/node_info_cache.h"

#include "absl/strings/str_cat.h"
#include "extensions/common/context.h"
#include "google/protobuf/util/json_util.h"

using google::protobuf::util::Status;

#ifdef NULL_PLUGIN

using Envoy::Extensions::Common::Wasm::Null::Plugin::getStringValue;
using Envoy::Extensions::Common::Wasm::Null::Plugin::getStructValue;
using Envoy::Extensions::Common::Wasm::Null::Plugin::logDebug;
using Envoy::Extensions::Common::Wasm::Null::Plugin::logInfo;

#endif  // NULL_PLUGIN

namespace Wasm {
namespace Common {

namespace {

// getNodeInfo fetches peer node info from host filter state. It returns true if
// no error occurs.
bool getNodeInfo(StringView peer_metadata_key,
                 wasm::common::NodeInfo* node_info) {
  google::protobuf::Struct metadata;
  if (!getStructValue({"filter_state", peer_metadata_key}, &metadata)) {
    LOG_DEBUG(absl::StrCat("cannot get metadata for: ", peer_metadata_key));
    return false;
  }

  auto status = ::Wasm::Common::extractNodeMetadata(metadata, node_info);
  if (status != Status::OK) {
    LOG_DEBUG(absl::StrCat("cannot parse peer node metadata ",
                           metadata.DebugString(), ": ", status.ToString()));
    return false;
  }
  return true;
}

}  // namespace

NodeInfoPtr NodeInfoCache::getPeerById(StringView peer_metadata_id_key,
                                       StringView peer_metadata_key) {
  if (max_cache_size_ < 0) {
    // Cache is disabled, fetch node info from host.
    auto node_info_ptr = std::make_shared<wasm::common::NodeInfo>();
    if (getNodeInfo(peer_metadata_key, node_info_ptr.get())) {
      return node_info_ptr;
    }
    return nullptr;
  }

  std::string peer_id;
  if (!getStringValue({"filter_state", peer_metadata_id_key}, &peer_id)) {
    LOG_DEBUG(absl::StrCat("cannot get metadata for: ", peer_metadata_id_key));
    return nullptr;
  }
  auto nodeinfo_it = cache_.find(peer_id);
  if (nodeinfo_it != cache_.end()) {
    return nodeinfo_it->second;
  }

  // Do not let the cache grow beyond max_cache_size_.
  if (int32_t(cache_.size()) > max_cache_size_) {
    auto it = cache_.begin();
    cache_.erase(cache_.begin(), std::next(it, max_cache_size_ / 4));
    LOG_INFO(absl::StrCat("cleaned cache, new cache_size:", cache_.size()));
  }
  auto node_info_ptr = std::make_shared<wasm::common::NodeInfo>();
  if (getNodeInfo(peer_metadata_key, node_info_ptr.get())) {
    auto emplacement = cache_.emplace(peer_id, std::move(node_info_ptr));
    return emplacement.first->second;
  }
  return nullptr;
}

}  // namespace Common
}  // namespace Wasm
