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
#include "src/envoy/http/authn/jwt_authn_utils.h"
#include "src/envoy/http/jwt_auth/auth_store.h"
#include "src/envoy/http/jwt_auth/config.pb.h"

namespace Envoy {
namespace Http {
namespace Istio {
namespace AuthN {

// The comparator for ::istio::authentication::v1alpha1::Jwt
struct JwtAuthnComparator {
  bool operator()(const ::istio::authentication::v1alpha1::Jwt &a,
                  const ::istio::authentication::v1alpha1::Jwt &b) const {
    std::string s1, s2;
    a.SerializeToString(&s1);
    b.SerializeToString(&s2);
    return s1 < s2;
  }
};

// The map type for ::istio::authentication::v1alpha1::Jwt ->
// Envoy::Http::JwtAuth::JwtAuthStoreFactory
typedef std::map<::istio::authentication::v1alpha1::Jwt,
                 Envoy::Http::JwtAuth::JwtAuthStoreFactory, JwtAuthnComparator>
    JwtToAuthFactoryMap;

// The map type for ::istio::authentication::v1alpha1::Jwt ->
// Envoy::Http::JwtAuth::JwtAuthStore *
typedef std::map<::istio::authentication::v1alpha1::Jwt,
                 Envoy::Http::JwtAuth::JwtAuthStore *, JwtAuthnComparator>
    JwtToAuthStoreMap;

// Store the JwtAuthStoreFactory objects
class JwtAuthnFactoryStore : public Logger::Loggable<Logger::Id::config> {
 public:
  JwtAuthnFactoryStore(Server::Configuration::FactoryContext &context)
      : context_(context) {}

  // Get the reference of the JwtAuthStore objects
  JwtToAuthStoreMap &store() {
    jwt_store_tls_.clear();
    for (auto it = jwt_store_.begin(); it != jwt_store_.end(); it++) {
      jwt_store_tls_[it->first] = &(it->second.store());
    }
    return jwt_store_tls_;
  }

  // Add a JWT config to the store.
  void addToStore(const ::istio::authentication::v1alpha1::Jwt &jwt) {
    // TODO(lei-tang): it may be ok to use issuer as the key but need to make
    // sure
    // no cases of multiple Jwt have the same issuer
    if (jwt_store_.find(jwt) != jwt_store_.end()) {
      ENVOY_LOG(debug, "{}: AuthFilterConfig exists already", __FUNCTION__);
      return;
    }
    // Add a JwtAuthStoreFactory
    Http::JwtAuth::Config::AuthFilterConfig config;
    Envoy::Http::Istio::AuthN::convertJwtAuthFormat(jwt, &config);
    jwt_store_.emplace(std::piecewise_construct, std::forward_as_tuple(jwt),
                       std::forward_as_tuple(config, context_));
    ENVOY_LOG(debug, "{}: added a JwtAuthStoreFactory", __FUNCTION__);
  }

 private:
  // Store the FactoryContext object reference
  Server::Configuration::FactoryContext &context_;

  // Store the JwtAuthStoreFactory objects in a map.
  // The key is JWT.
  // TODO(lei-tang): it may be ok to use issuer as the key but need to make sure
  // no cases of multiple Jwt have the same issuer
  JwtToAuthFactoryMap jwt_store_{};

  // Store the JwtAuthStore objects in a map.
  // Generated from JwtAuthStoreFactory at run time due to the thread local
  // nature.
  // The key is JWT.
  // TODO(lei-tang): it may be ok to use issuer as the key but need to make sure
  // no cases of multiple Jwt have the same issuer
  JwtToAuthStoreMap jwt_store_tls_{};
};

}  // namespace AuthN
}  // namespace Istio
}  // namespace Http
}  // namespace Envoy
