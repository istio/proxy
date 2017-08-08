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

#ifndef PROXY_CONFIG_H
#define PROXY_CONFIG_H

#include "envoy/json/json_object.h"
#include "envoy/upstream/cluster_manager.h"
#include "server/config/network/http_connection_manager.h"

#include <vector>

namespace Envoy {
namespace Http {
namespace Auth {

// Class to hold an issuer's info.
class IssuerInfo {
 public:
  std::string name_;       // e.g. "https://accounts.google.com"
  std::string pkey_type_;  // format of public key. "jwks" or "pem"
  std::string pkey_;       // public key
  IssuerInfo(const std::string &name, const std::string &pkey_type,
             const std::string &pkey);
};

// A config for Jwt auth filter
class JwtAuthConfig {
 public:
  JwtAuthConfig(const Json::Object &config,
                Server::Configuration::FactoryContext &context)
      : cm_(context.clusterManager()) {
    Load(config);
  }

  // Each element corresponds to an issuer
  std::vector<std::shared_ptr<IssuerInfo> > issuers_;

 private:
  // Load the config from envoy config.
  void Load(const Json::Object &json);
  std::shared_ptr<IssuerInfo> LoadIssuer(Json::Object *json);
  std::shared_ptr<IssuerInfo> LoadIssuerFromDiscoveryDocument(
      Json::Object *json);
  std::shared_ptr<IssuerInfo> LoadPubkeyFromObject(Json::Object *json);
  std::string ReadWholeFile(const std::string &path);
  std::string ReadWholeFileByHttp(const std::string &uri,
                                  const std::string &cluster);
  std::string GetContentFromUri(const std::string &uri,
                                const std::string &cluster);
  std::shared_ptr<IssuerInfo> LoadIssuerFromDiscoveryDocumentStr(
      const std::string &doc, const std::string &cluster);

  Upstream::ClusterManager &cm_;
};

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy

#endif  // PROXY_CONFIG_H
