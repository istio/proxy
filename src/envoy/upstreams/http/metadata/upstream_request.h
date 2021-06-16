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

#include "absl/types/optional.h"
#include "envoy/http/codec.h"
#include "envoy/http/protocol.h"
#include "envoy/router/router.h"
#include "envoy/stream_info/stream_info.h"
#include "envoy/upstream/host_description.h"
#include "envoy/upstream/load_balancer.h"
#include "envoy/upstream/thread_local_cluster.h"
#include "source/extensions/upstreams/http/http/upstream_request.h"

namespace Envoy {
namespace Upstreams {
namespace Http {
namespace Metadata {

class MetadataConnPool
    : public Extensions::Upstreams::Http::Http::HttpConnPool {
 public:
  MetadataConnPool(Upstream::ThreadLocalCluster& thread_local_cluster,
                   bool is_connect, const Router::RouteEntry& route_entry,
                   absl::optional<Envoy::Http::Protocol> downstream_protocol,
                   Upstream::LoadBalancerContext* ctx)
      : HttpConnPool(thread_local_cluster, is_connect, route_entry,
                     downstream_protocol, ctx) {}

  void onPoolReady(Envoy::Http::RequestEncoder& callbacks_encoder,
                   Upstream::HostDescriptionConstSharedPtr host,
                   const StreamInfo::StreamInfo& info,
                   absl::optional<Envoy::Http::Protocol> protocol) override;
};

class MetadataUpstream
    : public Extensions::Upstreams::Http::Http::HttpUpstream {
 public:
  MetadataUpstream(Router::UpstreamToDownstream& upstream_request,
                   Envoy::Http::RequestEncoder* encoder)
      : HttpUpstream(upstream_request, encoder) {}
};

}  // namespace Metadata
}  // namespace Http
}  // namespace Upstreams
}  // namespace Envoy
