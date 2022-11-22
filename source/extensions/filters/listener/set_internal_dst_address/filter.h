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

#include "envoy/network/filter.h"

namespace Istio {
namespace SetInternalDstAddress {

class Filter : public Envoy::Network::ListenerFilter,
               public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
 public:
  // Network::ListenerFilter
  Envoy::Network::FilterStatus onAccept(
      Envoy::Network::ListenerFilterCallbacks& cb) override;

  Envoy::Network::FilterStatus onData(
      Envoy::Network::ListenerFilterBuffer&) override {
    return Envoy::Network::FilterStatus::Continue;
  }

  size_t maxReadBytes() const override { return 0; }
};

}  // namespace SetInternalDstAddress
}  // namespace Istio
