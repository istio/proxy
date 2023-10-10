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

#include "source/extensions/filters/http/common/factory_base.h"
#include "source/extensions/filters/http/common/pass_through_filter.h"
#include "source/extensions/filters/http/peer_metadata/config.pb.h"
#include "source/extensions/filters/http/peer_metadata/config.pb.validate.h"
#include "source/extensions/common/workload_discovery/api.h"
#include "source/common/singleton/const_singleton.h"
#include "extensions/common/context.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace PeerMetadata {

constexpr absl::string_view WasmDownstreamPeer = "wasm.downstream_peer";
constexpr absl::string_view WasmDownstreamPeerID = "wasm.downstream_peer_id";
constexpr absl::string_view WasmUpstreamPeer = "wasm.upstream_peer";
constexpr absl::string_view WasmUpstreamPeerID = "wasm.upstream_peer_id";

struct HeaderValues {
  const Http::LowerCaseString Baggage{"baggage"};
  const Http::LowerCaseString ExchangeMetadataHeader{"x-envoy-peer-metadata"};
  const Http::LowerCaseString ExchangeMetadataHeaderId{"x-envoy-peer-metadata-id"};
};

using Headers = ConstSingleton<HeaderValues>;

// Peer info in the flatbuffers format.
using PeerInfo = std::string;

struct Context {
  bool request_peer_id_received_{false};
  bool request_peer_received_{false};
};

// Base class for the discovery methods. First derivation wins but all methods perform removal.
class DiscoveryMethod {
public:
  virtual ~DiscoveryMethod() = default;
  virtual absl::optional<PeerInfo> derivePeerInfo(const StreamInfo::StreamInfo&, Http::HeaderMap&,
                                                  Context&) const PURE;
  virtual void remove(Http::HeaderMap&) const {}
};

using DiscoveryMethodPtr = std::unique_ptr<DiscoveryMethod>;

class MXMethod : public DiscoveryMethod {
public:
  MXMethod(bool downstream, Server::Configuration::ServerFactoryContext& factory_context);
  absl::optional<PeerInfo> derivePeerInfo(const StreamInfo::StreamInfo&, Http::HeaderMap&,
                                          Context&) const override;
  void remove(Http::HeaderMap&) const override;

private:
  absl::optional<PeerInfo> lookup(absl::string_view id, absl::string_view value) const;
  const bool downstream_;
  struct MXCache : public ThreadLocal::ThreadLocalObject {
    absl::flat_hash_map<std::string, std::string> cache_;
  };
  mutable ThreadLocal::TypedSlot<MXCache> tls_;
  const int64_t max_peer_cache_size_{500};
};

// Base class for the propagation methods.
class PropagationMethod {
public:
  virtual ~PropagationMethod() = default;
  virtual void inject(const StreamInfo::StreamInfo&, Http::HeaderMap&, Context&) const PURE;
};

using PropagationMethodPtr = std::unique_ptr<PropagationMethod>;

class MXPropagationMethod : public PropagationMethod {
public:
  MXPropagationMethod(bool downstream, Server::Configuration::ServerFactoryContext& factory_context,
                      const io::istio::http::peer_metadata::Config_IstioHeaders&);
  void inject(const StreamInfo::StreamInfo&, Http::HeaderMap&, Context&) const override;

private:
  const bool downstream_;
  std::string computeValue(Server::Configuration::ServerFactoryContext&) const;
  const std::string id_;
  const std::string value_;
  const bool skip_external_clusters_;
  bool skipMXHeaders(const StreamInfo::StreamInfo&) const;
};

class FilterConfig : public Logger::Loggable<Logger::Id::filter> {
public:
  FilterConfig(const io::istio::http::peer_metadata::Config&,
               Server::Configuration::FactoryContext&);
  void discoverDownstream(StreamInfo::StreamInfo&, Http::RequestHeaderMap&, Context&) const;
  void discoverUpstream(StreamInfo::StreamInfo&, Http::ResponseHeaderMap&, Context&) const;
  void injectDownstream(const StreamInfo::StreamInfo&, Http::ResponseHeaderMap&, Context&) const;
  void injectUpstream(const StreamInfo::StreamInfo&, Http::RequestHeaderMap&, Context&) const;

private:
  std::vector<DiscoveryMethodPtr> buildDiscoveryMethods(
      const Protobuf::RepeatedPtrField<io::istio::http::peer_metadata::Config::DiscoveryMethod>&,
      bool downstream, Server::Configuration::FactoryContext&) const;
  std::vector<PropagationMethodPtr> buildPropagationMethods(
      const Protobuf::RepeatedPtrField<io::istio::http::peer_metadata::Config::PropagationMethod>&,
      bool downstream, Server::Configuration::FactoryContext&) const;
  StreamInfo::StreamSharingMayImpactPooling sharedWithUpstream() const {
    return shared_with_upstream_
               ? StreamInfo::StreamSharingMayImpactPooling::SharedWithUpstreamConnectionOnce
               : StreamInfo::StreamSharingMayImpactPooling::None;
  }
  void discover(StreamInfo::StreamInfo&, bool downstream, Http::HeaderMap&, Context&) const;
  void setFilterState(StreamInfo::StreamInfo&, bool downstream, const std::string& value) const;
  const bool shared_with_upstream_;
  const std::vector<DiscoveryMethodPtr> downstream_discovery_;
  const std::vector<DiscoveryMethodPtr> upstream_discovery_;
  const std::vector<PropagationMethodPtr> downstream_propagation_;
  const std::vector<PropagationMethodPtr> upstream_propagation_;
};

using FilterConfigSharedPtr = std::shared_ptr<FilterConfig>;

class Filter : public Http::PassThroughFilter {
public:
  Filter(const FilterConfigSharedPtr& config) : config_(config) {}
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap&, bool) override;
  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap&, bool) override;

private:
  FilterConfigSharedPtr config_;
  Context ctx_;
};

class FilterConfigFactory : public Common::FactoryBase<io::istio::http::peer_metadata::Config> {
public:
  FilterConfigFactory() : FactoryBase("envoy.filters.http.peer_metadata") {}

private:
  Http::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const io::istio::http::peer_metadata::Config&,
                                    const std::string&,
                                    Server::Configuration::FactoryContext&) override;
};

} // namespace PeerMetadata
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
