/* Copyright 2018 Istio Authors. All Rights Reserved.
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

#include <chrono>
#include <unordered_map>

#include "authentication/v1alpha1/policy.pb.h"
#include "src/envoy/http/jwt_auth/jwt.h"

namespace Envoy {
namespace Http {
namespace IstioAuthN {
namespace {
// Default cache expiration time in 5 minutes.
const int kPubkeyCacheExpirationSec = 600;
}  // namespace

// A JWT public key cache item.
class JwtPubkeyCacheItem {
 public:
  // Constructor for an empty JwtPubKeyCacheItem
  JwtPubkeyCacheItem() {}
  // Return true if cached pubkey is expired.
  bool Expired() const {
    return std::chrono::steady_clock::now() >= expiration_time_;
  }

  // Get the pubkey object.
  const JwtAuth::Pubkeys* pubkey() const { return pubkey_.get(); }

  // Check if one of the audiences is allowed.
  bool IsAudienceAllowed(const std::vector<std::string>& jwt_audiences) {
    if (audiences_.empty()) {
      return true;
    }
    for (const auto& aud : jwt_audiences) {
      if (audiences_.find(aud) != audiences_.end()) {
        return true;
      }
    }
    return false;
  }
  // Set JWT audiences
  void SetAudiences(const std::set<std::string>& audiences) {
    // add Jwt audiences
    for (const std::string& aud : audiences) {
      if (aud != "") {
        audiences_.insert(aud);
      }
    }
  }

  // Set a pubkey as string.
  JwtAuth::Status SetKey(const std::string& pubkey_str) {
    auto pubkey =
        JwtAuth::Pubkeys::CreateFrom(pubkey_str, JwtAuth::Pubkeys::JWKS);
    if (pubkey->GetStatus() != JwtAuth::Status::OK) {
      return pubkey->GetStatus();
    }
    pubkey_ = std::move(pubkey);

    expiration_time_ = std::chrono::steady_clock::now();
    expiration_time_ += std::chrono::seconds(kPubkeyCacheExpirationSec);
    return JwtAuth::Status::OK;
  }

 private:
  // Jwt audience set for fast lookup
  std::set<std::string> audiences_;
  // The generated pubkey object.
  std::unique_ptr<JwtAuth::Pubkeys> pubkey_;
  // The pubkey expiration time.
  std::chrono::steady_clock::time_point expiration_time_;
};

// A cache for the JWT public key items
class JwtPubkeyCache {
 public:
  // Add JWT pubkey cache items, indexed by the JWT issuer
  void AddPubkeyItems(const ::google::protobuf::RepeatedPtrField<
                      ::istio::authentication::v1alpha1::Jwt>
                          jwts) {
    for (const ::istio::authentication::v1alpha1::Jwt& jwt : jwts) {
      if (jwt.issuer() != "") {
        pubkey_cache_map_.emplace(std::pair<std::string, JwtPubkeyCacheItem>(
            jwt.issuer(), JwtPubkeyCacheItem{}));
        std::set<std::string> audiences;
        for (const std::string aud : jwt.audiences()) {
          audiences.insert(aud);
        }
        pubkey_cache_map_[jwt.issuer()].SetAudiences(audiences);
      }
    }
  }

  // Lookup issuer cache map.
  JwtPubkeyCacheItem* LookupByIssuer(const std::string& name) {
    auto it = pubkey_cache_map_.find(name);
    if (it == pubkey_cache_map_.end()) {
      return nullptr;
    }
    return &it->second;
  }

 private:
  // The JWT public key cache map indexed by issuer.
  std::unordered_map<std::string, JwtPubkeyCacheItem> pubkey_cache_map_;
};

}  // namespace IstioAuthN
}  // namespace Http
}  // namespace Envoy
