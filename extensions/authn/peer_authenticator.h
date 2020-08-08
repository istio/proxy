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
#include "extensions/authn/connection_context.h"
#include "extensions/authn/filter_context.h"
#include "security/v1beta1/peer_authentication.pb.h"
#include "src/istio/authn/context.pb.h"

namespace Extensions {
namespace AuthN {

class PeerAuthenticator {
 public:
  virtual ~PeerAuthenticator() = default;

  // Validate TLS/MTLS connection and extract authenticated attributes (just
  // source user identity for now). Unlike mTLS, TLS connection does not require
  // a client certificate..
  virtual bool validateX509(
      istio::authn::X509Payload* payload,
      const istio::security::v1beta1::PeerAuthentication::MutualTLS&
          mtls_policy) PURE;
};

// PeerAuthenticator performs mTLS authentication for given credential
// rule.
class PeerAuthenticatorImpl : public PeerAuthenticator {
 public:
  PeerAuthenticatorImpl(
      FilterContextPtr filter_context,
      const istio::security::v1beta1::PeerAuthentication& policy);

  // IRequestAuthenticator
  bool validateX509(
      istio::authn::X509Payload* payload,
      const istio::security::v1beta1::PeerAuthentication::MutualTLS&
          mtls_policy) override;

  // Perform authentication.
  bool run(istio::authn::Payload* payload);

 private:
  bool validateTrustDomain();

  // Reference to the authentication policy that the authenticator should
  // enforce. Typically, the actual object is owned by filter.
  const istio::security::v1beta1::PeerAuthentication
      peer_authentication_policy_;

  // Pointer to filter state. Do not own.
  FilterContextPtr filter_context_;
};

}  // namespace AuthN
}  // namespace Extensions
