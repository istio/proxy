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

#ifndef PROXY_PER_ROUTE_FILTER_H
#define PROXY_PER_ROUTE_FILTER_H

#include "envoy/http/filter.h"
#include "envoy/router/router.h"

#include <functional>
#include <memory>

namespace Envoy {
namespace Http {

template <typename InnerFilter>
class PerRouteFilter : public Http::StreamFilter, public AccessLog::Instance {
 public:
  typedef std::unique_ptr<InnerFilter> InnerFilterPtr;
  typedef std::function<InnerFilterPtr(const Envoy::Router::RouteEntry *)>
      Constructor;

  PerRouteFilter(Constructor constructor)
      : constructor_(constructor), inner_filter_(nullptr) {}

  void log(const Http::HeaderMap *request_headers,
           const Http::HeaderMap *response_headers,
           const RequestInfo::RequestInfo &request_info) override {
    if (constructor_) {
      Constructor constructor;
      constructor.swap(constructor_);
      inner_filter_ = constructor(request_info.routeEntry());
    }
    auto filter = dynamic_cast<AccessLog::Instance *>(inner_filter_.get());
    if (filter) {
      filter->log(request_headers, response_headers, request_info);
    }
  }

  void onDestroy() override {
    auto filter = dynamic_cast<Http::StreamFilterBase *>(inner_filter_.get());
    if (filter) {
      filter->onDestroy();
    }
  }

  FilterHeadersStatus decodeHeaders(HeaderMap &headers,
                                    bool end_stream) override {
    if (decoder_callbacks_ && constructor_) {
      Constructor constructor;
      constructor.swap(constructor_);
      inner_filter_ = constructor(entry(decoder_callbacks_->route()));
    }
    auto filter =
        dynamic_cast<Http::StreamDecoderFilter *>(inner_filter_.get());
    if (filter) {
      if (decoder_callbacks_) {
        filter->setDecoderFilterCallbacks(*decoder_callbacks_);
        decoder_callbacks_ = nullptr;
      }
      return filter->decodeHeaders(headers, end_stream);
    }
    return FilterHeadersStatus::Continue;
  }

  FilterDataStatus decodeData(Buffer::Instance &data,
                              bool end_stream) override {
    auto filter =
        dynamic_cast<Http::StreamDecoderFilter *>(inner_filter_.get());
    if (filter) {
      return filter->decodeData(data, end_stream);
    }
    return FilterDataStatus::Continue;
  }

  FilterTrailersStatus decodeTrailers(HeaderMap &trailers) override {
    auto filter =
        dynamic_cast<Http::StreamDecoderFilter *>(inner_filter_.get());
    if (filter) {
      return filter->decodeTrailers(trailers);
    }
    return FilterTrailersStatus::Continue;
  }

  void setDecoderFilterCallbacks(
      StreamDecoderFilterCallbacks &callbacks) override {
    decoder_callbacks_ = &callbacks;
  }

  FilterHeadersStatus encodeHeaders(HeaderMap &headers,
                                    bool end_stream) override {
    if (encoder_callbacks_ && constructor_) {
      Constructor constructor;
      constructor.swap(constructor_);
      inner_filter_ = constructor(entry(encoder_callbacks_->route()));
    }
    auto filter =
        dynamic_cast<Http::StreamEncoderFilter *>(inner_filter_.get());
    if (filter) {
      if (encoder_callbacks_) {
        filter->setEncoderFilterCallbacks(*encoder_callbacks_);
        encoder_callbacks_ = nullptr;
      }
      return filter->encodeHeaders(headers, end_stream);
    }
    return FilterHeadersStatus::Continue;
  }

  FilterHeadersStatus encode100ContinueHeaders(HeaderMap &headers) override {
    auto filter =
        dynamic_cast<Http::StreamEncoderFilter *>(inner_filter_.get());
    if (filter) {
      return filter->encode100ContinueHeaders(headers);
    }
    return FilterHeadersStatus::Continue;
  }

  FilterDataStatus encodeData(Buffer::Instance &data,
                              bool end_stream) override {
    auto filter =
        dynamic_cast<Http::StreamEncoderFilter *>(inner_filter_.get());
    if (filter) {
      return filter->encodeData(data, end_stream);
    }
    return FilterDataStatus::Continue;
  }

  FilterTrailersStatus encodeTrailers(HeaderMap &trailers) override {
    auto filter =
        dynamic_cast<Http::StreamEncoderFilter *>(inner_filter_.get());
    if (filter) {
      return filter->encodeTrailers(trailers);
    }
    return FilterTrailersStatus::Continue;
  }

  void setEncoderFilterCallbacks(
      StreamEncoderFilterCallbacks &callbacks) override {
    encoder_callbacks_ = &callbacks;
  }

 private:
  Constructor constructor_;
  InnerFilterPtr inner_filter_;
  StreamDecoderFilterCallbacks *decoder_callbacks_{nullptr};
  StreamEncoderFilterCallbacks *encoder_callbacks_{nullptr};

  const Router::RouteEntry *entry(const Router::RouteConstSharedPtr &route) {
    if (route) {
      return route->routeEntry();
    }
    return nullptr;
  }
};
}
}

#endif  // PROXY_PER_ROUTE_FILTER_H
