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

#include "extensions/authn/connection_context.h"

#include "absl/strings/string_view.h"

namespace Extensions {
namespace AuthN {

namespace {
static constexpr absl::string_view kSpiffePrefix = "spiffe://";

bool hasSpiffePrefix(const std::string& url) {
  return url.find(kSpiffePrefix.data()) != std::string::npos;
}
}  // namespace

ConnectionContextImpl::ConnectionContextImpl(const Connection* connection)
    : connection_(connection) {}

absl::optional<std::string> ConnectionContextImpl::trustDomain(bool peer) {
  const auto cert_san = certSan(peer);
  if (!cert_san.has_value() || !hasSpiffePrefix(cert_san.value())) {
    return absl::nullopt;
  }

  // Skip the prefix "spiffe://" before getting trust domain.
  auto slash = cert_san->find('/', kSpiffePrefix.size());
  if (slash == std::string::npos) {
    return absl::nullopt;
  }

  auto domain_len = slash - kSpiffePrefix.size();
  return cert_san->substr(kSpiffePrefix.size(), domain_len);
}

absl::optional<std::string> ConnectionContextImpl::principalDomain(bool peer) {
  const auto cert_san = certSan(peer);
  if (cert_san.has_value()) {
    if (hasSpiffePrefix(cert_san.value())) {
      // Strip out the prefix "spiffe://" in the identity.
      return cert_san->substr(kSpiffePrefix.size());
    } else {
      return cert_san;
    }
  }
  return absl::nullopt;
}

bool ConnectionContextImpl::isMutualTls() const {
  return connection_ != nullptr && connection_->ssl() != nullptr &&
         connection_->ssl()->peerCertificatePresented();
}

absl::optional<uint32_t> ConnectionContextImpl::port() const {
  if (!connection_) {
    return absl::nullopt;
  }
  const auto local_address_instance = connection_->localAddress();
  assert(local_address_instance != nullptr);
  const auto ip = local_address_instance->ip();
  if (!ip) {
    return absl::nullopt;
  }
  return ip->port();
}

absl::optional<std::string> ConnectionContextImpl::certSan(bool peer) {
  if (!connection_) {
    return absl::nullopt;
  }
  const auto ssl = connection_->ssl();
  if (!ssl) {
    return absl::nullopt;
  }
  const auto& sans =
      (peer ? ssl->uriSanPeerCertificate() : ssl->uriSanLocalCertificate());
  if (sans.empty()) {
    // empty result is not allowed.
    return absl::nullopt;
  }
  std::string picked_san;
  // return the first san with the 'spiffe://' prefix
  for (const auto& san : sans) {
    if (hasSpiffePrefix(san)) {
      picked_san = san;
      break;
    }
  }

  if (picked_san.empty()) {
    picked_san = sans[0];
  }

  return picked_san;
}
}  // namespace AuthN
}  // namespace Extensions