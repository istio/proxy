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

#pragma once

#include "absl/strings/string_view.h"
#include "envoy/config/filter/http/authn/v2alpha1/config.pb.h"
#include "proxy_wasm_intrinsics.h"
#include "src/envoy/http/authn_wasm/authenticator/base.h"

namespace Envoy {
namespace Wasm {
namespace Http {
namespace AuthN {

using istio::envoy::config::filter::http::authn::v2alpha1::FilterConfig;
using StringView = absl::string_view;

// AuthnRootContext is the root context for all streams processed by the
// thread. It has the same lifetime as the worker thread and acts as target for
// interactions that outlives individual stream, e.g. timer, async calls.
class AuthnRootContext : public RootContext {
 public:
  AuthnRootContext(uint32_t id, absl::string_view root_id)
      : RootContext(id, root_id) {}
  ~AuthnRootContext() {}

  // RootContext
  bool validateConfiguration(size_t) override { return true; }
  bool onConfigure(size_t) override { return true; };
  bool onStart(size_t) override { return true; }
  void onTick() override {}
  void onQueueReady(uint32_t) override {}
  bool onDone() override { return true; }

  // Low level HTTP/gRPC interface.
  void onHttpCallResponse(uint32_t token, uint32_t headers, size_t body_size,
                          uint32_t trailers) override {}
  void onGrpcReceiveInitialMetadata(uint32_t token, uint32_t headers) override {
  }
  void onGrpcReceiveTrailingMetadata(uint32_t token,
                                     uint32_t trailers) override {}
  void onGrpcReceive(uint32_t token, size_t body_size) override {}
  void onGrpcClose(uint32_t token, GrpcStatus status) override {}

  const FilterConfig& filterConfig() { return filter_config_; };

 private:
  FilterConfig filter_config_;
};

// Per-stream context.
class AuthnContext : public Context {
 public:
  explicit AuthnContext(uint32_t id, RootContext* root) : Context(id, root) {}
  ~AuthnContext() = default;

  void onCreate() override {}

  // Context
  FilterStatus onNewConnection() override { return FilterStatus::Continue; }
  FilterStatus onDownstreamData(size_t, bool) override {
    return FilterStatus::Continue;
  }
  FilterStatus onUpstreamData(size_t, bool) override {
    return FilterStatus::Continue;
  }
  void onDownstreamConnectionClose(PeerType) override {}
  void onUpstreamConnectionClose(PeerType) override {}
  FilterHeadersStatus onRequestHeaders(uint32_t) override;
  FilterMetadataStatus onRequestMetadata(uint32_t) override {
    return FilterMetadataStatus::Continue;
  }
  FilterDataStatus onRequestBody(size_t, bool) override {
    return FilterDataStatus::Continue;
  }
  FilterTrailersStatus onRequestTrailers(uint32_t) override {
    return FilterTrailersStatus::Continue;
  }
  FilterHeadersStatus onResponseHeaders(uint32_t) override {
    return FilterHeadersStatus::Continue;
  }
  FilterMetadataStatus onResponseMetadata(uint32_t) override {
    return FilterMetadataStatus::Continue;
  }
  FilterDataStatus onResponseBody(size_t, bool) override {
    return FilterDataStatus::Continue;
  }
  FilterTrailersStatus onResponseTrailers(uint32_t) override {
    return FilterTrailersStatus::Continue;
  }
  void onDone() override {}
  void onLog() override {}

  const FilterConfig& filterConfig() { return rootContext()->filterConfig(); };

 private:
  std::unique_ptr<AuthenticatorBase> createPeerAuthenticator(
      FilterContextPtr filter_context);
  // TODO(shikugawa): origin authenticator implementation.
  // std::unique_ptr<istio::AuthN::AuthenticatorBase> createOriginAuthenticator(
  //   istio::AuthN::FilterContext* filter_context);

  inline AuthnRootContext* rootContext() {
    return dynamic_cast<AuthnRootContext*>(this->root());
  };

  // Context for authentication process. Created in decodeHeader to start
  // authentication process.
  FilterContextPtr filter_context_;
};

static RegisterContextFactory register_AuthnWasm(
    CONTEXT_FACTORY(AuthnContext), ROOT_FACTORY(AuthnRootContext));

}  // namespace AuthN
}  // namespace Http
}  // namespace Wasm
}  // namespace Envoy
