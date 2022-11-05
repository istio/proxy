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
#include "envoy/stream_info/filter_state.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace IstioAuthn {

constexpr absl::string_view SpiffePrefix("spiffe://");
constexpr absl::string_view PeerPrincipalKey = "io.istio.peer_principal";

class PeerPrincipal : public StreamInfo::FilterState::Object, public Hashable {
 public:
  PeerPrincipal(const std::string& principal) : principal_(principal) {}
  absl::optional<std::string> serializeAsString() const override {
    return principal_;
  }
  // Envoy::Hashable
  absl::optional<uint64_t> hash() const override;

 private:
  const std::string principal_;
};

}  // namespace IstioAuthn
}  // namespace NetworkFilters
}  // namespace Extensions
}  // namespace Envoy
