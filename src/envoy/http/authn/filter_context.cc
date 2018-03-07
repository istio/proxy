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

#include "src/envoy/http/authn/filter_context.h"
#include "src/envoy/utils/utils.h"

namespace iaapi = istio::authentication::v1alpha1;

namespace Envoy {
namespace Http {

FilterContext::FilterContext() {}
FilterContext::~FilterContext() {}

void FilterContext::setPeerResult(const IstioAuthN::Payload* payload) {
  if (payload != nullptr) {
    if (payload->has_x509()) {
      result_.set_peer_user(payload->x509().user());
    } else if (payload->has_jwt()) {
      result_.set_peer_user(payload->jwt().user());
    } else {
      ENVOY_LOG(warn,
                "Source authentiation payload doesn't contain x509 nor jwt "
                "payload.");
    }
  }
}
void FilterContext::setOriginResult(const IstioAuthN::Payload* payload) {
  // Authentication pass, look at the return payload and store to the context
  // output. Set filter to continueDecoding when done.
  // At the moment, only JWT can be used for origin authentication, so
  // it's ok just to check jwt payload.
  if (payload != nullptr && payload->has_jwt()) {
    *result_.mutable_origin() = payload->jwt();
  }
}

void FilterContext::setPrincipal(iaapi::CredentialRule::Binding binding) {
  switch (binding) {
    case iaapi::CredentialRule::USE_PEER:
      result_.set_principal(result_.peer_user());
      return;
    case iaapi::CredentialRule::USE_ORIGIN:
      result_.set_principal(result_.origin().user());
      return;
    default:
      // Should never come here.
      ENVOY_LOG(error, "Invalid binding value {}", binding);
      return;
  }
}

void FilterContext::setHeaders(HeaderMap* headers) { headers_ = headers; }
HeaderMap* FilterContext::headers() { return headers_; }

// void FilterContext::setAuthenticator(
//     std::unique_ptr<AuthenticatorBase> authenticator) {
//   authenticator_ = std::move(authenticator);
// }
// AuthenticatorBase* FilterContext::authenticator() {
//   return authenticator_.get();
// }

}  // namespace Http
}  // namespace Envoy
