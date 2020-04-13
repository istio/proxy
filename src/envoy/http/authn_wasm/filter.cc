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

#include "src/envoy/http/authn_wasm/filter.h"
#include "src/envoy/http/authn_wasm/connection_context.h"
#include "src/envoy/http/authn_wasm/authenticator/peer.h" 

#include "google/protobuf/text_format.h"
#include "google/protobuf/util/json_util.h"

#include "authentication/v1alpha1/policy.pb.h"

namespace Envoy {
namespace Extensions {
namespace Wasm {
namespace AuthN {

Istio::AuthN::HeaderMap void unmarshalPairs(const Pairs& pairs) {
  Istio::AuthN::HeaderMap header_map;
  for (auto&& [key, value]: pairs) {
    header_map[key] = value;
  }
  return header_map;
}

bool AuthnRootContext::onConfigure(size_t) {
  WasmDataPtr configuration = getConfiguration();
  google::protobuf::util::JsonParseOptions json_options;
  google::protobuf::util::Status status = JsonStringToMessage(configuration->toString(),
                                      &filter_config_, json_options);

  if (status != google::protobuf::util::Status::OK) {
    logError("Cannot parse authentication filter config: " + configuration->toString());
    return false;
  }

  logInfo("Istio AuthN filter is started with this configuration: ", configuration->toString());
  return true;
}

FilterHeadersStatus AuthnContext::onRequestHeaders(uint32_t) {
  const auto context = ConnectionContext();
  const auto metadata = getValue({"metadata"});
  filter_context_.reset(
    new Istio::AuthN::FilterContext(
      unmarshalPairs(getRequestHeader()->pairs()), context, metadata, filter_config_));

  istio::authn::Payload payload;

  if (PeerAuthenticator::create(filter_context_) && filter_config_.policy().peer_is_optional()) {
    logError("Peer authentication failed.");
    return FilterHeadersStatus::StopIteration;
  }

  // TODO(shikugawa): origin authenticator
  // TODO(shikugawa): save authenticate result state as dynamic metadata

  return FilterHeadersStatus::Continue;
}

}
}
}
}