/* Copyright Istio Authors. All Rights Reserved.
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

#include "source/extensions/filters/http/misdirected_request/misdirected_filter.h"

#include "absl/strings/match.h"

namespace Envoy {
namespace Http {
namespace MisdirectedRequest {

namespace {

constexpr absl::string_view DefaultMetadataNamespace = "istio.misdirected_request";
constexpr absl::string_view DefaultHostsField = "hosts";

// Strips an optional ":port" suffix from an :authority value. Hostnames are
// already lower-cased upstream, so no case folding is performed here. IPv6 literals
// (which contain ']') are returned unchanged.
absl::string_view stripPort(absl::string_view host) {
  if (host.find(']') != absl::string_view::npos) {
    return host;
  }
  const auto colon = host.rfind(':');
  if (colon != absl::string_view::npos) {
    return host.substr(0, colon);
  }
  return host;
}

} // namespace

void HostSet::add(absl::string_view pattern) {
  if (pattern == "*") {
    catch_all_ = true;
  } else if (absl::StartsWith(pattern, "*.")) {
    wildcards_.emplace_back(std::string(pattern.substr(1))); // ".suffix"
  } else if (!pattern.empty()) {
    exact_.emplace(pattern);
  }
}

absl::string_view HostSet::longestWildcard(absl::string_view host) const {
  absl::string_view best;
  for (const auto& suffix : wildcards_) {
    // host must end with ".suffix" and contain at least one additional label.
    if (host.size() > suffix.size() && absl::EndsWith(host, suffix) && suffix.size() > best.size()) {
      best = suffix;
    }
  }
  return best;
}

MisdirectedFilterConfig::MisdirectedFilterConfig(
    const ProtoFilterConfig& proto_config,
    const envoy::config::core::v3::Metadata& listener_metadata) {
  for (const auto& h : proto_config.own_hostnames()) {
    own_.add(h);
  }

  const std::string ns = proto_config.listener_metadata_namespace().empty()
                             ? std::string(DefaultMetadataNamespace)
                             : proto_config.listener_metadata_namespace();
  const std::string field = proto_config.hosts_field().empty()
                                ? std::string(DefaultHostsField)
                                : proto_config.hosts_field();

  const auto& filter_metadata = listener_metadata.filter_metadata();
  const auto ns_it = filter_metadata.find(ns);
  if (ns_it != filter_metadata.end()) {
    const auto field_it = ns_it->second.fields().find(field);
    if (field_it != ns_it->second.fields().end() && field_it->second.has_list_value()) {
      for (const auto& v : field_it->second.list_value().values()) {
        all_.add(v.string_value());
      }
    }
  }
}

bool MisdirectedFilterConfig::isMisdirected(absl::string_view host) const {
  // Find the most specific listener hostname (across the whole port) that matches
  // host, then decide whether THIS listener is that owner. Specificity order is
  // exact > longest wildcard > catch-all, matching Gateway API hostname precedence.

  // 1) Exact match wins: owned iff this listener also has the exact hostname.
  if (all_.hasExact(host)) {
    return !own_.hasExact(host);
  }
  // 2) Otherwise the longest matching wildcard wins.
  const absl::string_view all_wild = all_.longestWildcard(host);
  if (!all_wild.empty()) {
    const absl::string_view own_wild = own_.longestWildcard(host);
    // Owned iff this listener's best matching wildcard is as specific as the best
    // wildcard on the port; a shorter/absent own wildcard means a sibling owns it.
    return own_wild.size() < all_wild.size();
  }
  // 3) Otherwise the catch-all listener (if any) wins.
  if (all_.catchAll()) {
    return !own_.catchAll();
  }
  // 4) No listener matches host at all: not misdirected. Let routing return 404.
  return false;
}

Http::FilterHeadersStatus MisdirectedFilter::decodeHeaders(Http::RequestHeaderMap& headers, bool) {
  const absl::string_view host = stripPort(headers.getHostValue());
  if (host.empty()) {
    return Http::FilterHeadersStatus::Continue;
  }
  if (config_->isMisdirected(host)) {
    ENVOY_LOG(debug, "misdirected request for host '{}'; returning 421", host);
    decoder_callbacks_->sendLocalReply(Http::Code::MisdirectedRequest, "misdirected request",
                                       nullptr, absl::nullopt, "misdirected_request");
    return Http::FilterHeadersStatus::StopIteration;
  }
  return Http::FilterHeadersStatus::Continue;
}

} // namespace MisdirectedRequest
} // namespace Http
} // namespace Envoy
