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

#include "src/envoy/upstreams/http/metadata/config.h"

#include <memory>

#include "absl/types/optional.h"
#include "envoy/http/protocol.h"
#include "envoy/registry/registry.h"
#include "envoy/router/router.h"
#include "envoy/upstream/load_balancer.h"
#include "envoy/upstream/thread_local_cluster.h"
#include "src/envoy/upstreams/http/metadata/upstream_request.h"

namespace Envoy {
namespace Upstreams {
namespace Http {
namespace Metadata {

Router::GenericConnPoolPtr
MetadataGenericConnPoolFactory::createGenericConnPool(
    Upstream::ThreadLocalCluster& thread_local_cluster, bool is_connect,
    const Router::RouteEntry& route_entry,
    absl::optional<Envoy::Http::Protocol> downstream_protocol,
    Upstream::LoadBalancerContext* ctx) const {
  ASSERT(!is_connect);
  auto ret = std::make_unique<MetadataConnPool>(
      thread_local_cluster, is_connect, route_entry, downstream_protocol, ctx);
  return ret->valid() ? std::move(ret) : nullptr;
}

REGISTER_FACTORY(MetadataGenericConnPoolFactory,
                 Router::GenericConnPoolFactory);

}  // namespace Metadata
}  // namespace Http
}  // namespace Upstreams
}  // namespace Envoy
