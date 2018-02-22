/* Copyright 2018 Istio Authors. All Rights Reserved.
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

#include "src/envoy/http/authn/http_filter.h"

namespace Envoy {
namespace Http {

AuthnFilter::AuthnFilter(Upstream::ClusterManager& cm, Auth::AuthnStore& store)
    : cm_(cm), store_(store) {}

AuthnFilter::~AuthnFilter() {}

void AuthnFilter::onDestroy() {
  ENVOY_LOG(debug, "Called AuthnFilter : {}", __func__);
}

FilterHeadersStatus AuthnFilter::decodeHeaders(HeaderMap&, bool) {
  ENVOY_LOG(debug, "Called AuthnFilter : {}", __func__);
  state_ = HandleHeaders;

  const ::istio::authentication::v1alpha1::Policy& config = store_.config();
  int peer_size = config.peers_size();
  ENVOY_LOG(debug, "AuthnFilter: {} config.peers_size()={}", __func__,
            peer_size);
  for (int i = 0; i < peer_size; i++) {
    const ::istio::authentication::v1alpha1::Mechanism& m = config.peers()[i];
    if (m.has_mtls()) {
      ENVOY_LOG(debug, "AuthnFilter: {} this connection requires mTLS",
                __func__);
    } else {
      ENVOY_LOG(debug, "AuthnFilter: {} this connection does not require mTLS",
                __func__);
    }
  }

  ENVOY_LOG(debug,
            "Called AuthnFilter : {}, return FilterHeadersStatus::Continue;",
            __func__);
  return FilterHeadersStatus::Continue;
}

FilterDataStatus AuthnFilter::decodeData(Buffer::Instance&, bool) {
  ENVOY_LOG(debug, "Called AuthnFilter : {}", __func__);
  state_ = HandleData;
  ENVOY_LOG(debug, "Called AuthnFilter : {} FilterDataStatus::Continue;",
            __FUNCTION__);
  return FilterDataStatus::Continue;
}

FilterTrailersStatus AuthnFilter::decodeTrailers(HeaderMap&) {
  ENVOY_LOG(debug, "Called AuthnFilter : {}", __func__);
  state_ = HandleTrailers;
  return FilterTrailersStatus::Continue;
}

void AuthnFilter::setDecoderFilterCallbacks(
    StreamDecoderFilterCallbacks& callbacks) {
  ENVOY_LOG(debug, "Called AuthnFilter : {}", __func__);
  decoder_callbacks_ = &callbacks;
}

}  // namespace Http
}  // namespace Envoy
