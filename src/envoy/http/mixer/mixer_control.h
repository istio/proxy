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

#pragma once

#include "envoy/event/dispatcher.h"
#include "envoy/runtime/runtime.h"
#include "envoy/thread_local/thread_local.h"
#include "envoy/upstream/cluster_manager.h"
#include "include/istio/control/http/controller.h"
#include "src/envoy/http/mixer/config.h"
#include "src/envoy/utils/grpc_transport.h"
#include "src/envoy/utils/mixer_control.h"
#include "src/envoy/utils/stats.h"

namespace Envoy {
namespace Http {
namespace Mixer {

class HttpMixerControl final : public ThreadLocal::ThreadLocalObject {
 public:
  // The constructor.
  HttpMixerControl(const HttpMixerConfig& mixer_config,
                   Upstream::ClusterManager& cm, Stats::Scope& scope,
                   Event::Dispatcher& dispatcher,
                   Runtime::RandomGenerator& random,
                   Utils::MixerFilterStats& stats)
      : config_(mixer_config),
        check_client_factory_(Utils::asyncClientFactoryForCluster(
            cm, config_.check_cluster(), scope)),
        report_client_factory_(Utils::asyncClientFactoryForCluster(
            cm, config_.report_cluster(), scope)),
        stats_obj_(dispatcher, stats,
                   config_.http_config().transport().stats_update_interval(),
                   [this](::istio::mixerclient::Statistics* stat) -> bool {
                     return GetStats(stat);
                   }) {
    ::istio::control::http::Controller::Options options(config_.http_config());

    Utils::CreateEnvironment(dispatcher, random, check_client_factory_.get(),
                             report_client_factory_.get(), &options.env);

    controller_ = ::istio::control::http::Controller::Create(options);
  }

  ::istio::control::http::Controller* controller() { return controller_.get(); }

  Utils::CheckTransport::Func GetCheckTransport(const HeaderMap* headers) {
    return Utils::CheckTransport::GetFunc(check_client_factory_.get(), headers);
  }

 private:
  // Call controller to get statistics.
  bool GetStats(::istio::mixerclient::Statistics* stat) {
    if (!controller_) {
      return false;
    }
    controller_->GetStatistics(stat);
    return true;
  }

  // The mixer config.
  const HttpMixerConfig& config_;

  // gRPC async client factories for check and report
  Grpc::AsyncClientFactoryPtr check_client_factory_;
  Grpc::AsyncClientFactoryPtr report_client_factory_;

  // The mixer control
  std::unique_ptr<::istio::control::http::Controller> controller_;
  // The stats object.
  Utils::MixerStatsObject stats_obj_;
};

}  // namespace Mixer
}  // namespace Http
}  // namespace Envoy
