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

#include <chrono>
#include <memory>

#include "src/envoy/mixer/grpc_transport.h"
#include "src/envoy/mixer/mixer_control.h"
#include "src/envoy/mixer/stats.h"

namespace Envoy {
namespace Http {
namespace Mixer {
namespace {

// A class to wrap envoy timer for mixer client timer.
class EnvoyTimer : public ::istio::mixer_client::Timer {
 public:
  EnvoyTimer(Event::TimerPtr timer) : timer_(std::move(timer)) {}

  void Stop() override { timer_->disableTimer(); }
  void Start(int interval_ms) override {
    timer_->enableTimer(std::chrono::milliseconds(interval_ms));
  }

 private:
  Event::TimerPtr timer_;
};

// Create all environment functions.
void CreateEnvironment(Upstream::ClusterManager& cm,
                       Event::Dispatcher& dispatcher,
                       Runtime::RandomGenerator& random,
                       ::istio::mixer_client::Environment* env) {
  env->check_transport = CheckTransport::GetFunc(cm, nullptr);
  env->report_transport = ReportTransport::GetFunc(cm);

  env->timer_create_func = [&dispatcher](std::function<void()> timer_cb)
      -> std::unique_ptr<::istio::mixer_client::Timer> {
        return std::unique_ptr<::istio::mixer_client::Timer>(
            new EnvoyTimer(dispatcher.createTimer(timer_cb)));
      };

  env->uuid_generate_func = [&random]() -> std::string {
    return random.uuid();
  };
}

}  // namespace

HttpMixerControl::HttpMixerControl(const HttpMixerConfig& mixer_config,
                                   Upstream::ClusterManager& cm,
                                   Event::Dispatcher& dispatcher,
                                   Runtime::RandomGenerator& random,
                                   const std::string& stats_prefix,
                                   Stats::Scope& scope)
    : cm_(cm), stats_obj_(dispatcher, stats_prefix, scope) {
  ::istio::mixer_control::http::Controller::Options options(
      mixer_config.http_config, mixer_config.legacy_quotas);

  CreateEnvironment(cm, dispatcher, random, &options.env);

  controller_ = ::istio::mixer_control::http::Controller::Create(options);

  stats_obj_.InitGetStatisticsFunc(
      std::bind(&::istio::mixer_control::http::Controller::GetStatistics,
                controller(), std::placeholders::_1));

  // Initialize old_stats for envoy stats update.
  stats_obj_.GetStatistics(stats_obj_.mutate_old_stats());
  stats_obj_.SetUpStatsTimer();

  has_v2_config_ = mixer_config.has_v2_config;
}

TcpMixerControl::TcpMixerControl(const TcpMixerConfig& mixer_config,
                                 Upstream::ClusterManager& cm,
                                 Event::Dispatcher& dispatcher,
                                 Runtime::RandomGenerator& random,
                                 const std::string& stats_prefix,
                                 Stats::Scope& scope)
    : stats_obj_(dispatcher, stats_prefix, scope) {
  ::istio::mixer_control::tcp::Controller::Options options(
      mixer_config.tcp_config);

  CreateEnvironment(cm, dispatcher, random, &options.env);

  controller_ = ::istio::mixer_control::tcp::Controller::Create(options);

  stats_obj_.InitGetStatisticsFunc(
      std::bind(&::istio::mixer_control::tcp::Controller::GetStatistics,
                controller(), std::placeholders::_1));

  // Initialize old_stats for envoy stats update.
  stats_obj_.GetStatistics(stats_obj_.mutate_old_stats());
  stats_obj_.SetUpStatsTimer();
}

}  // namespace Mixer
}  // namespace Http
}  // namespace Envoy
