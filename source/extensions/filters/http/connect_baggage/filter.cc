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
#include "source/common/common/hash.h"
#include "source/common/http/header_utility.h"
#include "source/extensions/filters/common/expr/cel_state.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace ConnectBaggage {

constexpr absl::string_view Baggage = "baggage";

class CelStateHashable : public Filters::Common::Expr::CelState, public Hashable {
public:
  explicit CelStateHashable(const Filters::Common::Expr::CelStatePrototype& proto)
      : CelState(proto) {}
  absl::optional<uint64_t> hash() const override { return HashUtil::xxHash64(value()); }
};

Http::FilterHeadersStatus Filter::decodeHeaders(Http::RequestHeaderMap& headers, bool) {
  const auto header_string =
      Http::HeaderUtility::getAllOfHeaderAsString(headers, Http::LowerCaseString(Baggage));
  const auto result = header_string.result();
  if (result) {
    const auto metadata_object = Istio::Common::WorkloadMetadataObject::fromBaggage(*result);
    const auto fb = Istio::Common::convertWorkloadMetadataToFlatNode(metadata_object);
    {
      Filters::Common::Expr::CelStatePrototype prototype(
          true, Filters::Common::Expr::CelStateType::FlatBuffers,
          toAbslStringView(Wasm::Common::nodeInfoSchema()),
          StreamInfo::FilterState::LifeSpan::FilterChain);
      auto state = std::make_unique<CelStateHashable>(prototype);
      state->setValue(absl::string_view(reinterpret_cast<const char*>(fb.data()), fb.size()));
      decoder_callbacks_->streamInfo().filterState()->setData(
          "wasm.downstream_peer", std::move(state), StreamInfo::FilterState::StateType::Mutable,
          StreamInfo::FilterState::LifeSpan::FilterChain,
          StreamInfo::FilterState::StreamSharing::SharedWithUpstreamConnectionOnce);
    }
    {
      // This is needed because TCP stats filter awaits for TCP prefix.
      Filters::Common::Expr::CelStatePrototype prototype(
          true, Filters::Common::Expr::CelStateType::String, absl::string_view(),
          StreamInfo::FilterState::LifeSpan::FilterChain);
      auto state = std::make_unique<Filters::Common::Expr::CelState>(prototype);
      state->setValue("unknown");
      decoder_callbacks_->streamInfo().filterState()->setData(
          "wasm.downstream_peer_id", std::move(state), StreamInfo::FilterState::StateType::Mutable,
          StreamInfo::FilterState::LifeSpan::FilterChain,
          StreamInfo::FilterState::StreamSharing::SharedWithUpstreamConnectionOnce);
    }
  }
  return Http::FilterHeadersStatus::Continue;
}

Http::FilterFactoryCb FilterConfigFactory::createFilterFactoryFromProtoTyped(
    const io::istio::http::connect_baggage::Config&, const std::string&,
    Server::Configuration::FactoryContext&) {
  return [](Http::FilterChainFactoryCallbacks& callbacks) {
    auto filter = std::make_shared<Filter>();
    callbacks.addStreamFilter(filter);
  };
}

REGISTER_FACTORY(FilterConfigFactory, Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace ConnectBaggage
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
