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

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"

namespace Envoy {
namespace Wasm {
namespace Http {
namespace AuthN {

constexpr absl::string_view kSPIFFEPrefix = "spiffe://";

template <class Derived>
class TlsCertificateInfo {
 public:
  absl::optional<std::string> getCertSans();

  absl::optional<std::string> getPrincipal();

  absl::optional<std::string> getTrustDomain();
};

class TlsPeerCertificateInfo
    : public TlsCertificateInfo<TlsPeerCertificateInfo> {
 public:
  explicit TlsPeerCertificateInfo(std::string&& serial_number,
                                  std::string&& issuer, std::string&& subject,
                                  std::string&& sha256_digest,
                                  std::vector<std::string>&& uri_sans,
                                  std::vector<std::string>&& dns_sans,
                                  bool validated, bool presented)
      : serial_number_(std::move(serial_number)),
        issuer_(std::move(issuer)),
        subject_(std::move(subject)),
        sha256_digest_(std::move(sha256_digest)),
        uri_sans_(std::move(uri_sans)),
        dns_sans_(std::move(dns_sans)),
        validated_(validated),
        presented_(presented) {}
  const std::string& serialNumber() const { return serial_number_; }
  const std::string& issuer() const { return issuer_; }
  const std::string& subject() const { return subject_; }
  const std::string& sha256Digest() const { return sha256_digest_; }
  const std::vector<std::string>& uriSans() const { return uri_sans_; }
  const std::vector<std::string>& dnsSans() const { return dns_sans_; }
  const bool& validated() const { return validated_; }
  const bool& presented() const { return presented_; }

 private:
  const std::string serial_number_;
  const std::string issuer_;
  const std::string subject_;
  const std::string sha256_digest_;
  const std::vector<std::string> uri_sans_;
  const std::vector<std::string> dns_sans_;
  const bool validated_;
  const bool presented_;
};

class TlsLocalCertificateInfo
    : public TlsCertificateInfo<TlsLocalCertificateInfo> {
 public:
  explicit TlsLocalCertificateInfo(std::string&& subject,
                                   std::vector<std::string>&& uri_sans,
                                   std::vector<std::string>&& dns_sans)
      : subject_(std::move(subject)),
        uri_sans_(std::move(uri_sans)),
        dns_sans_(std::move(dns_sans)) {}
  const std::string& subject() const { return subject_; }
  const std::vector<std::string>& uriSans() const { return uri_sans_; }
  const std::vector<std::string>& dnsSans() const { return dns_sans_; }

 private:
  std::string subject_;
  std::vector<std::string> uri_sans_;
  std::vector<std::string> dns_sans_;
};

using TlsPeerCertificateInfoPtr = std::unique_ptr<TlsPeerCertificateInfo>;
using TlsLocalCertificateInfoPtr = std::unique_ptr<TlsLocalCertificateInfo>;

}  // namespace AuthN
}  // namespace Http
}  // namespace Wasm
}  // namespace Envoy