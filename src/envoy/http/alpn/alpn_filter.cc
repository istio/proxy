/* Copyright 2019 Istio Authors. All Rights Reserved.
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

#include "src/envoy/http/alpn/alpn_filter.h"

#include "common/network/application_protocol.h"

namespace Envoy {
namespace Http {
namespace Alpn {

AlpnFilterConfig::AlpnFilterConfig(
    const istio::envoy::config::filter::http::alpn::v2alpha1::FilterConfig
        &proto_config)
    : alpn_override_(proto_config.alpn_override().begin(),
                     proto_config.alpn_override().end()) {}

Http::FilterHeadersStatus AlpnFilter::decodeHeaders(Http::HeaderMap &, bool) {
  const auto &alpn_override = config_->getAlpnOverride();
  if (!alpn_override.empty()) {
    ENVOY_LOG(debug, "override with {} ALPNs", alpn_override.size());
    decoder_callbacks_->streamInfo().filterState().setData(
        Network::ApplicationProtocols::key(),
        std::make_unique<Network::ApplicationProtocols>(alpn_override),
        Envoy::StreamInfo::FilterState::StateType::ReadOnly);
  } else {
    ENVOY_LOG(debug, "ALPN override is empty");
  }
  return Http::FilterHeadersStatus::Continue;
}

}  // namespace Alpn
}  // namespace Http
}  // namespace Envoy
