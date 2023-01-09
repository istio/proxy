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

#include "source/extensions/filters/http/connect_baggage/filter.h"

#include "envoy/registry/registry.h"
#include "envoy/server/factory_context.h"
#include "extensions/common/context.h"
#include "extensions/common/metadata_object.h"
#include "source/common/http/header_utility.h"
#include "source/extensions/filters/common/expr/cel_state.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace ConnectBaggage {

constexpr absl::string_view Baggage = "baggage";
constexpr absl::string_view DownstreamPeerKey = "wasm.downstream_peer";

Http::FilterHeadersStatus Filter::decodeHeaders(Http::RequestHeaderMap& headers,
                                                bool) {
  if (propagate_) {
    const auto* object = decoder_callbacks_->streamInfo()
                             .filterState()
                             ->getDataReadOnly<Filters::Common::Expr::CelState>(
                                 DownstreamPeerKey);
    if (object) {
      const auto peer = Istio::Common::convertFlatNodeToWorkloadMetadata(
          *flatbuffers::GetRoot<Wasm::Common::FlatNode>(
              object->value().data()));
      headers.addCopy(Http::LowerCaseString(Baggage), peer.baggage());
    }
    return Http::FilterHeadersStatus::Continue;
  }
  const auto header_string = Http::HeaderUtility::getAllOfHeaderAsString(
      headers, Http::LowerCaseString(Baggage));
  const auto result = header_string.result();
  if (result) {
    const auto metadata_object =
        Istio::Common::WorkloadMetadataObject::fromBaggage(*result);
    const auto fb =
        Istio::Common::convertWorkloadMetadataToFlatNode(metadata_object);
    {
      Filters::Common::Expr::CelStatePrototype prototype(
          true, Filters::Common::Expr::CelStateType::FlatBuffers,
          toAbslStringView(Wasm::Common::nodeInfoSchema()),
          StreamInfo::FilterState::LifeSpan::FilterChain);
      auto state = std::make_unique<Filters::Common::Expr::CelState>(prototype);
      state->setValue(absl::string_view(
          reinterpret_cast<const char*>(fb.data()), fb.size()));
      decoder_callbacks_->streamInfo().filterState()->setData(
          "wasm.downstream_peer", std::move(state),
          StreamInfo::FilterState::StateType::Mutable,
          StreamInfo::FilterState::LifeSpan::FilterChain,
          StreamInfo::FilterState::StreamSharing::SharedWithUpstreamConnection);
    }
    {
      Filters::Common::Expr::CelStatePrototype prototype(
          true, Filters::Common::Expr::CelStateType::String,
          absl::string_view(), StreamInfo::FilterState::LifeSpan::FilterChain);
      auto state = std::make_unique<Filters::Common::Expr::CelState>(prototype);
      state->setValue("unknown");
      decoder_callbacks_->streamInfo().filterState()->setData(
          "wasm.downstream_peer_id", std::move(state),
          StreamInfo::FilterState::StateType::Mutable,
          StreamInfo::FilterState::LifeSpan::FilterChain,
          StreamInfo::FilterState::StreamSharing::SharedWithUpstreamConnection);
    }
  }
  return Http::FilterHeadersStatus::Continue;
}

Http::FilterFactoryCb FilterConfigFactory::createFilterFactoryFromProtoTyped(
    const io::istio::http::connect_baggage::Config& config, const std::string&,
    Server::Configuration::FactoryContext&) {
  return [config](Http::FilterChainFactoryCallbacks& callbacks) {
    auto filter = std::make_shared<Filter>(config.propagate());
    callbacks.addStreamFilter(filter);
  };
}

REGISTER_FACTORY(FilterConfigFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory);

}  // namespace ConnectBaggage
}  // namespace HttpFilters
}  // namespace Extensions
}  // namespace Envoy
