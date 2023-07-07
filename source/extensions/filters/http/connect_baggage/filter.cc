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
#include "extensions/common/proto_util.h"
#include "source/common/common/hash.h"
#include "source/common/common/base64.h"
#include "source/common/http/header_utility.h"
#include "source/common/http/utility.h"
#include "source/extensions/filters/common/expr/cel_state.h"
#include "source/common/singleton/const_singleton.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace ConnectBaggage {

struct HeaderValues {
  const Http::LowerCaseString Baggage{"baggage"};
  const Http::LowerCaseString ExchangeMetadataHeader{"x-envoy-peer-metadata"};
  const Http::LowerCaseString ExchangeMetadataHeaderId{"x-envoy-peer-metadata-id"};
};

using Headers = ConstSingleton<HeaderValues>;

// Extended peer info that supports "hashing" to enable sharing with the
// upstream connection via an internal listener.
class CelStateHashable : public Filters::Common::Expr::CelState, public Hashable {
public:
  explicit CelStateHashable(const Filters::Common::Expr::CelStatePrototype& proto)
      : CelState(proto) {}
  absl::optional<uint64_t> hash() const override { return HashUtil::xxHash64(value()); }
};

struct CelPrototypeValues {
  const Filters::Common::Expr::CelStatePrototype NodeInfo{
      true, Filters::Common::Expr::CelStateType::FlatBuffers,
      toAbslStringView(Wasm::Common::nodeInfoSchema()),
      // Life span is only needed for Wasm set_property, not here.
      StreamInfo::FilterState::LifeSpan::FilterChain};
  const Filters::Common::Expr::CelStatePrototype NodeId{
      true, Filters::Common::Expr::CelStateType::String, absl::string_view(),
      // Life span is only needed for Wasm set_property, not here.
      StreamInfo::FilterState::LifeSpan::FilterChain};
};

using CelPrototypes = ConstSingleton<CelPrototypeValues>;

absl::optional<PeerInfo> BaggageMethod::derivePeerInfo(const StreamInfo::StreamInfo&,
                                                       Http::HeaderMap& headers) const {
  const auto header_string =
      Http::HeaderUtility::getAllOfHeaderAsString(headers, Headers::get().Baggage);
  const auto result = header_string.result();
  if (result) {
    const auto metadata_object = Istio::Common::WorkloadMetadataObject::fromBaggage(*result);
    return Istio::Common::convertWorkloadMetadataToFlatNode(metadata_object);
  }
  return {};
}

absl::optional<PeerInfo> XDSMethod::derivePeerInfo(const StreamInfo::StreamInfo& info,
                                                   Http::HeaderMap&) const {
  if (!metadata_provider_) {
    return {};
  }
  Network::Address::InstanceConstSharedPtr peer_address =
      info.downstreamAddressProvider().remoteAddress();
  const auto metadata_object = metadata_provider_->GetMetadata(peer_address);
  if (metadata_object) {
    return Istio::Common::convertWorkloadMetadataToFlatNode(metadata_object.value());
  }
  return {};
}

absl::optional<PeerInfo> MXMethod::derivePeerInfo(const StreamInfo::StreamInfo&,
                                                  Http::HeaderMap& headers) const {
  const auto peer_id_header = headers.get(Headers::get().ExchangeMetadataHeaderId);
  absl::string_view peer_id =
      peer_id_header.empty() ? "" : peer_id_header[0]->value().getStringView();
  const auto peer_info_header = headers.get(Headers::get().ExchangeMetadataHeader);
  absl::string_view peer_info =
      peer_info_header.empty() ? "" : peer_info_header[0]->value().getStringView();
  if (!peer_info.empty()) {
    const auto out = lookup(peer_id, peer_info);
    headers.remove(Headers::get().ExchangeMetadataHeaderId);
    headers.remove(Headers::get().ExchangeMetadataHeader);
    return out;
  }
  return {};
}

absl::optional<PeerInfo> MXMethod::lookup(absl::string_view id, absl::string_view value) const {
  if (max_peer_cache_size_ > 0 && !id.empty()) {
    auto it = cache_.find(id);
    if (it != cache_.end()) {
      return it->second;
    }
  }
  const auto bytes = Base64::decodeWithoutPadding(value);
  google::protobuf::Struct metadata;
  if (!metadata.ParseFromString(bytes)) {
    return {};
  }
  const auto fb = ::Wasm::Common::extractNodeFlatBufferFromStruct(metadata);
  std::string out(reinterpret_cast<const char*>(fb.data()), fb.size());
  if (max_peer_cache_size_ > 0 && !id.empty()) {
    // do not let the cache grow beyond max cache size.
    if (static_cast<uint32_t>(cache_.size()) > max_peer_cache_size_) {
      cache_.erase(cache_.begin(), std::next(cache_.begin(), max_peer_cache_size_ / 4));
    }
    cache_.emplace(id, out);
  }
  return out;
}

MXPropagationMethod::MXPropagationMethod(
    Server::Configuration::ServerFactoryContext& factory_context)
    : id_(factory_context.localInfo().node().id()) {
  const auto fb = ::Wasm::Common::extractNodeFlatBufferFromStruct(
      factory_context.localInfo().node().metadata());
  google::protobuf::Struct metadata;
  ::Wasm::Common::extractStructFromNodeFlatBuffer(
      *flatbuffers::GetRoot<::Wasm::Common::FlatNode>(fb.data()), &metadata);
  std::string metadata_bytes;
  ::Wasm::Common::serializeToStringDeterministic(metadata, &metadata_bytes);
  value_ = Base64::encode(metadata_bytes.data(), metadata_bytes.size());
}

void MXPropagationMethod::inject(Http::HeaderMap& headers) const {
  headers.setReference(Headers::get().ExchangeMetadataHeaderId, id_);
  headers.setReference(Headers::get().ExchangeMetadataHeader, value_);
}

FilterConfig::FilterConfig(const io::istio::http::connect_baggage::Config& config,
                           Server::Configuration::FactoryContext& factory_context)
    : shared_with_upstream_(config.shared_with_upstream()),
      downstream_discovery_(buildDiscoveryMethods(config.downstream_discovery(), factory_context)),
      upstream_discovery_(buildDiscoveryMethods(config.upstream_discovery(), factory_context)),
      downstream_propagation_(
          buildPropagationMethods(config.downstream_propagation(), factory_context)),
      upstream_propagation_(
          buildPropagationMethods(config.upstream_propagation(), factory_context)) {}

std::vector<DiscoveryMethodPtr> FilterConfig::buildDiscoveryMethods(
    const Protobuf::RepeatedPtrField<io::istio::http::connect_baggage::Config::DiscoveryMethod>&
        config,
    Server::Configuration::FactoryContext& factory_context) const {
  std::vector<DiscoveryMethodPtr> methods;
  methods.reserve(config.size());
  for (const auto& method : config) {
    switch (method.method_specifier_case()) {
    case io::istio::http::connect_baggage::Config::DiscoveryMethod::MethodSpecifierCase::kBaggage:
      methods.push_back(std::make_unique<BaggageMethod>());
      break;
    case io::istio::http::connect_baggage::Config::DiscoveryMethod::MethodSpecifierCase::
        kWorkloadDiscovery:
      methods.push_back(std::make_unique<XDSMethod>(factory_context.getServerFactoryContext()));
      break;
    case io::istio::http::connect_baggage::Config::DiscoveryMethod::MethodSpecifierCase::
        kIstioHeaders:
      methods.push_back(std::make_unique<MXMethod>());
      break;
    default:
      break;
    }
  }
  return methods;
}

std::vector<PropagationMethodPtr> FilterConfig::buildPropagationMethods(
    const Protobuf::RepeatedPtrField<io::istio::http::connect_baggage::Config::PropagationMethod>&
        config,
    Server::Configuration::FactoryContext& factory_context) const {
  std::vector<PropagationMethodPtr> methods;
  methods.reserve(config.size());
  for (const auto& method : config) {
    switch (method.method_specifier_case()) {
    case io::istio::http::connect_baggage::Config::PropagationMethod::MethodSpecifierCase::
        kIstioHeaders:
      methods.push_back(
          std::make_unique<MXPropagationMethod>(factory_context.getServerFactoryContext()));
      break;
    default:
      break;
    }
  }
  return methods;
}

void FilterConfig::discoverDownstream(StreamInfo::StreamInfo& info,
                                      Http::RequestHeaderMap& headers) const {
  absl::optional<PeerInfo> result;
  for (const auto& method : downstream_discovery_) {
    result = method->derivePeerInfo(info, headers);
    if (result) {
      break;
    }
  }
  if (result) {
    setFilterState(info, true, *result);
  }
}

void FilterConfig::discoverUpstream(StreamInfo::StreamInfo& info,
                                    Http::ResponseHeaderMap& headers) const {
  absl::optional<PeerInfo> result;
  for (const auto& method : upstream_discovery_) {
    result = method->derivePeerInfo(info, headers);
    if (result) {
      break;
    }
  }
  if (result) {
    setFilterState(info, false, *result);
  }
}

void FilterConfig::injectDownstream(Http::ResponseHeaderMap& headers) const {
  for (const auto& method : downstream_propagation_) {
    method->inject(headers);
  }
}

void FilterConfig::injectUpstream(Http::RequestHeaderMap& headers) const {
  for (const auto& method : upstream_propagation_) {
    method->inject(headers);
  }
}

void FilterConfig::setFilterState(StreamInfo::StreamInfo& info, bool downstream,
                                  const std::string& value) const {
  auto node_info = std::make_unique<CelStateHashable>(CelPrototypes::get().NodeInfo);
  node_info->setValue(value);
  info.filterState()->setData(downstream ? "wasm.downstream_peer" : "wasm.upstream_peer",
                              std::move(node_info), StreamInfo::FilterState::StateType::Mutable,
                              StreamInfo::FilterState::LifeSpan::FilterChain, sharedWithUpstream());
  // This is needed because stats filter awaits for the prefix on the wire and checks for the key
  // presence before emitting any telemetry.
  auto node_id = std::make_unique<Filters::Common::Expr::CelState>(CelPrototypes::get().NodeId);
  node_id->setValue("unknown");
  info.filterState()->setData(downstream ? "wasm.downstream_peer_id" : "wasm.upstream_peer_id",
                              std::move(node_id), StreamInfo::FilterState::StateType::Mutable,
                              StreamInfo::FilterState::LifeSpan::FilterChain, sharedWithUpstream());
}

Http::FilterHeadersStatus Filter::decodeHeaders(Http::RequestHeaderMap& headers, bool) {
  config_->discoverDownstream(decoder_callbacks_->streamInfo(), headers);
  config_->injectUpstream(headers);
  return Http::FilterHeadersStatus::Continue;
}

Http::FilterHeadersStatus Filter::encodeHeaders(Http::ResponseHeaderMap& headers, bool) {
  config_->discoverUpstream(decoder_callbacks_->streamInfo(), headers);
  config_->injectDownstream(headers);
  return Http::FilterHeadersStatus::Continue;
}

Http::FilterFactoryCb FilterConfigFactory::createFilterFactoryFromProtoTyped(
    const io::istio::http::connect_baggage::Config& config, const std::string&,
    Server::Configuration::FactoryContext& factory_context) {
  auto filter_config = std::make_shared<FilterConfig>(config, factory_context);
  return [filter_config](Http::FilterChainFactoryCallbacks& callbacks) {
    auto filter = std::make_shared<Filter>(filter_config);
    callbacks.addStreamFilter(filter);
  };
}

REGISTER_FACTORY(FilterConfigFactory, Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace ConnectBaggage
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
