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

#include "src/envoy/http/authn_wasm/request.h"

#include "absl/strings/str_cat.h"
#include "authentication/v1alpha1/policy.pb.h"
#include "common/http/headers.h"

#ifdef NULL_PLUGIN

namespace proxy_wasm {
namespace null_plugin {
namespace Http {
namespace AuthN {

#endif

RequestAuthenticator::RequestAuthenticator(
    FilterContextPtr filter_context,
    const istio::authentication::v1alpha1::Policy& policy)
    : AuthenticatorBase(filter_context), policy_(policy) {}

RequestAuthenticator::run(istio::authn::Payload* payload) {
  if (policy_.origins_size() == 0 &&
      policy_.principal_binding() ==
          istio::authentication::v1alpha1::PrincipalBinding::USE_ORIGIN) {
    // Validation should reject policy that have rule to USE_ORIGIN but
    // does not provide any origin method so this code should
    // never reach. However, it's ok to treat it as authentication
    // fails.
    logWarn(absl::StrCat(
        "Principal is binded to origin, but no method specified in policy ",
        policy_.DebugString()));
    return false;
  }

  constexpr auto isCorsPreflightRequest =
      [](const Http::RequestHeaderMap& headers) -> bool {
    return headers.Method() &&
           headers.Method()->value().getStringView() ==
               Http::Headers::get().MethodValues.Options &&
           headers.Origin() && !headers.Origin()->value().empty() &&
           headers.AccessControlRequestMethod() &&
           !headers.AccessControlRequestMethod()->value().empty();
  };

  if (isCorsPreflightRequest(filterContext()->headerMap())) {
    // The CORS preflight doesn't include user credentials, allow regardless of
    // JWT policy. See
    // http://www.w3.org/TR/cors/#cross-origin-request-with-preflight.
    logDebug("CORS preflight request allowed regardless of JWT policy");
    return true;
  }

  absl::string_view path;
  if (filterContext()->headerMap().find(":path") != filterContext()->headerMap().end()) {
    path = filterContext()->headerMap().at(":path");

    size_t offset = path.find_first_of("?#");
    if (offset != absl::string_view::npos) {
      path.remove_suffix(path.length() - offset);
    }
    logTrace(absl::StrCat("Got request path {}", path));
  } else {
    logError(absl::StrCat("Failed to get request path, JWT will always be used for validation"));
  }

  bool triggered = false;
  bool triggered_success = false;
  for (const auto& method : policy_.origins()) {
    const auto& jwt = method.jwt();

    if (AuthnUtils::ShouldValidateJwtPerPath(path, jwt)) {
      logDebug("Validating request path ", path, " for jwt ", jwt.DebugString());
      // set triggered to true if any of the jwt trigger rule matched.
      triggered = true;
      if (validateJwt(jwt, payload)) {
        ENVOY_LOG(debug, "JWT validation succeeded");
        triggered_success = true;
        break;
      }
    }
  }

  // returns true if no jwt was triggered, or triggered and success.
  if (!triggered || triggered_success) {
    filterContext()->setOriginResult(payload);
    filterContext()->setPrincipal(policy_.principal_binding());
    logDebug("Origin authenticator succeeded");
    return true;
  }

  logDebug("Origin authenticator failed");
  return false;
}

#ifdef NULL_PLUGIN

}  // namespace AuthN
}  // namespace Http
}  // namespace null_plugin
}  // namespace proxy_wasm

#endif