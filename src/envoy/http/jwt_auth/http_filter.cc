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

#include "src/envoy/http/jwt_auth/http_filter.h"
#include "src/envoy/utils/constants.h"

#include "common/http/message_impl.h"
#include "common/http/codes.h"
#include "common/http/utility.h"
#include "envoy/http/async_client.h"

#include <chrono>
#include <string>

namespace Envoy {
namespace Http {

JwtVerificationFilter::JwtVerificationFilter(std::shared_ptr<Utils::Jwt::JwtAuthenticator> jwt_auth,
                                             const ::envoy::config::filter::http::jwt_authn::v2alpha::JwtAuthentication& config)
    : jwt_auth_(jwt_auth), config_(config) {}

JwtVerificationFilter::~JwtVerificationFilter() {}

void JwtVerificationFilter::onDestroy() {
  ENVOY_LOG(trace, "Called JwtVerificationFilter : {}", __func__);
  jwt_auth_->onDestroy();
  stream_reset_ = true;
}

FilterHeadersStatus JwtVerificationFilter::decodeHeaders(HeaderMap& headers,
                                                         bool) {
  ENVOY_LOG(trace, "Called JwtVerificationFilter : {}", __func__);
  state_ = Calling;
  stopped_ = false;
  stream_reset_ = false;
  headers_ = &headers;

  // Sanitize the JWT verification result in the HTTP headers
  // TODO (lei-tang): when the JWT verification result is in a configurable
  // header, need to sanitize based on the configuration.
  headers.remove(Utils::Constants::JwtPayloadKey());

  // Verify the JWT token, onDone() will be called when completed.
  jwt_auth_->Verify(headers, this);

  if (state_ == Complete) {
    ENVOY_LOG(trace, "Called JwtVerificationFilter : {} Complete", __func__);
    return FilterHeadersStatus::Continue;
  }
  ENVOY_LOG(trace, "Called JwtVerificationFilter : {} Stop", __func__);
  stopped_ = true;
  return FilterHeadersStatus::StopIteration;
}

void JwtVerificationFilter::onSuccess(const Utils::Jwt::Jwt *jwt, const Http::LowerCaseString *header) {
  ENVOY_LOG(trace, "Called JwtVerificationFilter : onSuccess {}");
  // This stream has been reset, abort the callback.
  if (state_ == Responded) {
    return;
  }
  state_ = Complete;
  // TODO(lei-tang): remove this backward compatibility.
  // Tracking issue: https://github.com/istio/istio/issues/4744
  headers_->addReferenceKey(Utils::Constants::JwtPayloadKey(), jwt->PayloadStrBase64Url());
  // Use the issuer field of the JWT to lookup forwarding rules.
  auto rules = config_.rules();
  auto rule = rules[jwt->Iss()];
  if (rule.has_forwarder()) {
    auto forwarding_rule = rule.forwarder();
    if (!forwarding_rule.forward_payload_header().empty()) {
      const Http::LowerCaseString key(
        forwarding_rule.forward_payload_header());
      if (key.get() != Utils::Constants::JwtPayloadKey().get()) {
        headers_->addCopy(key, jwt->PayloadStrBase64Url());
      }
    }
    if (!forwarding_rule.forward() && header) {
      // Remove JWT from headers.
      headers_->remove(*header);
    }
  }
  if (stopped_) {
    decoder_callbacks_->continueDecoding();
  }
}

void JwtVerificationFilter::onError(Utils::Jwt::Status status) {
  // This stream has been reset, abort the callback.
  ENVOY_LOG(trace, "Called JwtVerificationFilter : check onError {}",
            int(status));
  if (state_ != Calling) {
    return;
  }
  // Check if verification can be bypassed. If not error out.
  if (!OkToBypass()) {
    state_ = Responded;
    // verification failed
    Code code = Code(401);  // Unauthorized
    // Log failure reason but do not return in reply as we do not want to inadvertently leak potentially sensitive
    // JWT authentication configuration to an attacker.
    ENVOY_LOG(info, "JWT authentication failed: {}", Utils::Jwt::StatusToString(status));
    Utility::sendLocalReply(*decoder_callbacks_, stream_reset_, code, CodeUtility::toString(code));
  } else {
    ENVOY_LOG(debug, "Bypassing failed jwt authentication as defined by the jwt-auth filter's configuration.");
    state_ = Complete;
    if (stopped_) {
      decoder_callbacks_->continueDecoding();
    }
  }
}

FilterDataStatus JwtVerificationFilter::decodeData(Buffer::Instance&, bool) {
  ENVOY_LOG(trace, "Called JwtVerificationFilter : {}", __func__);
  if (state_ == Calling) {
    return FilterDataStatus::StopIterationAndWatermark;
  }
  return FilterDataStatus::Continue;
}

FilterTrailersStatus JwtVerificationFilter::decodeTrailers(HeaderMap&) {
  ENVOY_LOG(trace, "Called JwtVerificationFilter : {}", __func__);
  if (state_ == Calling) {
    return FilterTrailersStatus::StopIteration;
  }
  return FilterTrailersStatus::Continue;
}

void JwtVerificationFilter::setDecoderFilterCallbacks(
    StreamDecoderFilterCallbacks& callbacks) {
  ENVOY_LOG(trace, "Called JwtVerificationFilter : {}", __func__);
  decoder_callbacks_ = &callbacks;
}

bool JwtVerificationFilter::OkToBypass() const {
  // TODO: Use bypass field.
  return config_.allow_missing_or_failed();
}

}  // namespace Http
}  // namespace Envoy
