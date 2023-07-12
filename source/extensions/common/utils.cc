/* Copyright 2017 Istio Authors. All Rights Reserved.
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

#include "source/extensions/common/utils.h"

#include "absl/strings/match.h"

using ::google::protobuf::Message;
using ::google::protobuf::Struct;

namespace Envoy {
namespace Utils {

namespace {

const std::string kSPIFFEPrefix("spiffe://");

// Per-host opaque data field
const std::string kPerHostMetadataKey("istio");

// Attribute field for per-host data override
const std::string kMetadataDestinationUID("uid");

const std::string kNamespaceKey("/ns/");
const char kDelimiter = '/';

bool hasSPIFFEPrefix(const std::string& san) { return absl::StartsWith(san, kSPIFFEPrefix); }

bool getCertSAN(const Network::Connection* connection, bool peer, std::string* principal) {
  if (connection) {
    const auto ssl = connection->ssl();
    if (ssl != nullptr) {
      const auto& sans = (peer ? ssl->uriSanPeerCertificate() : ssl->uriSanLocalCertificate());
      if (sans.empty()) {
        // empty result is not allowed.
        return false;
      }
      // return the first san with the 'spiffe://' prefix
      for (const auto& san : sans) {
        if (hasSPIFFEPrefix(san)) {
          *principal = san;
          return true;
        }
      }
      // return the first san if no sans have the spiffe:// prefix
      *principal = sans[0];
      return true;
    }
  }
  return false;
}

} // namespace

void ExtractHeaders(const Http::HeaderMap& header_map, const std::set<std::string>& exclusives,
                    std::map<std::string, std::string>& headers) {
  struct Context {
    Context(const std::set<std::string>& exclusives, std::map<std::string, std::string>& headers)
        : exclusives(exclusives), headers(headers) {}
    const std::set<std::string>& exclusives;
    std::map<std::string, std::string>& headers;
  };
  Context ctx(exclusives, headers);
  header_map.iterate([&ctx](const Http::HeaderEntry& header) -> Http::HeaderMap::Iterate {
    auto key = std::string(header.key().getStringView());
    auto value = std::string(header.value().getStringView());
    if (ctx.exclusives.count(key) == 0) {
      ctx.headers[key] = value;
    }
    return Http::HeaderMap::Iterate::Continue;
  });
}

void FindHeaders(const Http::HeaderMap& header_map, const std::set<std::string>& inclusives,
                 std::map<std::string, std::string>& headers) {
  struct Context {
    Context(const std::set<std::string>& inclusives, std::map<std::string, std::string>& headers)
        : inclusives(inclusives), headers(headers) {}
    const std::set<std::string>& inclusives;
    std::map<std::string, std::string>& headers;
  };
  Context ctx(inclusives, headers);
  header_map.iterate([&ctx](const Http::HeaderEntry& header) -> Http::HeaderMap::Iterate {
    auto key = std::string(header.key().getStringView());
    auto value = std::string(header.value().getStringView());
    if (ctx.inclusives.count(key) != 0) {
      ctx.headers[key] = value;
    }
    return Http::HeaderMap::Iterate::Continue;
  });
}

bool GetIpPort(const Network::Address::Ip* ip, std::string* str_ip, int* port) {
  if (ip) {
    *port = ip->port();
    if (ip->ipv4()) {
      uint32_t ipv4 = ip->ipv4()->address();
      *str_ip = std::string(reinterpret_cast<const char*>(&ipv4), sizeof(ipv4));
      return true;
    }
    if (ip->ipv6()) {
      absl::uint128 ipv6 = ip->ipv6()->address();
      *str_ip = std::string(reinterpret_cast<const char*>(&ipv6), 16);
      return true;
    }
  }
  return false;
}

bool GetDestinationUID(const envoy::config::core::v3::Metadata& metadata, std::string* uid) {
  const auto filter_it = metadata.filter_metadata().find(kPerHostMetadataKey);
  if (filter_it == metadata.filter_metadata().end()) {
    return false;
  }
  const Struct& struct_pb = filter_it->second;
  const auto fields_it = struct_pb.fields().find(kMetadataDestinationUID);
  if (fields_it == struct_pb.fields().end()) {
    return false;
  }
  *uid = fields_it->second.string_value();
  return true;
}

bool GetPrincipal(const Network::Connection* connection, bool peer, std::string* principal) {
  std::string cert_san;
  if (getCertSAN(connection, peer, &cert_san)) {
    if (hasSPIFFEPrefix(cert_san)) {
      // Strip out the prefix "spiffe://" in the identity.
      *principal = cert_san.substr(kSPIFFEPrefix.size());
    } else {
      *principal = cert_san;
    }
    return true;
  }
  return false;
}

bool GetTrustDomain(const Network::Connection* connection, bool peer, std::string* trust_domain) {
  std::string cert_san;
  if (!getCertSAN(connection, peer, &cert_san) || !hasSPIFFEPrefix(cert_san)) {
    return false;
  }

  // Skip the prefix "spiffe://" before getting trust domain.
  std::size_t slash = cert_san.find('/', kSPIFFEPrefix.size());
  if (slash == std::string::npos) {
    return false;
  }

  std::size_t len = slash - kSPIFFEPrefix.size();
  *trust_domain = cert_san.substr(kSPIFFEPrefix.size(), len);
  return true;
}

bool IsMutualTLS(const Network::Connection* connection) {
  return connection != nullptr && connection->ssl() != nullptr &&
         connection->ssl()->peerCertificatePresented();
}

bool GetRequestedServerName(const Network::Connection* connection, std::string* name) {
  if (connection && !connection->requestedServerName().empty()) {
    *name = std::string(connection->requestedServerName());
    return true;
  }

  return false;
}

absl::Status ParseJsonMessage(const std::string& json, Message* output) {
  ::google::protobuf::util::JsonParseOptions options;
  options.ignore_unknown_fields = true;
  return ::google::protobuf::util::JsonStringToMessage(json, output, options);
}

absl::optional<absl::string_view> GetNamespace(absl::string_view principal) {
  // The namespace is a substring in principal with format:
  // "<DOMAIN>/ns/<NAMESPACE>/sa/<SERVICE-ACCOUNT>". '/' is not allowed to
  // appear in actual content except as delimiter between tokens.
  size_t begin = principal.find(kNamespaceKey);
  if (begin == absl::string_view::npos) {
    return {};
  }
  begin += kNamespaceKey.length();
  size_t end = principal.find(kDelimiter, begin);
  size_t len = (end == std::string::npos ? end : end - begin);
  return {principal.substr(begin, len)};
}

} // namespace Utils
} // namespace Envoy
