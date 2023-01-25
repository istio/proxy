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

#pragma once

#include "authentication/v1alpha1/policy.pb.h"
#include "envoy/http/header_map.h"
#include "source/common/common/logger.h"
#include "source/common/common/utility.h"
#include "src/istio/authn/context.pb.h"

namespace iaapi = istio::authentication::v1alpha1;

namespace Envoy {
namespace Http {
namespace Istio {
namespace AuthN {

// AuthnUtils class provides utility functions used for authentication.
class AuthnUtils : public Logger::Loggable<Logger::Id::filter> {
public:
  // Parse JWT payload string (which typically is the output from jwt filter)
  // and populate JwtPayload object. Return true if input string can be parsed
  // successfully. Otherwise, return false.
  static bool ProcessJwtPayload(const std::string& jwt_payload_str,
                                istio::authn::JwtPayload* payload);

  // Parses the original_payload in an exchanged JWT.
  // Returns true if original_payload can be
  // parsed successfully. Otherwise, returns false.
  static bool ExtractOriginalPayload(const std::string& token, std::string* original_payload);

  // Returns true if str is matched to match.
  static bool MatchString(absl::string_view str, const iaapi::StringMatch& match);

  // Returns true if the jwt should be validated. It will check if the request
  // path is matched to the trigger rule in the jwt.
  static bool ShouldValidateJwtPerPath(absl::string_view path, const iaapi::Jwt& jwt);
};

} // namespace AuthN
} // namespace Istio
} // namespace Http
} // namespace Envoy
