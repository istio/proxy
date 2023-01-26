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

#include "source/extensions/filters/http/authn/origin_authenticator.h"

#include "absl/strings/match.h"
#include "authentication/v1alpha1/policy.pb.h"
#include "source/common/http/headers.h"
#include "source/common/http/utility.h"
#include "source/extensions/filters/http/authn/authn_utils.h"

using istio::authn::Payload;

namespace iaapi = istio::authentication::v1alpha1;

namespace Envoy {
namespace Http {
namespace Istio {
namespace AuthN {

Http::RegisterCustomInlineHeader<Http::CustomInlineHeaderRegistry::Type::RequestHeaders>
    access_control_request_method_handle(Http::CustomHeaders::get().AccessControlRequestMethod);
Http::RegisterCustomInlineHeader<Http::CustomInlineHeaderRegistry::Type::RequestHeaders>
    origin_handle(Http::CustomHeaders::get().Origin);

bool isCORSPreflightRequest(const Http::RequestHeaderMap& headers) {
  return headers.Method() &&
         headers.Method()->value().getStringView() == Http::Headers::get().MethodValues.Options &&
         !headers.getInlineValue(origin_handle.handle()).empty() &&
         !headers.getInlineValue(access_control_request_method_handle.handle()).empty();
}

OriginAuthenticator::OriginAuthenticator(FilterContext* filter_context, const iaapi::Policy& policy)
    : AuthenticatorBase(filter_context), policy_(policy) {}

bool OriginAuthenticator::run(Payload* payload) {
  if (policy_.origins_size() == 0 &&
      policy_.principal_binding() == iaapi::PrincipalBinding::USE_ORIGIN) {
    // Validation should reject policy that have rule to USE_ORIGIN but
    // does not provide any origin method so this code should
    // never reach. However, it's ok to treat it as authentication
    // fails.
    ENVOY_LOG(warn,
              "Principal is binded to origin, but no method specified in "
              "policy {}",
              policy_.DebugString());
    return false;
  }

  if (isCORSPreflightRequest(filter_context()->headerMap())) {
    // The CORS preflight doesn't include user credentials, allow regardless of
    // JWT policy. See
    // http://www.w3.org/TR/cors/#cross-origin-request-with-preflight.
    ENVOY_LOG(debug, "CORS preflight request allowed regardless of JWT policy");
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
    ENVOY_LOG(trace, "Got request path {}", path);
  } else {
    ENVOY_LOG(error, "Failed to get request path, JWT will always be used for "
                     "validation");
  }

  bool triggered = false;
  bool triggered_success = false;
  for (const auto& method : policy_.origins()) {
    const auto& jwt = method.jwt();

    if (AuthnUtils::ShouldValidateJwtPerPath(path, jwt)) {
      ENVOY_LOG(debug, "Validating request path {} for jwt {}", path, jwt.DebugString());
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
    filter_context()->setOriginResult(payload);
    filter_context()->setPrincipal(policy_.principal_binding());
    ENVOY_LOG(debug, "Origin authenticator succeeded");
    return true;
  }

  ENVOY_LOG(debug, "Origin authenticator failed");
  return false;
}

} // namespace AuthN
} // namespace Istio
} // namespace Http
} // namespace Envoy
