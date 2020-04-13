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

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/strings/match.h"

namespace Envoy {
namespace Http {
namespace Istio {
namespace AuthN {

constexpr absl::string_view kSPIFFEPrefix = "spiffe://";

template <class Derived>
class TlsCertificateInfo {
public:
  absl::optional<std::string> getCertSans() {
    const auto& uri_sans = static_cast<const Derived&>(*this).uriSans();
    for (const auto& uri_san: uri_sans) {
      if (absl::StartsWith(uri_san, kSPIFFEPrefix)) {
        return uri_san;
      }
    }
    if (!uri_sans.empty()) {
      return uri_sans[0];
    }
    return absl::nullopt;
  }

  absl::optional<std::string> getPrincipal() {
    const auto cert_sans = getCertSans();
    if (cert_sans.has_value()) {
      if (absl::StartsWith(cert_sans, kSPIFFEPrefix)) {
        return cert_sans.value().substr(kSPIFFEPrefix.size());
      }
      return cert_sans.value();
    }
    return absl::nullopt;
  }

  absl::optional<std::string> getTrustDomain() {
    const auto cert_san = getCertSans();
    if (!cert_san.has_value() || !absl::StartsWith(cert_san.value(), kSPIFFEPrefix)) {
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
};

class TlsPeerCertificateInfo : public TlsCertificateInfo<TlsPeerCertificateInfo> {
public:
  // getter
  std::string& serialNumber() { return serial_number_; } 
  std::string& issuer() { return issuer_; } 
  std::string& subject() { return subject_; } 
  std::string& sha256Digest() { return sha256_digest_; } 
  bool& validated() { return validated_; }
  bool& presented() { return presented_; }
  std::string& uriSans() { return uri_sans_; }
  std::string& dnsSans() { return dns_sans_; }

private:
  std::string serial_number_;
  std::string issuer_;
  std::string subject_;
  std::string sha256_digest_;
  std::string uri_sans_;
  std::string dns_sans_;

  bool validated_;  
  bool presented_;
};

class TlsLocalCertificateInfo : public TlsCertificateInfo<TlsLocalCertificateInfo> {
public:  
  // getter
  const std::string& subject() { return subject_; } 
  const std::vector<std::string> uriSans() { return uri_sans_; }
  const std::vector<std::string> dnsSans() { return dns_sans_; }
  
private:
  std::string subject_;

  std::vector<std::string> uri_sans_;
  std::vector<std::string> dns_sans_;  
};

using TlsPeerCertificateInfoPtr = std::unique_ptr<TlsPeerCertificateInfo>;
using TlsLocalCertificateInfoPtr = std::unique_ptr<TlsLocalCertificateInfo>;

}
}
}
}