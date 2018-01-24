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

#include "http_filter.h"
#include "config.h"
#include "jwt.h"

#include "common/http/message_impl.h"
#include "common/http/utility.h"
#include "envoy/http/async_client.h"
#include "server/config/network/http_connection_manager.h"

#include <chrono>
#include <string>

namespace Envoy {
namespace Http {

JwtVerificationFilter::JwtVerificationFilter(
    std::shared_ptr<Auth::ControllerFactory> controller_factory)
    : controller_(controller_factory->controller()) {}

JwtVerificationFilter::~JwtVerificationFilter() {}

void JwtVerificationFilter::onDestroy() {
  ENVOY_LOG(debug, "Called JwtVerificationFilter : {}", __func__);
  if (state_ != Calling) {
    cancel_check_ = nullptr;
  }
  state_ = Responded;
  if (cancel_check_) {
    cancel_check_();
    cancel_check_ = nullptr;
  }
}

FilterHeadersStatus JwtVerificationFilter::decodeHeaders(HeaderMap& headers,
                                                         bool) {
  ENVOY_LOG(debug, "Called JwtVerificationFilter : {}", __func__);
  state_ = Calling;
  stopped_ = false;

  cancel_check_ = controller_.Verify(
      headers, [this](const Auth::Status& status) { completeCheck(status); });

  if (state_ == Complete) {
    return FilterHeadersStatus::Continue;
  }
  ENVOY_LOG(debug, "Called JwtVerificationFilter : {} Stop", __func__);
  stopped_ = true;
  return FilterHeadersStatus::StopIteration;
}

void JwtVerificationFilter::completeCheck(const Auth::Status& status) {
  ENVOY_LOG(debug, "Called JwtVerificationFilter : check complete {}",
            int(status));
  // This stream has been reset, abort the callback.
  if (state_ == Responded) {
    return;
  }
  if (status != Auth::Status::OK) {
    state_ = Responded;
    // verification failed
    Code code = Code(401);  // Unauthorized
    // return failure reason as message body
    Utility::sendLocalReply(*decoder_callbacks_, false, code,
                            Auth::StatusToString(status));
    return;
  }

  state_ = Complete;
  if (stopped_) {
    decoder_callbacks_->continueDecoding();
  }
}

FilterDataStatus JwtVerificationFilter::decodeData(Buffer::Instance&, bool) {
  ENVOY_LOG(debug, "Called JwtVerificationFilter : {}", __func__);
  if (state_ == Calling) {
    return FilterDataStatus::StopIterationAndBuffer;
  }
  return FilterDataStatus::Continue;
}

FilterTrailersStatus JwtVerificationFilter::decodeTrailers(HeaderMap&) {
  ENVOY_LOG(debug, "Called JwtVerificationFilter : {}", __func__);
  if (state_ == Calling) {
    return FilterTrailersStatus::StopIteration;
  }
  return FilterTrailersStatus::Continue;
}

void JwtVerificationFilter::setDecoderFilterCallbacks(
    StreamDecoderFilterCallbacks& callbacks) {
  ENVOY_LOG(debug, "Called JwtVerificationFilter : {}", __func__);
  decoder_callbacks_ = &callbacks;
}

}  // Http
}  // Envoy
