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

#include "src/envoy/upstreams/http/metadata/upstream_request.h"

#include <memory>
#include <utility>

#include "absl/types/optional.h"
#include "envoy/http/codec.h"
#include "envoy/http/protocol.h"
#include "envoy/stream_info/stream_info.h"
#include "envoy/upstream/host_description.h"

namespace Envoy {
namespace Upstreams {
namespace Http {
namespace Metadata {

void MetadataConnPool::onPoolReady(
    Envoy::Http::RequestEncoder& request_encoder,
    Upstream::HostDescriptionConstSharedPtr host,
    const StreamInfo::StreamInfo& info,
    absl::optional<Envoy::Http::Protocol> protocol) {
  conn_pool_stream_handle_ = nullptr;
  auto upstream = std::make_unique<MetadataUpstream>(
      callbacks_->upstreamToDownstream(), &request_encoder);
  callbacks_->onPoolReady(std::move(upstream), host,
                          request_encoder.getStream().connectionLocalAddress(),
                          info, protocol);
}

}  // namespace Metadata
}  // namespace Http
}  // namespace Upstreams
}  // namespace Envoy
