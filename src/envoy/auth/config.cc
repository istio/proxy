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

#include "config.h"

#include <chrono>

namespace Envoy {
namespace Http {
namespace Auth {
namespace {

// Default public key cache cache duration: 5 minutes.
const int64_t kPubKeyCacheExpirationSec = 600;

}  // namespace

std::string IssuerInfo::Validate() const {
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

Config::Config(std::vector<IssuerInfo>&& issuers)
    : issuers_(std::move(issuers)) {
  ENVOY_LOG(debug, "Config: {}", __func__);
}

Config::Config(const Json::Object& config) {
  ENVOY_LOG(debug, "Config: {}", __func__);

  // Load the issuers as JSON array.
  std::vector<Json::ObjectSharedPtr> issuer_jsons;
  try {
    issuer_jsons = config.getObjectArray("issuers");
  } catch (...) {
    ENVOY_LOG(error, "Config: issuers should be array type");
    return;
  }

  for (auto issuer_json : issuer_jsons) {
    IssuerInfo issuer;
    if (LoadIssuerInfo(*issuer_json, &issuer)) {
      std::string err = issuer.Validate();
      if (err.empty()) {
        issuers_.push_back(issuer);
      } else {
        ENVOY_LOG(error, "Issuer [name = {}]: {}", issuer.name, err);
      }
    }
  }
}

bool Config::LoadIssuerInfo(const Json::Object& json, IssuerInfo* issuer) {
  // Check "name"
  issuer->name = json.getString("name", "");
  if (issuer->name == "") {
    ENVOY_LOG(error, "Issuer: Issuer name missing");
    return false;
  }

  // Check "audience".
  // It will be an empty array if the key "audience" does not exist.
  try {
    auto audiences = json.getStringArray("audiences", true);
    issuer->audiences.insert(audiences.begin(), audiences.end());
  } catch (...) {
    ENVOY_LOG(error, "Issuer [name = {}]: Bad audiences", issuer->name);
    return false;
  }

  // Check "pubkey"
  Json::ObjectSharedPtr json_pubkey;
  try {
    json_pubkey = json.getObject("pubkey");
  } catch (...) {
    ENVOY_LOG(error, "Issuer [name = {}]: Public key missing", issuer->name);
    return false;
  }

  // Check "type"
  std::string pubkey_type_str = json_pubkey->getString("type", "");
  if (pubkey_type_str == "pem") {
    issuer->pubkey_type = Pubkeys::PEM;
  } else if (pubkey_type_str == "jwks") {
    issuer->pubkey_type = Pubkeys::JWKS;
  } else {
    ENVOY_LOG(error, "Issuer [name = {}]: Public key type missing or invalid",
              issuer->name);
    return false;
  }

  // Check "value"
  issuer->pubkey_value = json_pubkey->getString("value", "");
  // If pubkey_value is not empty, not need for "uri" and "cluster"
  if (issuer->pubkey_value != "") {
    return true;
  }

  // Check "uri" and "cluster"
  issuer->uri = json_pubkey->getString("uri", "");
  issuer->cluster = json_pubkey->getString("cluster", "");

  // Check "cache_expiration_sec".
  issuer->pubkey_cache_expiration_sec = json_pubkey->getInteger(
      "cache_expiration_sec", kPubKeyCacheExpirationSec);
  return true;
}

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy
