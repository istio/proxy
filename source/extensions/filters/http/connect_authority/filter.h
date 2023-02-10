// Copyright Istio Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "source/extensions/filters/http/common/factory_base.h"
#include "source/extensions/filters/http/common/pass_through_filter.h"
#include "source/extensions/filters/network/common/factory_base.h"
#include "source/extensions/filters/http/connect_authority/config.pb.h"
#include "source/extensions/filters/http/connect_authority/config.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace ConnectAuthority {

class FilterConfig : public Router::RouteSpecificFilterConfig {
public:
  FilterConfig(const io::istio::http::connect_authority::Config& config)
      : enabled_(config.enabled()), port_(config.port()) {}
  bool enabled() const { return enabled_; }
  uint32_t port() const { return port_; }

private:
  const bool enabled_;
  const uint32_t port_;
};

class Filter : public Http::PassThroughFilter {
public:
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap&, bool) override;
};

class FilterConfigFactory : public Common::FactoryBase<io::istio::http::connect_authority::Config> {
public:
  FilterConfigFactory() : FactoryBase("envoy.filters.http.connect_authority") {}

private:
  Http::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const io::istio::http::connect_authority::Config&,
                                    const std::string&,
                                    Server::Configuration::FactoryContext&) override {
    return [](Http::FilterChainFactoryCallbacks& callbacks) {
      auto filter = std::make_shared<Filter>();
      callbacks.addStreamFilter(filter);
    };
  }
  Router::RouteSpecificFilterConfigConstSharedPtr
  createRouteSpecificFilterConfigTyped(const io::istio::http::connect_authority::Config& config,
                                       Envoy::Server::Configuration::ServerFactoryContext&,
                                       ProtobufMessage::ValidationVisitor&) override {
    return std::make_shared<FilterConfig>(config);
  }
};

class NetworkFilter : public Network::ReadFilter {
public:
  Network::FilterStatus onNewConnection() override;
  Network::FilterStatus onData(Buffer::Instance&, bool) override {
    return Network::FilterStatus::Continue;
  }
  void initializeReadFilterCallbacks(Network::ReadFilterCallbacks& callbacks) override {
    network_read_callbacks_ = &callbacks;
  }

private:
  Network::ReadFilterCallbacks* network_read_callbacks_;
};

class NetworkFilterConfigFactory
    : public NetworkFilters::Common::FactoryBase<io::istio::http::connect_authority::Config> {
public:
  NetworkFilterConfigFactory() : FactoryBase("envoy.filters.network.connect_authority") {}

private:
  Network::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const io::istio::http::connect_authority::Config&,
                                    Server::Configuration::FactoryContext&) override {
    return [](Network::FilterManager& filter_manager) {
      filter_manager.addReadFilter(std::make_shared<NetworkFilter>());
    };
  }
};

} // namespace ConnectAuthority
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
