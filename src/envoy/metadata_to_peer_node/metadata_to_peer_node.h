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
#include "envoy/stats/scope.h"
#include "extensions/common/context.h"
#include "source/common/common/logger.h"
#include "source/common/common/stl_helpers.h"
#include "source/extensions/filters/common/expr/cel_state.h"
#include "src/envoy/metadata_to_peer_node/config/metadata_to_peer_node.pb.h"

using namespace istio::telemetry::metadatatopeernode;
using ::Envoy::Extensions::Filters::Common::Expr::CelStatePrototype;
using ::Envoy::Extensions::Filters::Common::Expr::CelStateType;

namespace Envoy {
namespace MetadataToPeerNode {

/**
 * Global configuration for Metadata To Peer Node listener filter.
 */
class Config : public Logger::Loggable<Logger::Id::filter> {
 public:
  Config(const v1::Config&){};

  static const CelStatePrototype& nodeInfoPrototype() {
    static const CelStatePrototype* const prototype = new CelStatePrototype(
        true, CelStateType::FlatBuffers,
        Envoy::toAbslStringView(Wasm::Common::nodeInfoSchema()),
        StreamInfo::FilterState::LifeSpan::Request);
    return *prototype;
  }

  static const CelStatePrototype& nodeIdPrototype() {
    static const CelStatePrototype* const prototype =
        new CelStatePrototype(true, CelStateType::String, absl::string_view(),
                              StreamInfo::FilterState::LifeSpan::Request);
    return *prototype;
  }
};

using ConfigSharedPtr = std::shared_ptr<Config>;

/**
 * Metadata To Peer Node listener filter.
 */
class Filter : public Network::ListenerFilter,
               Logger::Loggable<Logger::Id::filter> {
 public:
  Filter(const ConfigSharedPtr& config) : config_(config) {}

  // Network::ListenerFilter
  Network::FilterStatus onAccept(Network::ListenerFilterCallbacks& cb) override;

  Network::FilterStatus onData(Network::ListenerFilterBuffer&) override;

  size_t maxReadBytes() const override;

 private:
  ConfigSharedPtr config_;
};

}  // namespace MetadataToPeerNode
}  // namespace Envoy
