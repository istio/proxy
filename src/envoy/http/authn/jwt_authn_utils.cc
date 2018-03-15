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

#include "src/envoy/http/authn/jwt_authn_utils.h"

namespace Envoy {
namespace Http {
namespace Istio {
namespace AuthN {

namespace {
const std::string kJwtClusterName("example_issuer");
}  // namespace

// Get the Jwks URI for Envoy cluster
const std::string getJwksUriEnvoyCluster() {
  // Todo: Pilot needs to put a cluster field in the Istio authn JWT config.
  // Before such field is added to the Istio authn JWT config,
  // it is temporarily hard-coded.
  return kJwtClusterName;
}

// Convert istio-authn::jwt to jwt_auth::jwt in protobuf format.
void convertJwtAuthFormat(
    const ::istio::authentication::v1alpha1::Jwt& jwt_authn,
    Http::JwtAuth::Config::AuthFilterConfig* proto_config) {
  // Todo: when istio-authn::jwt diverges from jwt_auth::jwt,
  // may need to convert more fields.
  auto jwt = proto_config->add_jwts();
  MessageUtil::jsonConvert(jwt_authn, *jwt);
  jwt->set_jwks_uri_envoy_cluster(getJwksUriEnvoyCluster());
}

}  // namespace AuthN
}  // namespace Istio
}  // namespace Http
}  // namespace Envoy
