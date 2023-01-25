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
#include "source/extensions/filters/http/common/factory_base.h"

namespace Envoy {
namespace Http {
namespace Alpn {

/**
 * Config registration for the alpn filter.
 */
class AlpnConfigFactory : public Server::Configuration::NamedHttpFilterConfigFactory {
public:
  // Server::Configuration::NamedHttpFilterConfigFactory
  Http::FilterFactoryCb
  createFilterFactoryFromProto(const Protobuf::Message& config, const std::string& stat_prefix,
                               Server::Configuration::FactoryContext& context) override;
  ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  std::string name() const override;

private:
  Http::FilterFactoryCb createFilterFactory(
      const istio::envoy::config::filter::http::alpn::v2alpha1::FilterConfig& config_pb,
      Upstream::ClusterManager& cluster_manager);
};

} // namespace Alpn
} // namespace Http
} // namespace Envoy
