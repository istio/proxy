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

#include "src/envoy/http/mixer/filter.h"

#include "common/common/base64.h"
#include "common/protobuf/utility.h"
#include "include/istio/utils/status.h"
#include "src/envoy/http/mixer/check_data.h"
#include "src/envoy/http/mixer/report_data.h"
#include "src/envoy/utils/authn.h"
#include "src/envoy/utils/header_update.h"

using ::google::protobuf::util::Status;
using ::istio::mixer::v1::RouteDirective;
using ::istio::mixerclient::CheckResponseInfo;

namespace Envoy {
namespace Http {
namespace Mixer {

Filter::Filter(Control& control)
    : control_(control),
      state_(NotStarted),
      initiating_call_(false),
      headers_(nullptr) {
  ENVOY_LOG(debug, "Called Mixer::Filter : {}", __func__);
}

void Filter::ReadPerRouteConfig(
    const Router::RouteEntry* entry,
    ::istio::control::http::Controller::PerRouteConfig* config) {
  if (entry == nullptr) {
    return;
  }

  // Check v2 per-route config.
  auto route_cfg = entry->perFilterConfigTyped<PerRouteServiceConfig>("mixer");
  if (route_cfg) {
    if (!control_.controller()->LookupServiceConfig(route_cfg->hash)) {
      control_.controller()->AddServiceConfig(route_cfg->hash,
                                              route_cfg->config);
    }
    config->service_config_id = route_cfg->hash;
    return;
  }
}

FilterHeadersStatus Filter::decodeHeaders(HeaderMap& headers, bool) {
  ENVOY_LOG(debug, "Called Mixer::Filter : {}", __func__);
  request_total_size_ += headers.refreshByteSize();

  ::istio::control::http::Controller::PerRouteConfig config;
  auto route = decoder_callbacks_->route();
  if (route) {
    ReadPerRouteConfig(route->routeEntry(), &config);
  }
  handler_ = control_.controller()->CreateRequestHandler(config);

  state_ = Calling;
  initiating_call_ = true;
  CheckData check_data(headers,
                       decoder_callbacks_->streamInfo().dynamicMetadata(),
                       decoder_callbacks_->connection());
  Utils::HeaderUpdate header_update(&headers);
  headers_ = &headers;
  handler_->Check(
      &check_data, &header_update,
      control_.GetCheckTransport(decoder_callbacks_->activeSpan()),
      [this](const CheckResponseInfo& info) { completeCheck(info); });
  initiating_call_ = false;

  if (state_ == Complete) {
    return FilterHeadersStatus::Continue;
  }
  ENVOY_LOG(debug, "Called Mixer::Filter : {} Stop", __func__);
  return FilterHeadersStatus::StopIteration;
}

FilterDataStatus Filter::decodeData(Buffer::Instance& data, bool end_stream) {
  ENVOY_LOG(debug, "Called Mixer::Filter : {} ({}, {})", __func__,
            data.length(), end_stream);
  request_total_size_ += data.length();
  if (state_ == Calling) {
    return FilterDataStatus::StopIterationAndWatermark;
  }
  return FilterDataStatus::Continue;
}

FilterTrailersStatus Filter::decodeTrailers(HeaderMap& trailers) {
  ENVOY_LOG(debug, "Called Mixer::Filter : {}", __func__);
  request_total_size_ += trailers.refreshByteSize();
  if (state_ == Calling) {
    return FilterTrailersStatus::StopIteration;
  }
  return FilterTrailersStatus::Continue;
}

void Filter::UpdateHeaders(
    HeaderMap& headers, const ::google::protobuf::RepeatedPtrField<
                            ::istio::mixer::v1::HeaderOperation>& operations) {
  for (auto const iter : operations) {
    switch (iter.operation()) {
      case ::istio::mixer::v1::HeaderOperation_Operation_REPLACE:
        headers.remove(LowerCaseString(iter.name()));
        headers.addCopy(LowerCaseString(iter.name()), iter.value());
        break;
      case ::istio::mixer::v1::HeaderOperation_Operation_REMOVE:
        headers.remove(LowerCaseString(iter.name()));
        break;
      case ::istio::mixer::v1::HeaderOperation_Operation_APPEND:
        headers.addCopy(LowerCaseString(iter.name()), iter.value());
        break;
      default:
        PANIC("unreachable header operation");
    }
  }
}

FilterHeadersStatus Filter::encodeHeaders(HeaderMap& headers, bool) {
  ENVOY_LOG(debug, "Called Mixer::Filter : {} {}", __func__, state_);
  // Init state is possible if a filter prior to mixerfilter interrupts the
  // filter chain
  ASSERT(state_ == NotStarted || state_ == Complete || state_ == Responded);
  if (state_ == Complete) {
    // handle response header operations
    UpdateHeaders(headers, route_directive_.response_header_operations());
  }
  return FilterHeadersStatus::Continue;
}

void Filter::setDecoderFilterCallbacks(
    StreamDecoderFilterCallbacks& callbacks) {
  ENVOY_LOG(debug, "Called Mixer::Filter : {}", __func__);
  decoder_callbacks_ = &callbacks;
}

void Filter::completeCheck(const CheckResponseInfo& info) {
  const Status& status = info.status();

  ENVOY_LOG(debug, "Called Mixer::Filter : check complete {}",
            status.ToString());
  // This stream has been reset, abort the callback.
  if (state_ == Responded) {
    return;
  }

  route_directive_ = info.routeDirective();

  Utils::CheckResponseInfoToStreamInfo(info, decoder_callbacks_->streamInfo());

  // handle direct response from the route directive
  if (route_directive_.direct_response_code() != 0) {
    int status_code = route_directive_.direct_response_code();
    ENVOY_LOG(debug, "Mixer::Filter direct response {}", status_code);
    state_ = Responded;
    decoder_callbacks_->sendLocalReply(
        Code(status_code), route_directive_.direct_response_body(),
        [this](HeaderMap& headers) {
          UpdateHeaders(headers, route_directive_.response_header_operations());
        },
        absl::nullopt);
    return;
  }

  // create a local reply for status not OK even if there is no direct response
  if (!status.ok()) {
    state_ = Responded;

    int status_code = ::istio::utils::StatusHttpCode(status.error_code());
    decoder_callbacks_->sendLocalReply(Code(status_code), status.ToString(),
                                       nullptr, absl::nullopt);
    return;
  }

  state_ = Complete;

  // handle request header operations
  if (nullptr != headers_) {
    UpdateHeaders(*headers_, route_directive_.request_header_operations());
    headers_ = nullptr;
    if (route_directive_.request_header_operations().size() > 0) {
      decoder_callbacks_->clearRouteCache();
    }
  }

  if (!initiating_call_) {
    decoder_callbacks_->continueDecoding();
  }
}

void Filter::onDestroy() {
  ENVOY_LOG(debug, "Called Mixer::Filter : {} state: {}", __func__, state_);
  if (state_ != Calling && handler_) {
    handler_->ResetCancel();
  }
  state_ = Responded;
  if (handler_) {
    handler_->CancelCheck();
  }
}

void Filter::log(const HeaderMap* request_headers,
                 const HeaderMap* response_headers,
                 const HeaderMap* response_trailers,
                 const StreamInfo::StreamInfo& stream_info) {
  ENVOY_LOG(debug, "Called Mixer::Filter : {}", __func__);
  if (!handler_) {
    if (request_headers == nullptr) {
      return;
    }

    // Here Request is rejected by other filters, Mixer filter is not called.
    ::istio::control::http::Controller::PerRouteConfig config;
    ReadPerRouteConfig(stream_info.routeEntry(), &config);
    handler_ = control_.controller()->CreateRequestHandler(config);
  }

  // If check is NOT called, check attributes are not extracted.
  CheckData check_data(*request_headers, stream_info.dynamicMetadata(),
                       decoder_callbacks_->connection());
  // response trailer header is not counted to response total size.
  ReportData report_data(request_headers, response_headers, response_trailers,
                         stream_info, request_total_size_);
  handler_->Report(&check_data, &report_data);
}

}  // namespace Mixer
}  // namespace Http
}  // namespace Envoy
