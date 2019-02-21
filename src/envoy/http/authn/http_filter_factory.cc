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

#include "envoy/config/filter/http/authn/v2alpha1/config.pb.h"
#include "envoy/registry/registry.h"
#include "envoy/server/filter_config.h"
#include "google/protobuf/util/json_util.h"
#include "src/envoy/http/authn/http_filter.h"
#include "src/envoy/utils/filter_names.h"
#include "src/envoy/utils/utils.h"

using istio::envoy::config::filter::http::authn::v2alpha1::FilterConfig;

namespace Envoy {
namespace Server {
namespace Configuration {

class AuthnFilterConfig : public NamedHttpFilterConfigFactory,
                          public Logger::Loggable<Logger::Id::filter> {
 public:
  Http::FilterFactoryCb createFilterFactory(const Json::Object& config,
                                            const std::string&,
                                            FactoryContext&) override {
    ENVOY_LOG(debug, "Called AuthnFilterConfig : {}", __func__);
    FilterConfig filter_config;
    google::protobuf::util::Status status =
        Utils::ParseJsonMessage(config.asJsonString(), &filter_config);
    ENVOY_LOG(debug, "Called AuthnFilterConfig : Utils::ParseJsonMessage()");
    if (!status.ok()) {
      ENVOY_LOG(critical, "Utils::ParseJsonMessage() return value is: " +
                              status.ToString());
      throw EnvoyException(
          "In createFilterFactory(), Utils::ParseJsonMessage() return value "
          "is: " +
          status.ToString());
    }
    return createFilterFactory(filter_config);
  }

  Http::FilterFactoryCb createFilterFactoryFromProto(
      const Protobuf::Message& proto_config, const std::string&,
      FactoryContext&) override {
    auto filter_config = dynamic_cast<const FilterConfig&>(proto_config);
    return createFilterFactory(filter_config);
  }

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    ENVOY_LOG(debug, "Called AuthnFilterConfig : {}", __func__);
    return ProtobufTypes::MessagePtr{new FilterConfig};
  }

  std::string name() override {
    return Utils::IstioFilterName::kAuthentication;
  }

 private:
  Http::FilterFactoryCb createFilterFactory(const FilterConfig& config_pb) {
    ENVOY_LOG(debug, "Called AuthnFilterConfig : {}", __func__);
    // Make it shared_ptr so that the object is still reachable when callback is
    // invoked.
    // TODO(incfly): add a test to simulate different config can be handled
    // correctly similar to multiplexing on different port.
    auto filter_config = std::make_shared<FilterConfig>(config_pb);
    // Print a log to remind user to upgrade to the mTLS setting. This will only
    // be called when a new config is received by Envoy.
    warnPermissiveMode(filter_config);
    return
        [filter_config](Http::FilterChainFactoryCallbacks& callbacks) -> void {
          callbacks.addStreamDecoderFilter(
              std::make_shared<Http::Istio::AuthN::AuthenticationFilter>(
                  *filter_config));
        };
  }

  void warnPermissiveMode(const FilterConfig& filter_config) {
    for (const auto& method : filter_config.policy().peers()) {
      switch (method.params_case()) {
        case iaapi::PeerAuthenticationMethod::ParamsCase::kMtls:
          if (method.mtls().mode() == iaapi::MutualTls_Mode_PERMISSIVE) {
            ENVOY_LOG(
                warn,
                "mTLS PERMISSIVE mode is used, connection can be either "
                "plaintext or TLS, and client cert can be omitted. "
                "Please consider to upgrade to mTLS STRICT mode for more secure "
                "configuration that only allows TLS connection with client cert. "
                "See https://istio.io/docs/tasks/security/mtls-migration/");
            return;
          }
          break;
        default:
          break;
      }
    }
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
