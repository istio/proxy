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

#include "envoy/json/json_object.h"

#include <memory>
#include <string>
#include <vector>

namespace Envoy {
namespace Http {
namespace Auth {

IssuerInfo::IssuerInfo(const std::string &name, const std::string &pkey_type,
                       const std::string &pkey) {
  name_ = name;
  pkey_type_ = pkey_type;
  pkey_ = pkey;
}

std::shared_ptr<IssuerInfo> JwtAuthConfig::LoadIssuerFromDiscoveryDocment(
    Json::Object *) {
  /*
   * TODO: implement
   */
  return nullptr;
}

std::shared_ptr<IssuerInfo> JwtAuthConfig::LoadPubkeyFromObject(
    Json::Object *json) {
  if (json->hasObject("type")) {
    std::string type = json->getString("type");
    if (json->hasObject("uri")) {
      std::string uri = json->getString("uri");
      /*
       * TODO: implement
       */
    } else if (json->hasObject("value")) {
      std::string pubkey = json->getString("value");
      return std::shared_ptr<IssuerInfo>(new IssuerInfo("", type, pubkey));
    }
  }
  return nullptr;
}

// Load information of an issuer. Returns nullptr if bad-formatted.
std::shared_ptr<IssuerInfo> JwtAuthConfig::LoadIssuer(Json::Object *json) {
  if (json->hasObject("discovery_document")) {
    return LoadIssuerFromDiscoveryDocment(
        json->getObject("discovery_document").get());
  } else {
    if (json->hasObject("name") && json->hasObject("pubkey")) {
      auto ret = LoadPubkeyFromObject(json->getObject("pubkey").get());
      ret->name_ = json->getString("name");
      return ret;
    }
  }
  return nullptr;
}

// Load config from envoy config.
void JwtAuthConfig::Load(const Json::Object &json) {
  issuers_.clear();
  if (json.hasObject("issuers")) {
    for (auto issuer_json : json.getObjectArray("issuers")) {
      auto issuer = LoadIssuer(issuer_json.get());
      if (issuer) {
        issuers_.push_back(issuer);
      }
    }
  }
}

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy