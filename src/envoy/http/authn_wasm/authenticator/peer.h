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

#include <memory>

#include "src/envoy/http/authn_wasm/authenticator/base.h"

namespace Envoy {
namespace Wasm {
namespace Http {
namespace AuthN {

// PeerAuthenticator performs peer authentication for given policy.
class PeerAuthenticator : public AuthenticatorBase {
 public:
  static std::unique_ptr<PeerAuthenticator> create(
      FilterContextPtr filter_context) {
    return std::make_unique<PeerAuthenticator>(
        filter_context, filter_context->filterConfig().policy());
  }

  bool run(istio::authn::Payload*) override;

  PeerAuthenticator(FilterContextPtr filter_context,
                    const istio::authentication::v1alpha1::Policy& policy);

 private:
  // Reference to the authentication policy that the authenticator should
  // enforce. Typically, the actual object is owned by filter.
  const istio::authentication::v1alpha1::Policy& policy_;
};

using PeerAuthenticatorPtr = std::unique_ptr<PeerAuthenticator>;

}  // namespace AuthN
}  // namespace Http
}  // namespace Wasm
}  // namespace Envoy
