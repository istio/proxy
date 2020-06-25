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

#include "extensions/authn/authenticator_base.h"

#include "absl/strings/str_cat.h"
#include "common/common/assert.h"
#include "extensions/authn/authn_utils.h"
#include "src/envoy/utils/filter_names.h"
#include "src/envoy/utils/utils.h"

using istio::authn::Payload;

namespace iaapi = istio::authentication::v1alpha1;

// WASM_PROLOG
#ifndef NULL_PLUGIN

#include "proxy_wasm_intrinsics.h"

#else  // NULL_PLUGIN

#include "include/proxy-wasm/null_plugin.h"

namespace proxy_wasm {
namespace null_plugin {
namespace AuthN {

using proxy_wasm::null_plugin::logDebug;
using proxy_wasm::null_plugin::logError;
using proxy_wasm::null_plugin::logTrace;
using proxy_wasm::null_plugin::logWarn;

#endif  // NULL_PLUGIN

using Envoy::Http::LowerCaseString;
using Envoy::Utils::GetPrincipal;
using Envoy::Utils::GetTrustDomain;

namespace {
// The default header name for an exchanged token
static const std::string kExchangedTokenHeaderName = "ingress-authorization";

// Returns whether the header for an exchanged token is found
bool FindHeaderOfExchangedToken(const iaapi::Jwt& jwt) {
  return (jwt.jwt_headers_size() == 1 &&
          LowerCaseString(kExchangedTokenHeaderName) ==
              LowerCaseString(jwt.jwt_headers(0)));
}

}  // namespace

AuthenticatorBase::AuthenticatorBase(FilterContext* filter_context)
    : filter_context_(*filter_context) {}

AuthenticatorBase::~AuthenticatorBase() {}

bool AuthenticatorBase::validateTrustDomain(
    const Connection* connection) const {
  std::string peer_trust_domain;
  if (!GetTrustDomain(connection, true, &peer_trust_domain)) {
    logError("trust domain validation failed: cannot get peer trust domain");
    return false;
  }

  std::string local_trust_domain;
  if (!GetTrustDomain(connection, false, &local_trust_domain)) {
    logError("trust domain validation failed: cannot get local trust domain");
    return false;
  }

  if (peer_trust_domain != local_trust_domain) {
    logError(
        absl::StrCat("trust domain validation failed: peer trust domain {} "
                     "different from local trust domain {}",
                     peer_trust_domain, local_trust_domain));
    return false;
  }

  logDebug("trust domain validation succeeded");
  return true;
}

bool AuthenticatorBase::validateX509(const iaapi::MutualTls& mtls,
                                     Payload* payload) const {
  const Connection* connection = filter_context_.connection();
  if (connection == nullptr) {
    // It's wrong if connection does not exist.
    logError("validateX509 failed: null connection.");
    return false;
  }
  // Always try to get principal and set to output if available.
  const bool has_user =
      connection->ssl() != nullptr &&
      connection->ssl()->peerCertificatePresented() &&
      GetPrincipal(connection, true, payload->mutable_x509()->mutable_user());
  logDebug(absl::StrCat("validateX509 mode {}: ssl={}, has_user={}",
                        iaapi::MutualTls::Mode_Name(mtls.mode()),
                        connection->ssl() != nullptr, has_user));

  if (!has_user) {
    // For plaintext connection, return value depend on mode:
    // - PERMISSIVE: always true.
    // - STRICT: always false.
    switch (mtls.mode()) {
      case iaapi::MutualTls::PERMISSIVE:
        return true;
      case iaapi::MutualTls::STRICT:
        return false;
      default:
        NOT_REACHED_GCOVR_EXCL_LINE;
    }
  }

  if (filter_context_.filter_config().skip_validate_trust_domain()) {
    logDebug("trust domain validation skipped");
    return true;
  }

  // For TLS connection with valid certificate, validate trust domain for both
  // PERMISSIVE and STRICT mode.
  return validateTrustDomain(connection);
}

bool AuthenticatorBase::validateJwt(const iaapi::Jwt& jwt, Payload* payload) {
  std::string jwt_payload;
  if (filter_context()->getJwtPayload(jwt.issuer(), &jwt_payload)) {
    std::string payload_to_process = jwt_payload;
    std::string original_payload;
    if (FindHeaderOfExchangedToken(jwt)) {
      if (AuthnUtils::ExtractOriginalPayload(jwt_payload, &original_payload)) {
        // When the header of an exchanged token is found and the token
        // contains the claim of the original payload, the original payload
        // is extracted and used as the token payload.
        payload_to_process = original_payload;
      } else {
        // When the header of an exchanged token is found but the token
        // does not contain the claim of the original payload, it
        // is regarded as an invalid exchanged token.
        logError(absl::StrCat(
            "Expect exchanged-token with original payload claim. Received: {}",
            jwt_payload));
        return false;
      }
    }
    return AuthnUtils::ProcessJwtPayload(payload_to_process,
                                         payload->mutable_jwt());
  }
  return false;
}

#ifdef NULL_PLUGIN
}  // namespace AuthN
}  // namespace null_plugin
}  // namespace proxy_wasm
#endif
