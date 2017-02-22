/* Copyright 2016 Google Inc. All Rights Reserved.
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

#include "precompiled/precompiled.h"

#include "common/common/base64.h"
#include "common/common/logger.h"
#include "common/http/headers.h"
#include "common/http/utility.h"
#include "envoy/server/instance.h"
#include "server/config/network/http_connection_manager.h"
#include "src/envoy/mixer/utils.h"

namespace Http {
namespace AddHeader {
namespace {

// The Json object name to specify attributes to pass to
// next hop istio proxy.
const std::string kJsonNameAttributes("attributes");

}  // namespace

class Config : public Logger::Loggable<Logger::Id::http> {
 private:
  Upstream::ClusterManager& cm_;
  std::string attributes_;

 public:
  Config(const Json::Object& config, Server::Instance& server)
      : cm_(server.clusterManager()) {
    const auto& attributes =
        Utils::ExtractStringMap(config, kJsonNameAttributes);
    if (!attributes.empty()) {
      std::string serialized_str = Utils::SerializeStringMap(attributes);
      attributes_ =
          Base64::encode(serialized_str.c_str(), serialized_str.size());
    }
  }

  const std::string& attributes() const { return attributes_; }
};

typedef std::shared_ptr<Config> ConfigPtr;

class Instance : public Http::StreamDecoderFilter {
 private:
  ConfigPtr config_;

 public:
  Instance(ConfigPtr config) : config_(config) {}

  FilterHeadersStatus decodeHeaders(HeaderMap& headers,
                                    bool end_stream) override {
    if (!config_->attributes().empty()) {
      headers.addStatic(Utils::kHeaderNameIstioAttributes,
                        config_->attributes());
    }
    return FilterHeadersStatus::Continue;
  }

  FilterDataStatus decodeData(Buffer::Instance& data,
                              bool end_stream) override {
    return FilterDataStatus::Continue;
  }

  FilterTrailersStatus decodeTrailers(HeaderMap& trailers) override {
    return FilterTrailersStatus::Continue;
  }

  void setDecoderFilterCallbacks(
      StreamDecoderFilterCallbacks& callbacks) override {}
};

}  // namespace AddHeader
}  // namespace Http

namespace Server {
namespace Configuration {

class AddHeaderConfig : public HttpFilterConfigFactory {
 public:
  HttpFilterFactoryCb tryCreateFilterFactory(
      HttpFilterType type, const std::string& name, const Json::Object& config,
      const std::string&, Server::Instance& server) override {
    if (type != HttpFilterType::Decoder || name != "send_attribute") {
      return nullptr;
    }

    Http::AddHeader::ConfigPtr add_header_config(
        new Http::AddHeader::Config(config, server));
    return [add_header_config](
               Http::FilterChainFactoryCallbacks& callbacks) -> void {
      std::shared_ptr<Http::AddHeader::Instance> instance(
          new Http::AddHeader::Instance(add_header_config));
      callbacks.addStreamDecoderFilter(Http::StreamDecoderFilterPtr(instance));
    };
  }
};

static RegisterHttpFilterConfigFactory<AddHeaderConfig> register_;

}  // namespace Configuration
}  // namespace server
