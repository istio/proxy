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

#pragma once

#include "common/common/logger.h"
#include "envoy/local_info/local_info.h"
#include "src/envoy/http/mixer/control.h"
#include "src/envoy/utils/stats.h"
#include "src/istio/utils/logger.h"

namespace Envoy {
namespace Http {
namespace Mixer {
namespace {

// Envoy stats perfix for HTTP filter stats.
const std::string kHttpStatsPrefix("http_mixer_filter.");

}  // namespace

// This object is globally per listener.
// HttpMixerControl is created per-thread by this object.
class ControlFactory : public Logger::Loggable<Logger::Id::config> {
 public:
  ControlFactory(std::unique_ptr<Config> config,
                 Server::Configuration::FactoryContext& context)
      : control_data_(std::make_shared<ControlData>(
            std::move(config),
            generateStats(kHttpStatsPrefix, context.scope()))),
        tls_(context.threadLocal().allocateSlot()) {
    Upstream::ClusterManager& cm = context.clusterManager();
    Runtime::RandomGenerator& random = context.random();
    Stats::Scope& scope = context.scope();
    const LocalInfo::LocalInfo& local_info = context.localInfo();

    tls_->set([control_data = this->control_data_, &cm, &random, &scope,
               &local_info](Event::Dispatcher& dispatcher)
                  -> ThreadLocal::ThreadLocalObjectSharedPtr {
      return std::make_shared<Control>(control_data, cm, dispatcher, random,
                                       scope, local_info);
    });

    // All MIXER_DEBUG(), MIXER_WARN(), etc log messages will be passed to
    // ENVOY_LOG().
    istio::utils::setLogger(std::make_unique<LoggerAdaptor>());
  }

  Control& control() { return tls_->getTyped<Control>(); }

 private:
  // Generates stats struct.
  static Utils::MixerFilterStats generateStats(const std::string& name,
                                               Stats::Scope& scope) {
    return {ALL_MIXER_FILTER_STATS(POOL_COUNTER_PREFIX(scope, name))};
  }

  class LoggerAdaptor : public istio::utils::Logger,
                        Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
    virtual bool isLoggable(istio::utils::Logger::Level level) override {
      switch (level) {
        case istio::utils::Logger::Level::DEBUG_:
          return ENVOY_LOG_CHECK_LEVEL(debug);
        case istio::utils::Logger::Level::TRACE_:
          return ENVOY_LOG_CHECK_LEVEL(trace);
        case istio::utils::Logger::Level::INFO_:
          return ENVOY_LOG_CHECK_LEVEL(info);
        case istio::utils::Logger::Level::WARN_:
          return ENVOY_LOG_CHECK_LEVEL(warn);
        case istio::utils::Logger::Level::ERROR_:
          return ENVOY_LOG_CHECK_LEVEL(error);
        default:
          NOT_REACHED_GCOVR_EXCL_LINE;
      }
    }

    virtual void writeBuffer(istio::utils::Logger::Level level,
                             const char* buffer) override {
      switch (level) {
        case istio::utils::Logger::Level::DEBUG_:
          ENVOY_LOGGER().debug(buffer);
          break;
        case istio::utils::Logger::Level::TRACE_:
          ENVOY_LOGGER().trace(buffer);
          break;
        case istio::utils::Logger::Level::INFO_:
          ENVOY_LOGGER().info(buffer);
          break;
        case istio::utils::Logger::Level::WARN_:
          ENVOY_LOGGER().warn(buffer);
          break;
        case istio::utils::Logger::Level::ERROR_:
          ENVOY_LOGGER().error(buffer);
      }
    }
  };

  // The control data object
  ControlDataSharedPtr control_data_;
  // Thread local slot.
  ThreadLocal::SlotPtr tls_;
};

}  // namespace Mixer
}  // namespace Http
}  // namespace Envoy
