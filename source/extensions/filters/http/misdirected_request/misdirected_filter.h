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

#pragma once

#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "envoy/config/core/v3/base.pb.h"
#include "source/extensions/filters/http/common/pass_through_filter.h"
#include "source/extensions/filters/http/misdirected_request/config.pb.h"

namespace Envoy {
namespace Http {
namespace MisdirectedRequest {

using ProtoFilterConfig =
    istio::envoy::config::filter::http::misdirected_request::v1alpha1::FilterConfig;

// HostSet holds a parsed collection of Gateway API hostname patterns split by
// specificity tier: exact hostnames, single-label wildcards ("*.suffix"), and a
// catch-all flag ("*"). This allows O(1) exact lookup and an O(#wildcards) scan
// for the longest matching wildcard.
class HostSet {
public:
  // Parses and stores a single pattern ("a.example.com", "*.example.com", or "*").
  void add(absl::string_view pattern);

  bool hasExact(absl::string_view host) const { return exact_.contains(host); }
  // Returns the longest matching wildcard suffix (".suffix") for host, or empty if
  // none matches.
  absl::string_view longestWildcard(absl::string_view host) const;
  bool catchAll() const { return catch_all_; }

private:
  absl::flat_hash_set<std::string> exact_;
  std::vector<std::string> wildcards_; // stored in ".suffix" form
  bool catch_all_{false};
};

// MisdirectedFilterConfig parses, once per listener at config time, the hostnames
// served by this listener ("own") and the full set of listener hostnames sharing
// the port ("all"). The latter is read from shared listener metadata so it is
// materialized once regardless of how many filter chains share the port.
class MisdirectedFilterConfig {
public:
  MisdirectedFilterConfig(const ProtoFilterConfig& proto_config,
                          const envoy::config::core::v3::Metadata& listener_metadata);

  // Returns true if a request for `host` is misdirected for this listener and must
  // receive an HTTP 421. Returns false to let normal routing proceed, which yields
  // HTTP 200 when a route serves `host` or HTTP 404 when none does.
  bool isMisdirected(absl::string_view host) const;

private:
  HostSet own_;
  HostSet all_;
};

using MisdirectedFilterConfigSharedPtr = std::shared_ptr<MisdirectedFilterConfig>;

class MisdirectedFilter : public Http::PassThroughDecoderFilter,
                          Logger::Loggable<Logger::Id::filter> {
public:
  explicit MisdirectedFilter(const MisdirectedFilterConfigSharedPtr& config) : config_(config) {}

  // Http::PassThroughDecoderFilter
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& headers,
                                          bool end_stream) override;

private:
  const MisdirectedFilterConfigSharedPtr config_;
};

} // namespace MisdirectedRequest
} // namespace Http
} // namespace Envoy
