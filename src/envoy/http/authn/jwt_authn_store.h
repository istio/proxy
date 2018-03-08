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

#include "common/common/logger.h"
#include "envoy/server/filter_config.h"
#include "envoy/thread_local/thread_local.h"
#include "src/envoy/http/jwt_auth/auth_store.h"
#include "src/envoy/http/jwt_auth/config.pb.h"

namespace Envoy {
namespace Http {
namespace Istio {
namespace AuthN {

// Store the JwtAuthStoreFactory objects
class JwtAuthnFactoryStore : public Logger::Loggable<Logger::Id::config> {
 public:
  JwtAuthnFactoryStore(Server::Configuration::FactoryContext &context)
      : context_(context) {}

  // Get the reference of the JwtAuthStore objects
  std::map<std::string, Envoy::Http::JwtAuth::JwtAuthStore *> &store() {
    store_tls_.clear();
    for (auto it = store_.begin(); it != store_.end(); it++) {
      store_tls_[it->first] = &(it->second.store());
    }
    return store_tls_;
  }

  // Add an AuthFilterConfig to the store.
  void addToStore(Envoy::Http::JwtAuth::Config::AuthFilterConfig &config) {
    std::string config_str;
    config.SerializeToString(&config_str);
    if (store_.find(config_str) != store_.end()) {
      ENVOY_LOG(debug, "{}: AuthFilterConfig exists already", __FUNCTION__);
      return;
    }
    // Add a JwtAuthStoreFactory
    store_.emplace(std::piecewise_construct, std::forward_as_tuple(config_str),
                   std::forward_as_tuple(config, context_));
    ENVOY_LOG(debug, "{}: added a JwtAuthStoreFactory", __FUNCTION__);
  }

 private:
  // Store the FactoryContext object reference
  Server::Configuration::FactoryContext &context_;

  // Store the JwtAuthStoreFactory objects in a map.
  // The key is AuthFilterConfig as string.
  // Todo: may only need to use the issuer as the key.
  std::map<std::string, Envoy::Http::JwtAuth::JwtAuthStoreFactory> store_{};

  // Store the JwtAuthStore objects in a map.
  // Generated from JwtAuthStoreFactory at run time due to the thread local
  // nature.
  // The key is AuthFilterConfig as string.
  // Todo: may only need to use the issuer as the key.
  std::map<std::string, Envoy::Http::JwtAuth::JwtAuthStore *> store_tls_{};
};

}  // namespace AuthN
}  // namespace Istio
}  // namespace Http
}  // namespace Envoy
