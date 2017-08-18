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

#include "jwt.h"

#include "common/http/message_impl.h"
#include "envoy/http/async_client.h"
#include "envoy/json/json_object.h"
#include "envoy/json/json_object.h"
#include "envoy/upstream/cluster_manager.h"
#include "server/config/network/http_connection_manager.h"

#include <vector>

namespace Envoy {
namespace Http {
namespace Auth {

// Callback class for AsyncClient.
class AsyncClientCallbacks : public AsyncClient::Callbacks {
 public:
  AsyncClientCallbacks(Upstream::ClusterManager &cm, const std::string &cluster,
                       std::function<void(bool, const std::string &)> cb)
      : cm_(cm),
        cluster_(cm.get(cluster)->info()),
        timeout_(Optional<std::chrono::milliseconds>()),
        cb_(cb) {}
  void onSuccess(MessagePtr &&response);

  void onFailure(AsyncClient::FailureReason);

  void Call(const std::string &uri);

 private:
  Upstream::ClusterManager &cm_;
  Upstream::ClusterInfoConstSharedPtr cluster_;
  Optional<std::chrono::milliseconds> timeout_;
  std::function<void(bool, const std::string &)> cb_;
};

// Struct to hold an issuer's information.
struct IssuerInfo {
  IssuerInfo(Json::Object *json) : loaded_(false) { failed_ = !Preload(json); }
  bool failed_;          // True if Preload() or fetching public key failed
  bool loaded_;          // If the public key is loaded or not
  std::string uri_;      // URI for public key
  std::string cluster_;  // Envoy cluster name for public key

  bool Preload(Json::Object *json);
  std::string name_;       // e.g. "https://accounts.example.com"
  std::string pkey_type_;  // Format of public key. "jwks" or "pem"
  std::string pkey_;       // Public key

  std::unique_ptr<AsyncClientCallbacks> async_client_cb_;
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
  Upstream::ClusterManager &cm_;

 private:
  // Load the config from envoy config.
  void Load(const Json::Object &json);
};

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy

#endif  // PROXY_CONFIG_H
