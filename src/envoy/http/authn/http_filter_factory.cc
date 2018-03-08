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
#include "src/envoy/http/authn/jwt_authn_store.h"
#include "src/envoy/http/authn/jwt_authn_utils.h"
#include "src/envoy/utils/utils.h"

namespace Envoy {
namespace Server {
namespace Configuration {

namespace {
// The name for the Istio authentication filter.
const std::string kAuthnFactoryName("istio_authn");
}  // namespace

class AuthnFilterConfig : public NamedHttpFilterConfigFactory,
                          public Logger::Loggable<Logger::Id::filter> {
 public:
  HttpFilterFactoryCb createFilterFactory(const Json::Object& config,
                                          const std::string&,
                                          FactoryContext& context) override {
    ENVOY_LOG(debug, "Called AuthnFilterConfig : {}", __func__);

    google::protobuf::util::Status status =
        Utils::ParseJsonMessage(config.asJsonString(), &policy_);
    ENVOY_LOG(debug, "Called AuthnFilterConfig : Utils::ParseJsonMessage()");
    if (status.ok()) {
      return createFilter(context);
    } else {
      ENVOY_LOG(critical, "Utils::ParseJsonMessage() return value is: " +
                              status.ToString());
      throw EnvoyException(
          "In createFilterFactory(), Utils::ParseJsonMessage() return value "
          "is: " +
          status.ToString());
    }
  }

  HttpFilterFactoryCb createFilterFactoryFromProto(
      const Protobuf::Message& proto_config, const std::string&,
      FactoryContext& context) override {
    ENVOY_LOG(debug, "Called AuthnFilterConfig : {}", __func__);

    const istio::authentication::v1alpha1::Policy& policy =
        dynamic_cast<const istio::authentication::v1alpha1::Policy&>(
            proto_config);

    policy_ = policy;

    return createFilter(context);
  }

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    ENVOY_LOG(debug, "Called AuthnFilterConfig : {}", __func__);
    return ProtobufTypes::MessagePtr{
        new istio::authentication::v1alpha1::Policy};
  }

  std::string name() override { return kAuthnFactoryName; }

 private:
  HttpFilterFactoryCb createFilter(FactoryContext& context) {
    ENVOY_LOG(debug, "Called AuthnFilterConfig : {}", __func__);
    std::shared_ptr<Http::Istio::AuthN::JwtAuthnFactoryStore>
        jwt_factory_store =
            std::make_shared<Http::Istio::AuthN::JwtAuthnFactoryStore>(context);
    // Iterate through all peer rules for JWT configs
    for (int i = 0; i < policy_.peers_size(); i++) {
      auto m = policy_.peers(i);
      if (m.has_jwt()) {
        Http::JwtAuth::Config::AuthFilterConfig proto_config;
        Envoy::Http::Istio::AuthN::convertJwtAuthFormat(m.jwt(), &proto_config);
        jwt_factory_store->addToStore(proto_config);
      }
    }
    // Iterate through all CredentialRules for JWT configs
    for (int i = 0; i < policy_.credential_rules_size(); i++) {
      auto m = policy_.credential_rules(i);
      for (int j = 0; j < m.origins_size(); j++) {
        if (m.origins(j).has_jwt()) {
          Http::JwtAuth::Config::AuthFilterConfig proto_config;
          Envoy::Http::Istio::AuthN::convertJwtAuthFormat(m.origins(j).jwt(),
                                                          &proto_config);
          jwt_factory_store->addToStore(proto_config);
        }
      }
    }

    Upstream::ClusterManager& cm = context.clusterManager();
    return [&, jwt_factory_store](
               Http::FilterChainFactoryCallbacks& callbacks) -> void {
      callbacks.addStreamDecoderFilter(
          std::make_shared<Http::Istio::AuthN::AuthenticationFilter>(
              policy_, cm, jwt_factory_store->store()));
    };
  }

  istio::authentication::v1alpha1::Policy policy_;
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
