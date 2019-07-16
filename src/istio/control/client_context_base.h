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

#ifndef ISTIO_CONTROL_CLIENT_CONTEXT_BASE_H
#define ISTIO_CONTROL_CLIENT_CONTEXT_BASE_H

#include "include/istio/mixerclient/client.h"
#include "include/istio/mixerclient/timer.h"
#include "include/istio/utils/attribute_names.h"
#include "include/istio/utils/local_attributes.h"
#include "mixer/v1/config/client/client_config.pb.h"
#include "src/istio/mixerclient/check_context.h"
#include "src/istio/mixerclient/shared_attributes.h"

namespace istio {
namespace control {

// The global context object to hold the mixer client object
// to call Check/Report with cache.
class ClientContextBase {
 public:
  ClientContextBase(
      const ::istio::mixer::v1::config::client::TransportConfig& config,
      const ::istio::mixerclient::Environment& env, bool outbound,
      const ::istio::utils::LocalNode& local_node);

  // A constructor for unit-test to pass in a mock mixer_client
  ClientContextBase(
      std::unique_ptr<::istio::mixerclient::MixerClient> mixer_client,
      bool outbound, ::istio::utils::LocalAttributes& local_attributes)
      : mixer_client_(std::move(mixer_client)),
        outbound_(outbound),
        local_attributes_(local_attributes),
        network_fail_open_(false),
        retries_(0) {}
  // virtual destrutor
  virtual ~ClientContextBase() {}

  // Use mixer client object to make a Check call.
  void SendCheck(const ::istio::mixerclient::TransportCheckFunc& transport,
                 const ::istio::mixerclient::CheckDoneFunc& on_done,
                 ::istio::mixerclient::CheckContextSharedPtr& check_context);

  // Use mixer client object to make a Report call.
  void SendReport(
      const istio::mixerclient::SharedAttributesSharedPtr& attributes);

  // Get statistics.
  void GetStatistics(::istio::mixerclient::Statistics* stat) const;

  void AddLocalNodeAttributes(::istio::mixer::v1::Attributes* request) const;

  void AddLocalNodeForwardAttribues(
      ::istio::mixer::v1::Attributes* request) const;

  bool NetworkFailOpen() const { return network_fail_open_; }

  uint32_t Retries() const { return retries_; }

 private:
  // The mixer client object with check cache and report batch features.
  std::unique_ptr<::istio::mixerclient::MixerClient> mixer_client_;

  // If this is an outbound client context.
  bool outbound_;

  // local attributes - owned by the client context.
  ::istio::utils::LocalAttributes local_attributes_;

  bool network_fail_open_;
  uint32_t retries_;
};

}  // namespace control
}  // namespace istio

#endif  // ISTIO_CONTROL_CLIENT_CONTEXT_BASE_H
