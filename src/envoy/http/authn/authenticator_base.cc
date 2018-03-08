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

#include "src/envoy/http/authn/authenticator_base.h"
#include "src/envoy/http/authn/jwt_authn_utils.h"
#include "src/envoy/http/authn/mtls_authentication.h"

namespace iaapi = istio::authentication::v1alpha1;

namespace Envoy {
namespace Http {
namespace Istio {
namespace AuthN {
namespace {
// Returns true if rule is mathed for peer_id
bool isRuleMatchedWithPeer(const iaapi::CredentialRule& rule,
                           const std::string& peer_id) {
  if (rule.matching_peers_size() == 0) {
    return true;
  }
  for (const auto& allowed_id : rule.matching_peers()) {
    if (peer_id == allowed_id) {
      return true;
    }
  }
  return false;
}
}  // namespace

AuthenticatorBase::AuthenticatorBase(
    FilterContext* filter_context,
    const AuthenticatorBase::DoneCallback& done_callback,
    Upstream::ClusterManager& cm,
    std::map<std::string, JwtAuth::JwtAuthStore*>& jwt_store)
    : filter_context_(*filter_context),
      done_callback_(done_callback),
      cm_(cm),
      jwt_store_(jwt_store) {}

AuthenticatorBase::~AuthenticatorBase() {}

void AuthenticatorBase::done(bool success) const { done_callback_(success); }

void AuthenticatorBase::validateX509(
    const iaapi::MutualTls&,
    const AuthenticatorBase::MethodDoneCallback& done_callback) const {
  // Boilerplate for x509 validation and extraction. This function should
  // extract user from SAN field from the x509 certificate come with request.
  // (validation might not be needed, as establisment of the connection by
  // itself is validation).
  // If x509 is missing (i.e connection is not on TLS) or SAN value is not
  // legit, call callback with status FAILED.
  ENVOY_LOG(debug, "AuthenticationFilter: {} this connection requires mTLS",
            __func__);
  MtlsAuthentication mtls_authn(filter_context_.connection());
  if (mtls_authn.IsMutualTLS() == false) {
    done_callback(nullptr, false);
    return;
  }

  Payload payload;
  if (!mtls_authn.GetSourceUser(payload.mutable_x509()->mutable_user())) {
    done_callback(&payload, false);
  }

  // TODO (lei-tang): Adding other attributes (i.e ip) to payload if desire.
  done_callback(&payload, true);
}

void AuthenticatorBase::validateJwt(
    const iaapi::Jwt& jwt,
    const AuthenticatorBase::MethodDoneCallback& done_callback) {
  Http::JwtAuth::Config::AuthFilterConfig proto_config;
  Envoy::Http::Istio::AuthN::convertJwtAuthFormat(jwt, &proto_config);

  std::string config_str;
  proto_config.SerializeToString(&config_str);
  if (jwt_store_.find(config_str) == jwt_store_.end()) {
    ENVOY_LOG(error, "{}: the JWT config is not found: {}", __FUNCTION__,
              proto_config.DebugString());
    done_callback(nullptr, false);
    return;
  }

  // Choose the JwtAuthStore based on the Jwt config.
  jwt_auth_.reset(
      new Http::JwtAuth::JwtAuthenticator(cm_, *jwt_store_[config_str]));

  // Record done_callback so that it would be trigger by
  // jwt_authenticator.onDone.
  jwt_done_callback_ = &done_callback;

  // Verify the JWT token, onDone() will be called when completed.
  jwt_auth_->Verify(*filter_context()->headers(), this);
}

void AuthenticatorBase::onDone(const JwtAuth::Status& status) {
  ENVOY_LOG(debug, "AuthenticatorBase::onDone is called with status {}",
            int(status));
  if (status != JwtAuth::Status::OK) {
    (*jwt_done_callback_)(&payload_, false);
  } else {
    // TODO (lei-tang): Adding other JWT attributes (i.e jwt sub, claims) to
    // payload
    ENVOY_LOG(debug, "AuthenticatorBase::onDone JwtAuth returns OK.");
    (*jwt_done_callback_)(&payload_, true);
  }
  jwt_done_callback_ = nullptr;
}

const iaapi::CredentialRule& findCredentialRuleOrDefault(
    const iaapi::Policy& policy, const std::string& peer_id) {
  for (const auto& rule : policy.credential_rules()) {
    if (isRuleMatchedWithPeer(rule, peer_id)) {
      return rule;
    }
  }
  return iaapi::CredentialRule::default_instance();
}

}  // namespace AuthN
}  // namespace Istio
}  // namespace Http
}  // namespace Envoy
