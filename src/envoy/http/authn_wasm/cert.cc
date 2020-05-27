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

#include "src/envoy/http/authn_wasm/cert.h"

namespace Envoy {
namespace Wasm {
namespace Http {
namespace AuthN {

template <class Derived>
absl::optional<std::string> TlsCertificateInfo<Derived>::getCertSans() {
  const auto& uri_sans = static_cast<const Derived&>(*this).uriSans();
  for (const auto& uri_san : uri_sans) {
    if (absl::StartsWith(uri_san, kSPIFFEPrefix)) {
      return uri_san;
    }
  }
  if (!uri_sans.empty()) {
    return uri_sans[0];
  }
  return absl::nullopt;
}

template <class Derived>
absl::optional<std::string> TlsCertificateInfo<Derived>::getPrincipal() {
  const auto cert_sans = getCertSans();
  if (cert_sans.has_value()) {
    if (absl::StartsWith(cert_sans, kSPIFFEPrefix)) {
      return cert_sans.value().substr(kSPIFFEPrefix.size());
    }
    return cert_sans.value();
  }
  return absl::nullopt;
}

template <class Derived>
absl::optional<std::string> TlsCertificateInfo<Derived>::getTrustDomain() {
  const auto cert_san = getCertSans();
  if (!cert_san.has_value() ||
      !absl::StartsWith(cert_san.value(), kSPIFFEPrefix)) {
    return absl::nullopt;
  }

  // Skip the prefix "spiffe://" before getting trust domain.
  size_t slash = cert_san.find('/', kSPIFFEPrefix.size());
  if (slash == std::string::npos) {
    return absl::nullopt;
  }

  size_t len = slash - kSPIFFEPrefix.size();
  return cert_san.substr(kSPIFFEPrefix.size(), len);
}

}  // namespace AuthN
}  // namespace Http
}  // namespace Wasm
}  // namespace Envoy