/* Copyright 2017 Istio Authors. All Rights Reserved.
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

#ifndef AUTH_PUBKEY_CACHE_H
#define AUTH_PUBKEY_CACHE_H

#include <chrono>
#include <unordered_map>

#include "config.h"

namespace Envoy {
namespace Http {
namespace Auth {

// Struct to hold an issuer cache item.
class PubkeyCacheItem {
 public:
  PubkeyCacheItem(const IssuerInfo& config) : config_(config) {
    if (!config_.pubkey_value.empty()) {
      SetKey(config_.pubkey_value);
    }
  }

  // Return true if cached pubkey is expired.
  bool Expired() const {
    return config_.pubkey_cache_expiration_sec > 0 &&
           std::chrono::steady_clock::now() >= expiration_time_;
  }

  // Get the issuer config.
  const IssuerInfo& config() const { return config_; }

  // Get the pubkey object.
  const Pubkeys* pubkey() const { return pubkey_.get(); }

  // Set a pubkey as string.
  Status SetKey(const std::string& pubkey_str) {
    auto pubkey = Pubkeys::CreateFrom(pubkey_str, config_.pubkey_type);
    if (pubkey->GetStatus() != Status::OK) {
      return pubkey->GetStatus();
    }
    pubkey_ = std::move(pubkey);

    if (config_.pubkey_cache_expiration_sec > 0) {
      expiration_time_ =
          std::chrono::steady_clock::now() +
          std::chrono::seconds(config_.pubkey_cache_expiration_sec);
    }
    return Status::OK;
  }

 private:
  // The issuer config
  const IssuerInfo& config_;
  // The generated pubkey object.
  std::unique_ptr<Pubkeys> pubkey_;
  // The pubkey expiration time.
  std::chrono::steady_clock::time_point expiration_time_;
};

// Pubkey cache
class PubkeyCache {
 public:
  // Load the config from envoy config.
  PubkeyCache(const Config& config) {
    for (const auto& issuer : config.issuers()) {
      pubkey_cache_map_.emplace(issuer.name, issuer);
    }
  }

  // Lookup issuer cache map.
  PubkeyCacheItem* LookupByIssuer(const std::string& name) {
    auto it = pubkey_cache_map_.find(name);
    if (it == pubkey_cache_map_.end()) {
      return nullptr;
    }
    return &it->second;
  }

 private:
  // The public key cache map indexed by issuer.
  std::unordered_map<std::string, PubkeyCacheItem> pubkey_cache_map_;
};

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy

#endif  // AUTH_PUBKEY_CACHE_H
