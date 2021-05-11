/* Copyright 2021 Istio Authors. All Rights Reserved.
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

#include <memory>

#include "absl/types/optional.h"
#include "common/protobuf/protobuf.h"
#include "envoy/http/protocol.h"
#include "envoy/registry/registry.h"
#include "envoy/router/router.h"
#include "envoy/upstream/load_balancer.h"
#include "envoy/upstream/thread_local_cluster.h"

namespace Envoy {
namespace Upstreams {
namespace Http {
namespace Metadata {

/**
 * Config registration for the MetadataConnPool.
 * This extension is meant to be used to make only HTTP2 requests upstream.
 * Thus it does not support CONNECT and `is_connect` must be `false`.
 * @see Router::GenericConnPoolFactory
 */
class MetadataGenericConnPoolFactory : public Router::GenericConnPoolFactory {
 public:
  std::string name() const override {
    return "istio.filters.connection_pools.http.metadata";
  }
  std::string category() const override { return "envoy.upstreams"; }

  Router::GenericConnPoolPtr createGenericConnPool(
      Upstream::ThreadLocalCluster& thread_local_cluster, bool is_connect,
      const Router::RouteEntry& route_entry,
      absl::optional<Envoy::Http::Protocol> downstream_protocol,
      Upstream::LoadBalancerContext* ctx) const override;

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<ProtobufWkt::Struct>();
  }
};

DECLARE_FACTORY(MetadataGenericConnPoolFactory);

}  // namespace Metadata
}  // namespace Http
}  // namespace Upstreams
}  // namespace Envoy
