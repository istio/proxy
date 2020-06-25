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

#include "extensions/authn/peer_authenticator.h"

#include "absl/strings/str_cat.h"
#include "src/envoy/utils/utils.h"

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

PeerAuthenticator::PeerAuthenticator(FilterContext* filter_context,
                                     const iaapi::Policy& policy)
    : AuthenticatorBase(filter_context), policy_(policy) {}

bool PeerAuthenticator::run(Payload* payload) {
  bool success = false;
  if (policy_.peers_size() == 0) {
    logDebug("No method defined. Skip source authentication.");
    success = true;
    return success;
  }
  for (const auto& method : policy_.peers()) {
    switch (method.params_case()) {
      case iaapi::PeerAuthenticationMethod::ParamsCase::kMtls:
        success = validateX509(method.mtls(), payload);
        break;
      case iaapi::PeerAuthenticationMethod::ParamsCase::kJwt:
        success = validateJwt(method.jwt(), payload);
        break;
      default:
        logError(absl::StrCat("Unknown peer authentication param ",
                              method.DebugString()));
        success = false;
        break;
    }

    if (success) {
      break;
    }
  }

  if (success) {
    filter_context()->setPeerResult(payload);
  }

  return success;
}

#ifdef NULL_PLUGIN
}  // namespace AuthN
}  // namespace null_plugin
}  // namespace proxy_wasm
#endif