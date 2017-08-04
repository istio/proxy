//
// Created by mtakigiku on 8/4/17.
//

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