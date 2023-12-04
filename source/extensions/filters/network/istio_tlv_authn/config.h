/* Copyright Istio Authors. All Rights Reserved.
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

#include "envoy/common/hashable.h"
#include "envoy/network/filter.h"
#include "envoy/server/filter_config.h"
#include "envoy/stream_info/filter_state.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace IstioAuthn {

class IstioTLVAuthnFilter : 
  public Network::ReadFilter, 
  public Network::ConnectionCallbacks,
  public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
public:
  IstioTLVAuthnFilter(bool shared)
      : shared_(shared ? StreamInfo::StreamSharingMayImpactPooling::SharedWithUpstreamConnectionOnce
                       : StreamInfo::StreamSharingMayImpactPooling::None) {}

  // Network::ConnectionCallbacks
  void onEvent(Network::ConnectionEvent event) override;
  void onAboveWriteBufferHighWatermark() override {}
  void onBelowWriteBufferLowWatermark() override {}

  // Network::ReadFilter
  Network::FilterStatus onData(Buffer::Instance&, bool) override {
    return Network::FilterStatus::Continue;
  }
  Network::FilterStatus onNewConnection() override { return Network::FilterStatus::Continue; }
  void initializeReadFilterCallbacks(Network::ReadFilterCallbacks& callbacks) override;

private:
  void populate() const;
  const StreamInfo::StreamSharingMayImpactPooling shared_;
  Network::ReadFilterCallbacks* read_callbacks_{nullptr};
};

} // namespace IstioAuthn
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
