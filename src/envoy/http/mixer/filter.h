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

#pragma once

#include "common/common/logger.h"
#include "envoy/access_log/access_log.h"
#include "envoy/http/filter.h"
#include "src/envoy/http/mixer/control.h"

namespace Envoy {
namespace Http {
namespace Mixer {

// The struct to store per-route service config and its hash.
struct PerRouteServiceConfig : public Router::RouteSpecificFilterConfig {
  // The per_route service config.
  ::istio::mixer::v1::config::client::ServiceConfig config;

  // Its config hash
  std::string hash;
};

// The struct to store gRPC message counter state.
struct GrpcMessageCounter {
  GrpcMessageCounter() : state(ExpectByte0), current_size(0), count(0){};

  // gRPC uses 5 byte header to encode subsequent message length
  enum GrpcReadState {
    ExpectByte0 = 0,
    ExpectByte1,
    ExpectByte2,
    ExpectByte3,
    ExpectByte4,
    ExpectMessage
  };

  // current read state
  GrpcReadState state;

  // current message size
  uint32_t current_size;

  // message counter
  uint64_t count;
};

class Filter : public StreamFilter,
               public AccessLog::Instance,
               public Logger::Loggable<Logger::Id::filter> {
 public:
  Filter(Control& control);

  // Http::StreamDecoderFilter
  FilterHeadersStatus decodeHeaders(HeaderMap& headers, bool) override;
  FilterDataStatus decodeData(Buffer::Instance& data, bool end_stream) override;
  FilterTrailersStatus decodeTrailers(HeaderMap& trailers) override;
  void setDecoderFilterCallbacks(
      StreamDecoderFilterCallbacks& callbacks) override;

  // Http::StreamFilterBase
  void onDestroy() override;

  // Http::StreamEncoderFilter
  FilterHeadersStatus encode100ContinueHeaders(HeaderMap&) override {
    return FilterHeadersStatus::Continue;
  }
  FilterHeadersStatus encodeHeaders(HeaderMap& headers, bool) override;
  FilterDataStatus encodeData(Buffer::Instance&, bool) override;
  FilterTrailersStatus encodeTrailers(HeaderMap&) override {
    return FilterTrailersStatus::Continue;
  }
  Http::FilterMetadataStatus encodeMetadata(MetadataMap&) override {
    return FilterMetadataStatus::Continue;
  }
  void setEncoderFilterCallbacks(StreamEncoderFilterCallbacks&) override {}

  // This is the callback function when Check is done.
  void completeCheck(const ::istio::mixerclient::CheckResponseInfo& info);

  // Called when the request is completed.
  virtual void log(const HeaderMap* request_headers,
                   const HeaderMap* response_headers,
                   const HeaderMap* response_trailers,
                   const StreamInfo::StreamInfo& stream_info) override;

 private:
  // Read per-route config.
  void ReadPerRouteConfig(
      const Router::RouteEntry* entry,
      ::istio::control::http::Controller::PerRouteConfig* config);

  // Update header maps
  void UpdateHeaders(HeaderMap& headers,
                     const ::google::protobuf::RepeatedPtrField<
                         ::istio::mixer::v1::HeaderOperation>& operations);

  // The control object.
  Control& control_;
  // The request handler.
  std::unique_ptr<::istio::control::http::RequestHandler> handler_;

  enum State { NotStarted, Calling, Complete, Responded };
  // The state
  State state_;
  bool initiating_call_;

  // Point to the request HTTP headers
  HeaderMap* headers_;

  // Total number of bytes received, including request headers, body, and
  // trailers.
  uint64_t request_total_size_{0};

  // True for gRPC requests
  bool grpc_request_{false};
  GrpcMessageCounter grpc_request_counter_;
  GrpcMessageCounter grpc_response_counter_;

  // The stream decoder filter callback.
  StreamDecoderFilterCallbacks* decoder_callbacks_{nullptr};

  // Returned directive
  ::istio::mixer::v1::RouteDirective route_directive_{
      ::istio::mixer::v1::RouteDirective::default_instance()};
};

}  // namespace Mixer
}  // namespace Http
}  // namespace Envoy
