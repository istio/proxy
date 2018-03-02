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

#include "src/envoy/http/authn/http_filter.h"
#include "common/http/utility.h"
#include "src/envoy/http/authn/mtls_authentication.h"
#include "src/envoy/utils/utils.h"

namespace Envoy {
namespace Http {
namespace {

bool MatchCredentialRule(const std::string& peer_user,
                         const iaapi::CredentialRule& rule) {
  if (rule.matching_peers_size() == 0) {
    return true;
  }
  for (const auto& allowed_id : rule.matching_peers()) {
    if (peer_user == allowed_id) {
      return true;
    }
  }
  return false;
}

const iaapi::CredentialRule& GetCredentialRuleOrDefault(
    const std::string& peer_user, const iaapi::Policy& config) {
  for (const auto& rule : config.credential_rules()) {
    if (MatchCredentialRule(peer_user, rule)) {
      return rule;
    }
  }
  return iaapi::CredentialRule::default_instance();
}

}  // namespace

AuthenticationFilter::AuthenticationFilter(
    const istio::authentication::v1alpha1::Policy& config)
    : config_(config) {}

AuthenticationFilter::~AuthenticationFilter() {}

void AuthenticationFilter::onDestroy() {
  ENVOY_LOG(debug, "Called AuthenticationFilter : {}", __func__);
}

FilterHeadersStatus AuthenticationFilter::decodeHeaders(HeaderMap& headers,
                                                        bool) {
  ENVOY_LOG(debug, "Called AuthenticationFilter : {}", __func__);
  state_ = IstioAuthN::State::PROCESSING;
  if (config_.peers_size() == 0) {
    ENVOY_LOG(debug, "No method defined. Skip source authentication.");
    onAuthenticatePeerDone(&headers, 0, nullptr, IstioAuthN::Status::SUCCESS);
  } else {
    authenticatePeer(
        headers, config_.peers(0),
        std::bind(&AuthenticationFilter::onAuthenticatePeerDone, this, &headers,
                  0, std::placeholders::_1, std::placeholders::_2));
  }
  if (state_ == IstioAuthN::State::COMPLETE) {
    return FilterHeadersStatus::Continue;
  }

  stopped_ = true;
  return FilterHeadersStatus::StopIteration;
}

void AuthenticationFilter::authenticatePeer(
    HeaderMap& headers, const iaapi::PeerAuthenticationMethod& method,
    const AuthenticateDoneCallback& done_callback) {
  switch (method.params_case()) {
    case iaapi::PeerAuthenticationMethod::ParamsCase::kMtls:
      ValidateX509(headers, method.mtls(), done_callback);
      break;
    case iaapi::PeerAuthenticationMethod::ParamsCase::kJwt:
      ValidateJwt(headers, method.jwt(), done_callback);
      break;
    case iaapi::PeerAuthenticationMethod::ParamsCase::kNone:
      done_callback(nullptr, IstioAuthN::Status::SUCCESS);
      break;
    default:
      ENVOY_LOG(error, "Unknown peer authentication param {}",
                method.DebugString());
  }
}

void AuthenticationFilter::onAuthenticatePeerDone(
    HeaderMap* headers, int peer_method_index,
    std::unique_ptr<IstioAuthN::AuthenticatePayload> payload,
    const IstioAuthN::Status& status) {
  if (status == IstioAuthN::Status::FAILED) {
    peer_method_index++;
    if (peer_method_index >= config_.peers_size()) {
      // No more method left to try, reject request.
      rejectRequest("Source authentication failed.");
    } else {
      // Try next one.
      authenticatePeer(*headers, config_.peers(peer_method_index),
                       std::bind(&AuthenticationFilter::onAuthenticatePeerDone,
                                 this, headers, peer_method_index,
                                 std::placeholders::_1, std::placeholders::_2));
    }
  }

  // Source authentication success, continue for credetial / origin
  // authentication.
  if (status == IstioAuthN::Status::SUCCESS) {
    if (payload != nullptr) {
      if (payload->has_x509()) {
        context_.set_peer_user(payload->x509().user());
      } else if (payload->has_jwt()) {
        context_.set_peer_user(payload->jwt().user());
      } else {
        ENVOY_LOG(warn,
                  "Source authentiation payload doesn't contain x509 nor jwt "
                  "payload.");
      }
    }

    const auto& rule =
        GetCredentialRuleOrDefault(context_.peer_user(), config_);
    if (rule.origins_size() == 0) {
      switch (rule.binding()) {
        case iaapi::CredentialRule::USE_ORIGIN:
          // Validation should reject policy that have rule to USE_ORIGIN but
          // does not provide any origin method so this code should
          // never reach. However, it's ok to treat it as authentication
          // fails.
          ENVOY_LOG(
              warn,
              "Principal is binded to origin, but not methods specified in "
              "rule {}",
              rule.DebugString());
          onAuthenticateOriginDone(headers, &rule, 0, nullptr,
                                   IstioAuthN::Status::FAILED);
          break;
        case iaapi::CredentialRule::USE_PEER:
          // On the other hand, it's ok to have no (origin) methods if
          // rule USE_SOURCE
          onAuthenticateOriginDone(headers, &rule, 0, nullptr,
                                   IstioAuthN::Status::SUCCESS);
          break;
        default:
          // Should never come here.
          ENVOY_LOG(error, "Invalid binding value for rule {}",
                    rule.DebugString());
          break;
      }
      return;
    }
    authenticateOrigin(
        *headers, rule.origins(0),
        std::bind(&AuthenticationFilter::onAuthenticateOriginDone, this,
                  headers, &rule, 0, std::placeholders::_1,
                  std::placeholders::_2));
  }
}

void AuthenticationFilter::authenticateOrigin(
    HeaderMap& headers, const iaapi::OriginAuthenticationMethod& method,
    const AuthenticateDoneCallback& done_callback) {
  ValidateJwt(headers, method.jwt(), done_callback);
}

void AuthenticationFilter::onAuthenticateOriginDone(
    HeaderMap* headers, const iaapi::CredentialRule* rule, int method_index,
    std::unique_ptr<IstioAuthN::AuthenticatePayload> payload,
    const IstioAuthN::Status& status) {
  if (status == IstioAuthN::Status::FAILED) {
    // Try next one.
    method_index++;
    if (method_index < rule->origins_size()) {
      authenticateOrigin(
          *headers, rule->origins(method_index),
          std::bind(&AuthenticationFilter::onAuthenticateOriginDone, this,
                    headers, rule, method_index, std::placeholders::_1,
                    std::placeholders::_2));
    } else {
      rejectRequest("Origin authentication failed.");
    }
  }
  if (status == IstioAuthN::Status::SUCCESS) {
    // At the moment, only JWT can be used for origin authentication, so
    // it's ok just to check jwt payload.
    if (payload != nullptr && payload->has_jwt()) {
      *context_.mutable_origin() = payload->jwt();
    }
    switch (rule->binding()) {
      case iaapi::CredentialRule::USE_PEER:
        context_.set_principal(context_.peer_user());
        break;
      case iaapi::CredentialRule::USE_ORIGIN:
        context_.set_principal(context_.origin().user());
        break;
      default:
        // Should never come here.
        ENVOY_LOG(error, "Invalid binding value for rule {}",
                  rule->DebugString());
        break;
    }
    // It's done. continueDecoding to accept request.
    continueDecoding();
  }
}

void AuthenticationFilter::continueDecoding() {
  state_ = IstioAuthN::State::COMPLETE;
  if (stopped_) {
    decoder_callbacks_->continueDecoding();
  }
}

void AuthenticationFilter::rejectRequest(const std::string& message) {
  Utility::sendLocalReply(*decoder_callbacks_, false, Http::Code::Unauthorized,
                          message);
}
void AuthenticationFilter::ValidateX509(
    const HeaderMap&, const iaapi::MutualTls&,
    const AuthenticateDoneCallback& done_callback) const {
  // Boilerplate for x509 validation and extraction. This function should
  // extract user from SAN field from the x509 certificate come with request.
  // (validation might not be needed, as establisment of the connection by
  // itself is validation).
  // If x509 is missing (i.e connection is not on TLS) or SAN value is not
  // legit, call callback with status FAILED.
  ENVOY_LOG(debug, "AuthenticationFilter: {} this connection requires mTLS",
            __func__);
  MtlsAuthentication mtls_authn(decoder_callbacks_->connection());
  if (mtls_authn.IsMutualTLS() == false) {
    done_callback(nullptr, IstioAuthN::Status::FAILED);
    return;
  }

  std::unique_ptr<IstioAuthN::AuthenticatePayload> payload(
      new IstioAuthN::AuthenticatePayload());
  if (!mtls_authn.GetSourceUser(payload->mutable_x509()->mutable_user())) {
    done_callback(std::move(payload), IstioAuthN::Status::FAILED);
  }

  // TODO (lei-tang): Adding other attributes (i.e ip) to payload if desire.
  done_callback(std::move(payload), IstioAuthN::Status::SUCCESS);
}

void AuthenticationFilter::ValidateJwt(
    const HeaderMap&, const iaapi::Jwt&,
    const AuthenticateDoneCallback& done_callback) const {
  std::unique_ptr<IstioAuthN::AuthenticatePayload> payload(
      new IstioAuthN::AuthenticatePayload());
  // TODO (diemtvu/lei-tang): construct jwt_authenticator and call Verify;
  // pass done_callback so that it would be trigger by jwt_authenticator.onDone.
  done_callback(std::move(payload), IstioAuthN::Status::FAILED);
}

FilterDataStatus AuthenticationFilter::decodeData(Buffer::Instance&, bool) {
  ENVOY_LOG(debug, "Called AuthenticationFilter : {}", __func__);
  ENVOY_LOG(debug,
            "Called AuthenticationFilter : {} FilterDataStatus::Continue;",
            __FUNCTION__);
  return FilterDataStatus::Continue;
}

FilterTrailersStatus AuthenticationFilter::decodeTrailers(HeaderMap&) {
  ENVOY_LOG(debug, "Called AuthenticationFilter : {}", __func__);
  if (state_ == IstioAuthN::State::PROCESSING) {
    return FilterTrailersStatus::StopIteration;
  }
  return FilterTrailersStatus::Continue;
}

void AuthenticationFilter::setDecoderFilterCallbacks(
    StreamDecoderFilterCallbacks& callbacks) {
  ENVOY_LOG(debug, "Called AuthenticationFilter : {}", __func__);
  decoder_callbacks_ = &callbacks;
}

}  // namespace Http
}  // namespace Envoy
