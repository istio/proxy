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

constexpr absl::string_view SpiffePrefix("spiffe://");
constexpr absl::string_view PeerPrincipalKey = "io.istio.peer_principal";
constexpr absl::string_view LocalPrincipalKey = "io.istio.local_principal";

class Principal : public StreamInfo::FilterState::Object, public Hashable {
public:
  Principal(const std::string& principal) : principal_(principal) { ASSERT(principal.size() > 0); }
  absl::optional<std::string> serializeAsString() const override { return principal_; }
  absl::string_view principal() const { return principal_; }
  // Envoy::Hashable
  absl::optional<uint64_t> hash() const override;

private:
  const std::string principal_;
};

struct PrincipalInfo {
  absl::string_view peer;
  absl::string_view local;
};

// Obtains the peer and the local principals using the filter state.
PrincipalInfo getPrincipals(const StreamInfo::FilterState& filter_state);

// WARNING: The filter state is populated in on Connected event due to
// https://github.com/envoyproxy/envoy/issues/9023. Request-based protocols
// such as HTTP are not affected, since the upstream is determined after
// onData(). RBAC and ext_authz both follow the same pattern in checking in
// onData(), but any filter using onNewConnection() will not have access to the
// principals. For example, tcp_proxy cannot use the principals as a transport
// socket option at the moment.
class IstioAuthnFilter : public Network::ReadFilter, public Network::ConnectionCallbacks {
public:
  IstioAuthnFilter(bool shared)
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
