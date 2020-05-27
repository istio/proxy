/* Copyright 2020 Istio Authors. All Rights Reserved.
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

#include <cstdlib>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "envoy/http/header_map.h"
#include "src/envoy/http/authn_wasm/authenticator/base.h"

namespace Envoy {
namespace Wasm {
namespace Http {
namespace AuthN {

namespace {
// The default header name for an exchanged token
static constexpr absl::string_view kExchangedTokenHeaderName =
    "ingress-authorization";

// Returns whether the header for an exchanged token is found
bool FindHeaderOfExchangedToken(
    const istio::authentication::v1alpha1::Jwt& jwt) {
  return (jwt.jwt_headers_size() == 1 &&
          Envoy::Http::LowerCaseString(kExchangedTokenHeaderName.data()) ==
              Envoy::Http::LowerCaseString(jwt.jwt_headers(0)));
}

}  // namespace

AuthenticatorBase::AuthenticatorBase(FilterContextPtr filter_context)
    : filter_context_(filter_context) {}

AuthenticatorBase::~AuthenticatorBase() {}

bool AuthenticatorBase::validateTrustDomain() const {
  const auto& conn_context = filter_context_->connectionContext();
  if (conn_context.peerCertificateInfo() == nullptr) {
    logError(absl::StrCat(
        "trust domain validation failed: failed to extract peer certificate ",
        "info"));
    return false;
  }

  const auto& peer_trust_domain =
      conn_context.peerCertificateInfo()->getTrustDomain();
  if (!peer_trust_domain.has_value()) {
    logError("trust domain validation failed: cannot get peer trust domain");
    return false;
  }

  if (conn_context.localCertificateInfo() == nullptr) {
    logError(
        absl::StrCat("trust domain validation failed: failed to extract local ",
                     "certificate info"));
    return false;
  }

  const auto& local_trust_domain =
      conn_context.localCertificateInfo()->getTrustDomain();
  if (!local_trust_domain.has_value()) {
    logError("trust domain validation failed: cannot get local trust domain");
    return false;
  }

  if (peer_trust_domain.value() != local_trust_domain.value()) {
    logError(absl::StrCat("trust domain validation failed: peer trust domain ",
                          peer_trust_domain.value()));
    logError(absl::StrCat("different from local trust domain ",
                          local_trust_domain.value()));
    return false;
  }

  logDebug("trust domain validation succeeded");
  return true;
}

bool AuthenticatorBase::validateX509(
    const istio::authentication::v1alpha1::MutualTls& mtls,
    istio::authn::Payload* payload) const {
  bool has_user;
  const auto& conn_context = filter_context_->connectionContext();
  const bool presented = conn_context.peerCertificateInfo() != nullptr &&
                         conn_context.peerCertificateInfo()->presented();

  if (conn_context.peerCertificateInfo() != nullptr) {
    const auto principal = conn_context.peerCertificateInfo()->getPrincipal();
    if (principal.has_value()) {
      *(payload->mutable_x509()->mutable_user()) = principal.value();
    }
    has_user = presented && principal.has_value();
  }

  logDebug(absl::StrCat(
      "validateX509 mode: ",
      istio::authentication::v1alpha1::MutualTls::Mode_Name(mtls.mode())));
  logDebug(absl::StrCat("validateX509 ssl: ", conn_context.isTls()));
  logDebug(absl::StrCat("validateX509 has_user: ", has_user));

  if (!has_user) {
    // For plaintext connection, return value depend on mode:
    // - PERMISSIVE: always true.
    // - STRICT: always false.
    switch (mtls.mode()) {
      case istio::authentication::v1alpha1::MutualTls::PERMISSIVE:
        return true;
      case istio::authentication::v1alpha1::MutualTls::STRICT:
        return false;
      default:
        logError("should not be reached to this section.");
        abort();
    }
  }

  if (filter_context_->filterConfig().skip_validate_trust_domain()) {
    logDebug("trust domain validation skipped");
    return true;
  }

  // For TLS connection with valid certificate, validate trust domain for both
  // PERMISSIVE and STRICT mode.
  return validateTrustDomain();
}

// TODO(shikugawa): implement validateJWT
bool AuthenticatorBase::validateJwt(
    const istio::authentication::v1alpha1::Jwt& params,
    istio::authn::Payload* payload) {
  return true;
}

}  // namespace AuthN
}  // namespace Http
}  // namespace Wasm
}  // namespace Envoy
