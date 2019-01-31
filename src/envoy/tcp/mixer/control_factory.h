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

#include "envoy/local_info/local_info.h"
#include "src/envoy/tcp/mixer/control.h"

namespace Envoy {
namespace Tcp {
namespace Mixer {
namespace {

// Envoy stats perfix for TCP filter stats.
const std::string kTcpStatsPrefix("tcp_mixer_filter.");

}  // namespace

class ControlFactory : public Logger::Loggable<Logger::Id::filter> {
 public:
  ControlFactory(std::unique_ptr<Config> config,
                 Server::Configuration::FactoryContext& context)
      : control_data_(std::make_shared<ControlData>(
            std::move(config), generateStats(kTcpStatsPrefix, context.scope()),
            context.random().uuid())),
        tls_(context.threadLocal().allocateSlot()) {
    Runtime::RandomGenerator& random = context.random();
    Stats::Scope& scope = context.scope();
    const LocalInfo::LocalInfo& local_info = context.localInfo();

    tls_->set([control_data = this->control_data_,
               &cm = context.clusterManager(), &random, &scope,
               &local_info](Event::Dispatcher& dispatcher)
                  -> ThreadLocal::ThreadLocalObjectSharedPtr {
      return ThreadLocal::ThreadLocalObjectSharedPtr(
          new Control(control_data, cm, dispatcher, random, scope, local_info));
    });
  }

  // Get the per-thread control
  Control& control() { return tls_->getTyped<Control>(); }

 private:
  // Generates stats struct.
  static Utils::MixerFilterStats generateStats(const std::string& name,
                                               Stats::Scope& scope) {
    return {ALL_MIXER_FILTER_STATS(POOL_COUNTER_PREFIX(scope, name))};
  }

  // The control data object
  ControlDataSharedPtr control_data_;
  // the thread local slots
  ThreadLocal::SlotPtr tls_;
};

}  // namespace Mixer
}  // namespace Tcp
}  // namespace Envoy
