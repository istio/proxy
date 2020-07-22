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
#include "extensions/authn/authenticator_base.h"

#include "envoy/config/core/v3/base.pb.h"

#ifndef NULL_PLUGIN
#include "proxy_wasm_intrinsics.h"
#else
#include "include/proxy-wasm/null_plugin.h"

using proxy_wasm::null_plugin::logDebug;
using proxy_wasm::null_plugin::logError;

namespace proxy_wasm {
namespace null_plugin {
namespace AuthN {

#endif

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

  const FilterConfig& filterConfig() { return filter_config_; };

 private:
  FilterConfig filter_config_;
};

// Per-stream context.
class AuthnContext : public Context {
 public:
  explicit AuthnContext(uint32_t id, RootContext* root) : Context(id, root) {}
  ~AuthnContext() = default;

  FilterHeadersStatus onRequestHeaders(uint32_t, bool) override;

  const FilterConfig& filterConfig();

 private:
  // std::unique_ptr<AuthenticatorBase> createPeerAuthenticator(
  // FilterContext* filter_context);
  // TODO(shikugawa): origin authenticator implementation.
  // std::unique_ptr<istio::AuthN::AuthenticatorBase> createOriginAuthenticator(
  //   istio::AuthN::FilterContext* filter_context);
};

#ifdef NULL_PLUGIN
PROXY_WASM_NULL_PLUGIN_REGISTRY;
#endif

static RegisterContextFactory register_AuthnWasm(
    CONTEXT_FACTORY(AuthnContext), ROOT_FACTORY(AuthnRootContext));

#ifdef NULL_PLUGIN
}  // namespace AuthN
}  // namespace null_plugin
}  // namespace proxy_wasm

#endif