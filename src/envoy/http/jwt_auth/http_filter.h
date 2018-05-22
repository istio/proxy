/* Copyright 2017 Istio Authors. All Rights Reserved.
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
#include "envoy/config/filter/http/jwt_authn/v2alpha/config.pb.h"

#include "src/envoy/utils/jwt_authenticator.h"

#include "common/common/logger.h"
#include "envoy/http/filter.h"

namespace Envoy {
namespace Http {

// The Envoy filter to process JWT auth.
class JwtVerificationFilter : public StreamDecoderFilter,
                              public Utils::Jwt::JwtAuthenticator::Callbacks,
                              public Logger::Loggable<Logger::Id::filter> {
 public:
  JwtVerificationFilter(std::shared_ptr<Utils::Jwt::JwtAuthenticator> jwt_auth,
                        const ::envoy::config::filter::http::jwt_authn::v2alpha::JwtAuthentication& config);
  ~JwtVerificationFilter();

  // Http::StreamFilterBase
  void onDestroy() override;

  // Http::StreamDecoderFilter
  FilterHeadersStatus decodeHeaders(HeaderMap& headers, bool) override;
  FilterDataStatus decodeData(Buffer::Instance&, bool) override;
  FilterTrailersStatus decodeTrailers(HeaderMap&) override;
  void setDecoderFilterCallbacks(
      StreamDecoderFilterCallbacks& callbacks) override;

 private:
  // JwtAuth::Authenticator::Callbacks interface.
  // To be called when its Verify() call is completed successfully.
  void onSuccess(const Utils::Jwt::Jwt *jwt, const Http::LowerCaseString *header) override;
  // To be called when token authentication fails
  void onError(Utils::Jwt::Status status) override;

  // The callback funcion.
  StreamDecoderFilterCallbacks* decoder_callbacks_;
  // The auth object.
  std::shared_ptr<Utils::Jwt::JwtAuthenticator> jwt_auth_;
  // The filter configuration.
  const ::envoy::config::filter::http::jwt_authn::v2alpha::JwtAuthentication config_;
  // The state of the request
  enum State { Init, Calling, Responded, Complete };
  State state_ = Init;
  // Mark if request has been stopped.
  bool stopped_ = false;
  // Stream has been reset.
  bool stream_reset_;

  // The HTTP request headers
  Http::HeaderMap* headers_{};

  bool OkToBypass() const;
};

}  // namespace Http
}  // namespace Envoy
