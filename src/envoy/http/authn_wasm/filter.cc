/* Copyright 2020 Istio Authors. All Rights Reserved.
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

#include "absl/strings/str_cat.h"
#include "authentication/v1alpha1/policy.pb.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/json_util.h"
#include "src/envoy/http/authn_wasm/authenticator/peer.h"
#include "src/envoy/http/authn_wasm/connection_context.h"
#include "src/envoy/http/authn_wasm/filter.h"

namespace Envoy {
namespace Wasm {
namespace Http {
namespace AuthN {

FilterHeadersStatus AuthnContext::onRequestHeaders(uint32_t) {
  const auto context = ConnectionContext();
  std::string metadata_bytes;

  if (!getValue({"metadata"}, &metadata_bytes)) {
    logError("Failed to read metadata");
    return FilterHeadersStatus::StopIteration;
  }

  istio::authn::Metadata metadata;
  metadata.ParseFromString(metadata_bytes);
  const auto request_headers = getRequestHeaderPairs()->pairs();

  filter_context_.reset(
      new FilterContext(request_headers, metadata, filterConfig()));

  istio::authn::Payload payload;

  if (!PeerAuthenticator::create(filter_context_)->run(&payload) &&
      !filterConfig().policy().peer_is_optional()) {
    logError("Peer authentication failed.");
    return FilterHeadersStatus::StopIteration;
  }

  // if (!)

  return FilterHeadersStatus::Continue;
}

}  // namespace AuthN
}  // namespace Http
}  // namespace Wasm
}  // namespace Envoy