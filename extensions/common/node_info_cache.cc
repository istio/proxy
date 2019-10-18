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
#include "extensions/common/context.h"

#ifdef NULL_PLUGIN

using Envoy::Extensions::Common::Wasm::Null::Plugin::getStringValue;
using Envoy::Extensions::Common::Wasm::Null::Plugin::getStructValue;
using Envoy::Extensions::Common::Wasm::Null::Plugin::logDebug;
using Envoy::Extensions::Common::Wasm::Null::Plugin::logInfo;
using google::protobuf::util::Status;

#endif  // NULL_PLUGIN

namespace Wasm {
namespace Common {

const wasm::common::NodeInfo& NodeInfoCache::getPeerById(
    StringView peer_metadata_id_key, StringView peer_metadata_key,
    std::string* peer_id) {
  if (!getStringValue({"filter_state", peer_metadata_id_key}, peer_id)) {
    LOG_DEBUG(absl::StrCat("cannot get metadata for: ", peer_metadata_id_key));
    return cache_[""];
  }
  auto nodeinfo_it = cache_.find(*peer_id);
  if (nodeinfo_it != cache_.end()) {
    return nodeinfo_it->second;
  }

  // Do not let the cache grow beyond max_cache_size_.
  if (cache_.size() > max_cache_size_) {
    auto it = cache_.begin();
    cache_.erase(cache_.begin(), std::next(it, max_cache_size_ / 4));
    LOG_INFO(absl::StrCat("cleaned cache, new cache_size:", cache_.size()));
  }

  google::protobuf::Struct metadata;
  if (!getStructValue({"filter_state", peer_metadata_key}, &metadata)) {
    LOG_DEBUG(absl::StrCat("cannot get metadata for: ", peer_metadata_key));
    return cache_[""];
  }

  auto status =
      ::Wasm::Common::extractNodeMetadata(metadata, &(cache_[*peer_id]));
  if (status != Status::OK) {
    LOG_DEBUG(absl::StrCat("cannot parse peer node metadata ",
                           metadata.DebugString(), ": ", status.ToString()));
    return cache_[""];
  }

  return cache_[*peer_id];
}

const wasm::common::NodeInfo& NodeInfoCache::getPeerById(
    absl::string_view peer_metadata_id_key,
    absl::string_view peer_metadata_key) {
  std::string peer_id;
  return getPeerById(peer_metadata_id_key, peer_metadata_key, &peer_id);
}

}  // namespace Common
}  // namespace Wasm
