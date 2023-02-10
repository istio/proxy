// Copyright Istio Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "envoy/common/hashable.h"
#include "envoy/network/filter.h"
#include "envoy/stream_info/filter_state.h"

namespace Istio {
namespace SetInternalDstAddress {

const absl::string_view FilterStateKey = "istio.set_internal_dst_address";

struct Authority : public Envoy::StreamInfo::FilterState::Object, public Envoy::Hashable {
  Authority(absl::string_view value, uint32_t port) : value_(value), port_(port) {}
  absl::optional<std::string> serializeAsString() const override { return value_; }
  absl::optional<uint64_t> hash() const override;

  const std::string value_;
  // Default value 0 implies no port is overriden from the authority.
  const uint32_t port_;
};

class Filter : public Envoy::Network::ListenerFilter,
               public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
public:
  // Network::ListenerFilter
  Envoy::Network::FilterStatus onAccept(Envoy::Network::ListenerFilterCallbacks& cb) override;

  Envoy::Network::FilterStatus onData(Envoy::Network::ListenerFilterBuffer&) override {
    return Envoy::Network::FilterStatus::Continue;
  }

  size_t maxReadBytes() const override { return 0; }
};

} // namespace SetInternalDstAddress
} // namespace Istio
