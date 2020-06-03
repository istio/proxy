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

#include "src/envoy/http/authn_wasm/base.h"

#include <cstdlib>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/json/json_loader.h"

#ifndef NULL_PLUGIN
#include "proxy_wasm_intrinsics.h"
#else
#include "include/proxy-wasm/null_plugin.h"

using proxy_wasm::null_plugin::logDebug;
using proxy_wasm::null_plugin::logError;

namespace proxy_wasm {
namespace null_plugin {
namespace Http {
namespace AuthN {

#endif

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

// The JWT audience key name
static constexpr absl::string_view kJwtAudienceKey = "aud";
// The JWT issuer key name
static constexpr absl::string_view kJwtIssuerKey = "iss";
// The key name for the original claims in an exchanged token
static constexpr absl::string_view kExchangedTokenOriginalPayload =
    "original_claims";

bool ExtractOriginalPayload(const std::string& token,
                            std::string* original_payload) {
  Envoy::Json::ObjectSharedPtr json_obj;
  try {
    json_obj = Json::Factory::loadFromString(token);
  } catch (...) {
    return false;
  }

  if (json_obj->hasObject(kExchangedTokenOriginalPayload) == false) {
    return false;
  }

  Envoy::Json::ObjectSharedPtr original_payload_obj;
  try {
    auto original_payload_obj =
        json_obj->getObject(kExchangedTokenOriginalPayload);
    *original_payload = original_payload_obj->asJsonString();
    logDebug(absl::StrCat(__FUNCTION__,
                          ": the original payload in exchanged token is ",
                          *original_payload));
  } catch (...) {
    logDebug(absl::StrCat(
        __FUNCTION__,
        ": original_payload in exchanged token is of invalid format."));
    return false;
  }

  return true;
}

bool ProcessJwtPayload(const std::string& payload_str,
                       istio::authn::JwtPayload* payload) {
  Envoy::Json::ObjectSharedPtr json_obj;
  try {
    json_obj = Json::Factory::loadFromString(payload_str);
    ENVOY_LOG(debug, "{}: json object is {}", __FUNCTION__,
              json_obj->asJsonString());
  } catch (...) {
    return false;
  }

  *payload->mutable_raw_claims() = payload_str;

  auto claims = payload->mutable_claims()->mutable_fields();
  // Extract claims as string lists
  json_obj->iterate([json_obj, claims](const std::string& key,
                                       const Json::Object&) -> bool {
    // In current implementation, only string/string list objects are extracted
    std::vector<std::string> list;
    ExtractStringList(key, *json_obj, &list);
    for (auto s : list) {
      (*claims)[key].mutable_list_value()->add_values()->set_string_value(s);
    }
    return true;
  });
  // Copy audience to the audience in context.proto
  if (claims->find(kJwtAudienceKey) != claims->end()) {
    for (const auto& v : (*claims)[kJwtAudienceKey].list_value().values()) {
      payload->add_audiences(v.string_value());
    }
  }

  // Build user
  if (claims->find(kJwtIssuerKey) != claims->end() &&
      claims->find("sub") != claims->end()) {
    payload->set_user(
        (*claims)[kJwtIssuerKey].list_value().values().Get(0).string_value() +
        "/" + (*claims)["sub"].list_value().values().Get(0).string_value());
  }
  // Build authorized presenter (azp)
  if (claims->find("azp") != claims->end()) {
    payload->set_presenter(
        (*claims)["azp"].list_value().values().Get(0).string_value());
  }

  return true;
}

}  // namespace

AuthenticatorBase::AuthenticatorBase(FilterContextPtr filter_context)
    : filter_context_(filter_context) {}

AuthenticatorBase::~AuthenticatorBase() {}

bool AuthenticatorBase::validateTrustDomain() const {
  const auto& conn_context = filter_context_->connectionContext();
  if (conn_context.peerCertificateInfo() == nullptr) {
    logError(
        "trust domain validation failed: failed to extract peer certificate "
        "info");
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
        "trust domain validation failed: failed to extract local certificate "
        "info");
    return false;
  }

  const auto& local_trust_domain =
      conn_context.localCertificateInfo()->getTrustDomain();
  if (!local_trust_domain.has_value()) {
    logError("trust domain validation failed: cannot get local trust domain");
    return false;
  }

  if (peer_trust_domain.value() != local_trust_domain.value()) {
    logError(absl::StrCat("trust domain validation failed: peer trust domain",
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
  return validateTrustDomain(conn_context);
}

bool AuthenticatorBase::validateJwt(
    const istio::authentication::v1alpha1::Jwt& jwt,
    istio::authn::Payload* payload) {
  auto jwt_payload = filterContext()->getJwtPayload(jwt.issuer());

  if (jwt_payload.has_value()) {
    std::string payload_to_process = jwt_payload;
    std::string original_payload;

    if (FindHeaderOfExchangedToken(jwt)) {
      if (ExtractOriginalPayload(jwt_payload, &original_payload)) {
        // When the header of an exchanged token is found and the token
        // contains the claim of the original payload, the original payload
        // is extracted and used as the token payload.
        payload_to_process = original_payload;
      } else {
        // When the header of an exchanged token is found but the token
        // does not contain the claim of the original payload, it
        // is regarded as an invalid exchanged token.
        logError(absl::StrCat(
            "Expect exchanged-token with original payload claim. Received: ",
            jwt_payload));
        return false;
      }
    }
    ProcessJwtPayload(payload_to_process, payload->mutable_jwt());
  }
  return false;
}

bool AuthenticatorBase::validateTrustDomain(
    const ConnectionContext& connection) const {
  auto peer_trust_domain = connection.peerCertificateInfo()->getTrustDomain();
  if (!peer_trust_domain.has_value()) {
    logError("trust domain validation failed: cannot get peer trust domain");
    return false;
  }

  auto local_trust_domain = connection.localCertificateInfo()->getTrustDomain();
  if (!local_trust_domain.has_value()) {
    logError("trust domain validation failed: cannot get local trust domain");
    return false;
  }

  if (peer_trust_domain.value() != local_trust_domain.value()) {
    logError(absl::StrCat("trust domain validation failed: peer trust domain ",
                          peer_trust_domain.value(),
                          " different from local trust domain ",
                          local_trust_domain.value()));
    return false;
  }

  logDebug("trust domain validation succeeded");
  return true;
}

#ifdef NULL_PLUGIN

}  // namespace AuthN
}  // namespace Http
}  // namespace null_plugin
}  // namespace proxy_wasm

#endif