/* Copyright 2019 Istio Authors. All Rights Reserved.
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

#include "envoy/config/filter/http/alpn/v2alpha1/config.pb.h"
#include "source/extensions/filters/http/common/pass_through_filter.h"

namespace Envoy {
namespace Http {
namespace Alpn {

using AlpnOverrides = absl::flat_hash_map<Http::Protocol, std::vector<std::string>>;

class AlpnFilterConfig {
public:
  AlpnFilterConfig(
      const istio::envoy::config::filter::http::alpn::v2alpha1::FilterConfig& proto_config,
      Upstream::ClusterManager& cluster_manager);

  Upstream::ClusterManager& clusterManager() { return cluster_manager_; }

  const std::vector<std::string> alpnOverrides(const Http::Protocol& protocol) const {
    if (alpn_overrides_.count(protocol)) {
      return alpn_overrides_.at(protocol);
    }
    return {};
  }

private:
  Http::Protocol getHttpProtocol(
      const istio::envoy::config::filter::http::alpn::v2alpha1::FilterConfig::Protocol& protocol);

  AlpnOverrides alpn_overrides_;
  Upstream::ClusterManager& cluster_manager_;
};

using AlpnFilterConfigSharedPtr = std::shared_ptr<AlpnFilterConfig>;

class AlpnFilter : public Http::PassThroughDecoderFilter, Logger::Loggable<Logger::Id::filter> {
public:
  explicit AlpnFilter(const AlpnFilterConfigSharedPtr& config) : config_(config) {}

  // Http::PassThroughDecoderFilter
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& headers,
                                          bool end_stream) override;

private:
  const AlpnFilterConfigSharedPtr config_;
};

} // namespace Alpn
} // namespace Http
} // namespace Envoy
