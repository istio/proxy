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

#include "extensions/authn/peer_authenticator.h"

namespace Extensions {
namespace AuthN {

PeerAuthenticatorImpl::PeerAuthenticatorImpl(
    FilterContextPtr filter_context,
    const istio::security::v1beta1::PeerAuthentication& policy)
    : peer_authentication_policy_(policy), filter_context_(filter_context) {}

bool PeerAuthenticatorImpl::validateX509(
    istio::authn::X509Payload* payload,
    const istio::security::v1beta1::PeerAuthentication::MutualTLS&
        mtls_policy) {
  if (mtls_policy.mode() ==
      istio::security::v1beta1::PeerAuthentication::MutualTLS::DISABLE) {
    return true;
  }

  const auto principal_domain =
      filter_context_->connectionContext()->principalDomain(true);
  const bool has_user = filter_context_->connectionContext()->isMutualTls() &&
                        principal_domain.has_value();

  if (!has_user) {
    switch (mtls_policy.mode()) {
      case istio::security::v1beta1::PeerAuthentication::MutualTLS::UNSET:
      case istio::security::v1beta1::PeerAuthentication::MutualTLS::PERMISSIVE:
        return true;
      case istio::security::v1beta1::PeerAuthentication::MutualTLS::STRICT:
        return false;
      default:
        NOT_REACHED_GCOVR_EXCL_LINE;
    }
  }

  payload->set_user(principal_domain.value());

  return validateTrustDomain();
}

bool PeerAuthenticatorImpl::run(istio::authn::Payload* payload) {
  const auto local_port = filter_context_->connectionContext()->port();
  const auto port_level_mtls = peer_authentication_policy_.port_level_mtls();

  if (local_port.has_value()) {
    const auto mtls_policy = port_level_mtls.find(local_port.value());
    if (mtls_policy != port_level_mtls.end()) {
      if (validateX509(payload->mutable_x509(), mtls_policy->second)) {
        filter_context_->setPeerAuthenticationResult(payload);
        return true;
      }

      return false;
    }
  }

  if (validateX509(payload->mutable_x509(),
                   peer_authentication_policy_.mtls())) {
    filter_context_->setPeerAuthenticationResult(payload);
    return true;
  }

  return false;
}

bool PeerAuthenticatorImpl::validateTrustDomain() {
  const auto peer_trust_domain =
      filter_context_->connectionContext()->trustDomain(true);
  if (!peer_trust_domain.has_value()) {
    return false;
  }

  const auto local_trust_domain =
      filter_context_->connectionContext()->trustDomain(false);
  if (!local_trust_domain.has_value()) {
    return false;
  }

  if (peer_trust_domain.value() != local_trust_domain.value()) {
    return false;
  }

  return true;
}

}  // namespace AuthN
}  // namespace Extensions