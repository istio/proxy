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

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/http/header_map_impl.h"
#include "common/http/status.h"
#include "common/protobuf/protobuf.h"
#include "envoy/config/core/v3/base.pb.h"
#include "envoy/http/codec.h"
#include "envoy/http/header_map.h"
#include "envoy/http/protocol.h"
#include "envoy/router/router.h"
#include "envoy/stream_info/stream_info.h"
#include "envoy/upstream/host_description.h"
#include "envoy/upstream/load_balancer.h"
#include "envoy/upstream/thread_local_cluster.h"
#include "extensions/upstreams/http/http/upstream_request.h"

namespace Envoy {
namespace Upstreams {
namespace Http {
namespace Metadata {

namespace {

const std::string kIstioMetadataKey = "istio";
const std::string kOriginalPortHeader = "x-istio-original-port";
const std::string kOriginalPortKey = "default_original_port";

void addHeader(Envoy::Http::RequestHeaderMap& headers,
               absl::string_view header_name,
               const envoy::config::core::v3::Metadata& metadata,
               absl::string_view key) {
  Envoy::Http::LowerCaseString header((std::string(header_name)));
  headers.remove(header);
  if (auto filter_metadata = metadata.filter_metadata().find(kIstioMetadataKey);
      filter_metadata != metadata.filter_metadata().end()) {
    const ProtobufWkt::Struct& data = filter_metadata->second;
    const auto& fields = data.fields();
    if (auto iter = fields.find(key); iter != fields.end()) {
      if (iter->second.kind_case() == ProtobufWkt::Value::kStringValue) {
        headers.setCopy(header, iter->second.string_value());
      }
    }
  }
}
}  // namespace

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
                   Envoy::Http::RequestEncoder* encoder,
                   Upstream::HostDescriptionConstSharedPtr host)
      : HttpUpstream(upstream_request, encoder), host_(host) {}

  Envoy::Http::Status encodeHeaders(
      const Envoy::Http::RequestHeaderMap& headers, bool end_stream) override {
    auto dup = Envoy::Http::RequestHeaderMapImpl::create();
    Envoy::Http::HeaderMapImpl::copyFrom(*dup, headers);
    addHeader(*dup, kOriginalPortHeader, host_->cluster().metadata(),
              kOriginalPortKey);
    return HttpUpstream::encodeHeaders(*dup, end_stream);
  }

 private:
  Upstream::HostDescriptionConstSharedPtr host_;
};

}  // namespace Metadata
}  // namespace Http
}  // namespace Upstreams
}  // namespace Envoy
