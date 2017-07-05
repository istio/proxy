/* Copyright 2017 Istio Authors. All Rights Reserved.
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

#include "common/common/logger.h"
#include "envoy/network/connection.h"
#include "envoy/network/filter.h"
#include "envoy/server/instance.h"
#include "server/config/network/http_connection_manager.h"
#include "server/configuration_impl.h"
#include "src/envoy/mixer/config.h"

#include <map>

//using ::google::protobuf::util::Status;
//using StatusCode = ::google::protobuf::util::error::Code;

namespace Envoy {
namespace Tcp {
namespace Mixer {

  class Config : public Logger::Loggable<Logger::Id::filter> {
 private:
  Upstream::ClusterManager& cm_;

 public:
  Config(const Json::Object& config, Server::Instance& server)
      : cm_(server.clusterManager()) {}
};

typedef std::shared_ptr<Config> ConfigPtr;

class Instance : public Network::ReadFilter,
                 public Network::ConnectionCallbacks,
		 Logger::Loggable<Logger::Id::filter>,
                 public std::enable_shared_from_this<Instance> {
 private:
  ConfigPtr config_;
  Network::ReadFilterCallbacks* filter_callbacks_{};

 public:
  Instance(ConfigPtr config) : config_(config) {
    log().debug("Called Tcp Mixer::Instance : {}", __func__);
  }

  // Returns a shared pointer of this object.
  std::shared_ptr<Instance> GetPtr() { return shared_from_this(); }

  // Network::ReadFilter
  Network::FilterStatus onData(Buffer::Instance& data) override {
    conn_log_info("tcp filter on data: {}", filter_callbacks_->connection(), data.length());
    return Network::FilterStatus::Continue;
  }

  Network::FilterStatus onNewConnection() override {
    conn_log_info("new tcp connection", filter_callbacks_->connection());
    if (!filter_callbacks_->upstreamHost()) {
      conn_log_info("new tcp connection, no upstream", filter_callbacks_->connection());
    }
    return Network::FilterStatus::Continue;
  }

  void initializeReadFilterCallbacks(
      Network::ReadFilterCallbacks& callbacks) override {
    log().debug("Called Tcp Mixer::Instance : {}", __func__);
    filter_callbacks_ = &callbacks;
    filter_callbacks_->connection().addConnectionCallbacks(*this);
  }

  // Network::ConnectionCallbacks
  void onEvent(uint32_t events) override {
    log().debug("Called Tcp Mixer::Instance : {} ({})", __func__, events);
  }
};

}  // namespace Mixer
}  // namespace Tcp

namespace Server {
namespace Configuration {

class TcpMixerFilter : public NetworkFilterConfigFactory {
 public:
  NetworkFilterFactoryCb tryCreateFilterFactory(
      NetworkFilterType type, const std::string& name,
      const Json::Object& config, Server::Instance& server) override {
    if (type != NetworkFilterType::Read || name != "mixer") {
      return nullptr;
    }

    Tcp::Mixer::ConfigPtr mixer_config(new Tcp::Mixer::Config(config, server));
    return [mixer_config](Network::FilterManager& filter_manager) -> void {
      filter_manager.addReadFilter(
          Network::ReadFilterSharedPtr{new Tcp::Mixer::Instance(mixer_config)});
    };
  }
};

static RegisterNetworkFilterConfigFactory<TcpMixerFilter> register_;

}  // namespace Configuration
}  // namespace Server
}  // namespace Envoy
