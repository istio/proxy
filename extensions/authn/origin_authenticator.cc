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

#include "extensions/authn/origin_authenticator.h"

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "authentication/v1alpha1/policy.pb.h"
#include "common/http/headers.h"
#include "extensions/authn/authn_utils.h"

using istio::authn::Payload;

namespace iaapi = istio::authentication::v1alpha1;

// WASM_PROLOG
#ifndef NULL_PLUGIN

#include "proxy_wasm_intrinsics.h"

#else  // NULL_PLUGIN

#include "include/proxy-wasm/null_plugin.h"

using proxy_wasm::null_plugin::logDebug;
using proxy_wasm::null_plugin::logError;
using proxy_wasm::null_plugin::logTrace;
using proxy_wasm::null_plugin::logWarn;

namespace proxy_wasm {
namespace null_plugin {
namespace AuthN {

#endif  // NULL_PLUGIN

using Envoy::Http::Headers;
using Envoy::Http::RequestHeaderMap;

bool isCORSPreflightRequest(const RequestHeaderMap& headers) {
  return headers.Method() &&
         headers.Method()->value().getStringView() ==
             Headers::get().MethodValues.Options &&
         headers.Origin() && !headers.Origin()->value().empty() &&
         headers.AccessControlRequestMethod() &&
         !headers.AccessControlRequestMethod()->value().empty();
}

OriginAuthenticator::OriginAuthenticator(FilterContext* filter_context,
                                         const iaapi::Policy& policy)
    : AuthenticatorBase(filter_context), policy_(policy) {}

bool OriginAuthenticator::run(Payload* payload) {
  if (policy_.origins_size() == 0 &&
      policy_.principal_binding() == iaapi::PrincipalBinding::USE_ORIGIN) {
    // Validation should reject policy that have rule to USE_ORIGIN but
    // does not provide any origin method so this code should
    // never reach. However, it's ok to treat it as authentication
    // fails.
    logWarn(absl::StrCat(
        "Principal is binded to origin, but no method specified in "
        "policy {}",
        policy_.DebugString()));
    return false;
  }

  if (isCORSPreflightRequest(filter_context()->headerMap())) {
    // The CORS preflight doesn't include user credentials, allow regardless of
    // JWT policy. See
    // http://www.w3.org/TR/cors/#cross-origin-request-with-preflight.
    logDebug("CORS preflight request allowed regardless of JWT policy");
    return true;
  }

  absl::string_view path;
  if (filter_context()->headerMap().Path() != nullptr) {
    path = filter_context()->headerMap().Path()->value().getStringView();

    // Trim query parameters and/or fragment if present
    size_t offset = path.find_first_of("?#");
    if (offset != absl::string_view::npos) {
      path.remove_suffix(path.length() - offset);
    }
    logTrace(absl::StrCat("Got request path {}", path));
  } else {
    logError(
        "Failed to get request path, JWT will always be used for "
        "validation");
  }

  bool triggered = false;
  bool triggered_success = false;
  for (const auto& method : policy_.origins()) {
    const auto& jwt = method.jwt();

    if (AuthnUtils::ShouldValidateJwtPerPath(path, jwt)) {
      logDebug(absl::StrCat("Validating request path {} for jwt {}", path,
                            jwt.DebugString()));
      // set triggered to true if any of the jwt trigger rule matched.
      triggered = true;
      if (validateJwt(jwt, payload)) {
        logDebug("JWT validation succeeded");
        triggered_success = true;
        break;
      }
    }
  }

  // returns true if no jwt was triggered, or triggered and success.
  if (!triggered || triggered_success) {
    filter_context()->setOriginResult(payload);
    filter_context()->setPrincipal(policy_.principal_binding());
    logDebug("Origin authenticator succeeded");
    return true;
  }

  logDebug("Origin authenticator failed");
  return false;
}

#ifdef NULL_PLUGIN
}  // namespace AuthN
}  // namespace null_plugin
}  // namespace proxy_wasm
#endif