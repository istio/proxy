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

#include "extensions/authn/request_authenticator.h"

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "common/http/headers.h"

using istio::authn::Payload;

// WASM_PROLOG
#ifndef NULL_PLUGIN

#include "proxy_wasm_intrinsics.h"

#else  // NULL_PLUGIN

#include "include/proxy-wasm/null_plugin.h"

namespace proxy_wasm {
namespace null_plugin {
namespace AuthN {

#endif  // NULL_PLUGIN

namespace {
// The default header name for an exchanged token
static const std::string kExchangedTokenHeaderName = "ingress-authorization";

// Returns whether the header for an exchanged token is found
bool FindHeaderOfExchangedToken(
    const istio::security::v1beta1::JWTRule& jwt_rule) {
  return (jwt_rule.from_headers_size() == 1 &&
          Envoy::Http::LowerCaseString(kExchangedTokenHeaderName) ==
              Envoy::Http::LowerCaseString(jwt_rule.from_headers(0).name()));
}
}  // namespace

Envoy::Http::RegisterCustomInlineHeader<
    Envoy::Http::CustomInlineHeaderRegistry::Type::RequestHeaders>
    access_control_request_method_handle(
        Envoy::Http::CustomHeaders::get().AccessControlRequestMethod);
Envoy::Http::RegisterCustomInlineHeader<
    Envoy::Http::CustomInlineHeaderRegistry::Type::RequestHeaders>
    origin_handle(Envoy::Http::CustomHeaders::get().Origin);

bool isCORSPreflightRequest(const Envoy::Http::RequestHeaderMap& headers) {
  return headers.Method() &&
         headers.Method()->value().getStringView() ==
             Envoy::Http::Headers::get().MethodValues.Options &&
         !headers.getInlineValue(origin_handle.handle()).empty() &&
         !headers.getInlineValue(access_control_request_method_handle.handle())
              .empty();
}

RequestAuthenticator::RequestAuthenticator(
    FilterContextPtr filter_context,
    const istio::security::v1beta1::RequestAuthentication& policy)
    : request_authentication_policy_(policy),filter_context_(filter_context) {}

bool RequestAuthenticator::run(Payload* payload) {
  if (isCORSPreflightRequest(filter_context_->headerMap())) {
    // The CORS preflight doesn't include user credentials, allow regardless of
    // JWT policy. See
    // http://www.w3.org/TR/cors/#cross-origin-request-with-preflight.
    // logDebug("CORS preflight request allowed regardless of JWT policy");
    return true;
  }

  absl::string_view path;
  if (filter_context_->headerMap().Path() != nullptr) {
    path = filter_context_->headerMap().Path()->value().getStringView();

    // Trim query parameters and/or fragment if present
    size_t offset = path.find_first_of("?#");
    if (offset != absl::string_view::npos) {
      path.remove_suffix(path.length() - offset);
    }
  }

  if (payload != nullptr &&
      payload->payload_case() == Payload::PayloadCase::kJwt) {
    if (validateJwt(payload->mutable_jwt())) {
      filter_context_->setOriginResult(payload);
      return true;
    }
  }

  return false;
}

bool RequestAuthenticator::validateJwt(istio::authn::JwtPayload* jwt) {
  for (const auto& jwt_rule : request_authentication_policy_.jwt_rules()) {
    std::string jwt_payload;
    if (!filter_context_->getJwtPayload(jwt_rule.issuer(), &jwt_payload)) {
      continue;
    }

    std::string payload_to_process = jwt_payload;
    std::string original_payload;
    if (FindHeaderOfExchangedToken(jwt_rule)) {
      if (!AuthnUtils::ExtractOriginalPayload(jwt_payload, &original_payload)) {
        // When the header of an exchanged token is found but the token
        // does not contain the claim of the original payload, it
        // is regarded as an invalid exchanged token.
        continue;
      }
      // When the header of an exchanged token is found and the token
      // contains the claim of the original payload, the original payload
      // is extracted and used as the token payload.
      payload_to_process = original_payload;
    }
    std::cout << payload_to_process << std::endl;
    if (AuthnUtils::ProcessJwtPayload(payload_to_process, jwt)) {
      return true;
    }
  }
  return false;
}

#ifdef NULL_PLUGIN
}  // namespace AuthN
}  // namespace null_plugin
}  // namespace proxy_wasm
#endif