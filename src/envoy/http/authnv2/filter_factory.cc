/* Copyright 2018 Istio Authors. All Rights Reserved.
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

#include "envoy/registry/registry.h"
#include "envoy/server/filter_config.h"
#include "google/protobuf/util/json_util.h"
#include "src/envoy/http/authnv2/filter.h"
#include "src/envoy/utils/filter_names.h"
#include "src/envoy/utils/utils.h"

using istio::envoy::config::filter::http::authn::v2alpha1::FilterConfig;

namespace Envoy {
namespace Server {
namespace Configuration {

namespace iaapi = istio::authentication::v1alpha1;

class AuthnFilterConfig : public NamedHttpFilterConfigFactory,
                          public Logger::Loggable<Logger::Id::filter> {
 public:
  Http::FilterFactoryCb createFilterFactory(const Json::Object&,
                                            const std::string&,
                                            FactoryContext&) override {
    ENVOY_LOG(debug, "Called AuthnFilterConfig : {}", __func__);
    return createFilterFactory();
  }

  Http::FilterFactoryCb createFilterFactoryFromProto(const Protobuf::Message&,
                                                     const std::string&,
                                                     FactoryContext&) override {
    return createFilterFactory();
  }

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    ENVOY_LOG(debug, "Called AuthnFilterConfig : {}", __func__);
    return ProtobufTypes::MessagePtr{new FilterConfig};
  }

  std::string name() override {
    return Utils::IstioFilterName::kAuthentication;
  }

 private:
  Http::FilterFactoryCb createFilterFactory() {
    ENVOY_LOG(debug, "Called AuthnFilterConfig : {}", __func__);
    return [](Http::FilterChainFactoryCallbacks& callbacks) -> void {
      callbacks.addStreamDecoderFilter(
          std::make_shared<Http::Istio::AuthN::AuthenticationFilter>());
    };
  }
};

/**
 * Static registration for the Authn filter. @see RegisterFactory.
 */
static Registry::RegisterFactory<AuthnFilterConfig,
                                 NamedHttpFilterConfigFactory>
    register_;

}  // namespace Configuration
}  // namespace Server
}  // namespace Envoy
