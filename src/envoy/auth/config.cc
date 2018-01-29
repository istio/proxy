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

#include "src/envoy/auth/config.h"

#include <chrono>

namespace Envoy {
namespace Http {
namespace Auth {
namespace {

// Default public key cache cache duration: 5 minutes.
const int64_t kPubKeyCacheExpirationSec = 600;

// Load issuer config from JSON.
void LoadIssuerConfig(const Json::Object& json, IssuerConfig* issuer) {
  // Check "name"
  issuer->name = json.getString("name", "");
  if (issuer->name == "") {
    throw EnvoyException("Issuer name missing");
  }

  // Check "audience".
  // It will be an empty array if the key "audience" does not exist.
  auto audiences = json.getStringArray("audiences", true);
  issuer->audiences.insert(audiences.begin(), audiences.end());

  // Check "pubkey"
  auto json_pubkey = json.getObject("pubkey");

  // Check "type"
  std::string pubkey_type_str = json_pubkey->getString("type", "");
  if (pubkey_type_str == "pem") {
    issuer->pubkey_type = Pubkeys::PEM;
  } else if (pubkey_type_str == "jwks") {
    issuer->pubkey_type = Pubkeys::JWKS;
  } else {
    throw EnvoyException(
        fmt::format("Issuer [name = {}]: Public key type missing or invalid",
                    issuer->name));
  }

  // Check "value"
  issuer->pubkey_value = json_pubkey->getString("value", "");

  // Check "uri" and "cluster"
  issuer->uri = json_pubkey->getString("uri", "");
  issuer->cluster = json_pubkey->getString("cluster", "");

  // Check "cache_expiration_sec".
  issuer->pubkey_cache_expiration_sec = json_pubkey->getInteger(
      "cache_expiration_sec", kPubKeyCacheExpirationSec);
}

}  // namespace

std::string IssuerConfig::Validate() const {
  if (!pubkey_value.empty()) {
    auto pubkey = Pubkeys::CreateFrom(pubkey_value, pubkey_type);
    if (pubkey->GetStatus() != Status::OK) {
      return std::string("Invalid public key value: ") + pubkey_value;
    }
  } else {
    if (uri == "") {
      return "Missing public key server uri";
    }
    if (cluster == "") {
      return "Missing public key server cluster";
    }
  }
  return "";
}

JwtAuthConfig::JwtAuthConfig(std::vector<IssuerConfig>&& issuers)
    : issuers_(std::move(issuers)) {
  ENVOY_LOG(debug, "JwtAuthConfig: {}", __func__);
}

JwtAuthConfig::JwtAuthConfig(const Json::Object& config) {
  ENVOY_LOG(debug, "JwtAuthConfig: {}", __func__);

  // Load the issuers as JSON array.
  auto issuer_jsons = config.getObjectArray("issuers");
  for (auto issuer_json : issuer_jsons) {
    IssuerConfig issuer;
    LoadIssuerConfig(*issuer_json, &issuer);
    std::string err = issuer.Validate();
    if (err.empty()) {
      issuers_.push_back(issuer);
    } else {
      throw EnvoyException(
          fmt::format("Issuer [name = {}]: {}", issuer.name, err));
    }
  }
}

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy
