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

#include "src/envoy/mixer/mixer_control.h"
#include "src/envoy/mixer/grpc_transport.h"

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

}  // namespace

MixerControl::MixerControl(const MixerConfig& mixer_config,
                           Upstream::ClusterManager& cm,
                           Event::Dispatcher& dispatcher,
                           Runtime::RandomGenerator& random)
    : cm_(cm) {
  ::istio::mixer_control::Controller::Options options(
      mixer_config.filter_config);

  options.check_transport = CheckTransport::GetFunc(cm, nullptr);
  options.report_transport = ReportTransport::GetFunc(cm);

  options.timer_create_func = [&dispatcher](std::function<void()> timer_cb)
      -> std::unique_ptr<::istio::mixer_client::Timer> {
        return std::unique_ptr<::istio::mixer_client::Timer>(
            new EnvoyTimer(dispatcher.createTimer(timer_cb)));
      };

  options.uuid_generate_func = [&random]() -> std::string {
    return random.uuid();
  };

  controller_ = ::istio::mixer_control::Controller::Create(options);
}

}  // namespace Mixer
}  // namespace Http
}  // namespace Envoy
