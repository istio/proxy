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

#include "src/envoy/http/authn/http_filter.h"
#include "envoy/registry/registry.h"
#include "google/protobuf/util/json_util.h"
#include "src/envoy/http/authn/policy.pb.validate.h"

namespace Envoy {
namespace Server {
namespace Configuration {

class AuthnFilterConfig : public NamedHttpFilterConfigFactory,
                          public Logger::Loggable<Logger::Id::http> {
 public:
  HttpFilterFactoryCb createFilterFactory(const Json::Object& config,
                                          const std::string&,
                                          FactoryContext& context) override {
    istio::authentication::v1alpha1::Policy proto_config;

    ENVOY_LOG(debug, "Called AuthnFilterConfig : {}", __func__);

    MessageUtil::loadFromJson(config.asJsonString(), proto_config);
    ENVOY_LOG(debug, "Called AuthnFilterConfig : loadFromJson()");

    return createFilter(proto_config, context);
  }

  HttpFilterFactoryCb createFilterFactoryFromProto(
      const Protobuf::Message& proto_config, const std::string&,
      FactoryContext& context) override {
    ENVOY_LOG(debug, "Called AuthnFilterConfig : {}", __func__);
    return createFilter(
        MessageUtil::downcastAndValidate<
            const istio::authentication::v1alpha1::Policy&>(proto_config),
        context);
  }

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    ENVOY_LOG(debug, "Called AuthnFilterConfig : {}", __func__);
    return ProtobufTypes::MessagePtr{
        new istio::authentication::v1alpha1::Policy};
  }

  std::string name() override { return "authN"; }

 private:
  HttpFilterFactoryCb createFilter(
      const istio::authentication::v1alpha1::Policy& proto_config,
      FactoryContext& context) {
    ENVOY_LOG(debug, "Called AuthnFilterConfig : {}", __func__);

    auto store_factory =
        std::make_shared<Http::Auth::AuthnStoreFactory>(proto_config, context);
    Upstream::ClusterManager& cm = context.clusterManager();
    return [&cm, store_factory](
               Http::FilterChainFactoryCallbacks& callbacks) -> void {
      callbacks.addStreamDecoderFilter(
          std::make_shared<Http::AuthnFilter>(cm, store_factory->store()));
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
