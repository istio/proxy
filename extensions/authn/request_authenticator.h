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

#pragma once

#include "extensions/authn/authn_utils.h"
#include "extensions/authn/filter_context.h"
#include "security/v1beta1/request_authentication.pb.h"
#include "src/istio/authn/context.pb.h"

// WASM_PROLOG
#ifndef NULL_PLUGIN

#include "proxy_wasm_intrinsics.h"

#else  // NULL_PLUGIN

#include "include/proxy-wasm/null_plugin.h"

namespace proxy_wasm {
namespace null_plugin {
namespace AuthN {

#endif  // NULL_PLUGIN

class IRequestAuthenticator {
public:
    virtual ~IRequestAuthenticator() = default;

    // Validates JWT given the jwt params. If JWT is validated, it will extract
    // attributes and claims (JwtPayload), returns status SUCCESS.
    // Otherwise, returns status FAILED.
    virtual bool validateJwt(istio::authn::JwtPayload* jwt) PURE;
};

// RequestAuthenticator performs origin authentication for given credential rule.
class RequestAuthenticator : public IRequestAuthenticator {
 public:
  RequestAuthenticator(
      FilterContextPtr filter_context,
      const istio::security::v1beta1::RequestAuthentication& policy);

  // IRequestAuthenticator
  bool validateJwt(istio::authn::JwtPayload* jwt) override;

  // Perform authentication.
  bool run(istio::authn::Payload* payload);

 private:
  // Reference to the authentication policy that the authenticator should
  // enforce. Typically, the actual object is owned by filter.
  const istio::security::v1beta1::RequestAuthentication
      request_authentication_policy_;

  // Pointer to filter state. Do not own.
  FilterContextPtr filter_context_;
};

#ifdef NULL_PLUGIN
}  // namespace AuthN
}  // namespace null_plugin
}  // namespace proxy_wasm
#endif