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

#include "src/envoy/http/authn_wasm/filter_context.h"

#include "absl/strings/str_cat.h"
#include "src/envoy/utils/filter_names.h"

#ifdef NULL_PLUGIN

namespace proxy_wasm {
namespace null_plugin {
namespace Http {
namespace AuthN {

#endif

FilterContext::FilterContext(
    ConnectionContext&& connection_context, const RawHeaderMap& raw_header_map,
    const istio::authn::Metadata& dynamic_metadata,
    const istio::envoy::config::filter::http::authn::v2alpha1::FilterConfig&
        filter_config)
    : connection_context_(std::move(connection_context)),
      filter_config_(filter_config),
      dynamic_metadata_(dynamic_metadata) {
  createHeaderMap(std::move(raw_header_map));
}

void FilterContext::setPeerResult(const istio::authn::Payload* payload) {
  if (payload != nullptr) {
    switch (payload->payload_case()) {
      case istio::authn::Payload::kX509:
        logDebug(absl::StrCat("Set peer from X509: ", payload->x509().user()));
        result_.set_peer_user(payload->x509().user());
        break;
      case istio::authn::Payload::kJwt:
        logDebug(absl::StrCat("Set peer from JWT: ", payload->jwt().user()));
        result_.set_peer_user(payload->jwt().user());
        break;
      default:
        logDebug("Payload has not peer authentication data");
        break;
    }
  }
}

void FilterContext::setOriginResult(const istio::authn::Payload* payload) {
  // Authentication pass, look at the return payload and store to the context
  // output. Set filter to continueDecoding when done.
  // At the moment, only JWT can be used for origin authentication, so
  // it's ok just to check jwt payload.
  if (payload != nullptr && payload->has_jwt()) {
    *result_.mutable_origin() = payload->jwt();
  }
}

void FilterContext::setPrincipal(
    const istio::authentication::v1alpha1::PrincipalBinding& binding) {
  switch (binding) {
    case istio::authentication::v1alpha1::PrincipalBinding::USE_PEER:
      logDebug(absl::StrCat("Set principal from peer: ", result_.peer_user()));
      result_.set_principal(result_.peer_user());
      return;
    case istio::authentication::v1alpha1::PrincipalBinding::USE_ORIGIN:
      logDebug(
          absl::StrCat("Set principal from origin: ", result_.origin().user()));
      result_.set_principal(result_.origin().user());
      return;
    default:
      // Should never come here.
      // TODO(shikugawa): add wasm logger and enable to write logging like under
      // format. e.g. logDebug("Invalid binding value", binding)
      logDebug("Invalid binding value");
      return;
  }
}

void FilterContext::createHeaderMap(const RawHeaderMap& raw_header_map) {
  for (const auto& header : raw_header_map) {
    header_map_.emplace(header.first.data(), header.second.data());
  }
}

#ifdef NULL_PLUGIN

}  // namespace AuthN
}  // namespace Http
}  // namespace null_plugin
}  // namespace proxy_wasm

#endif