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

#include "src/envoy/http/authn_wasm/config.h"

#include "authentication/v1alpha1/policy.pb.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/json_util.h"

namespace iaapi = istio::authentication::v1alpha1;

namespace Envoy {
namespace Extensions {
namespace Wasm {
namespace AuthnWasm {

// imports from the low-level API
using Common::Wasm::Null::NullVmPluginFactory;

// TODO: The WASM library should be improved with:
//  1) allow logging with a unified prefix.
//  2) expose macros that evaluates logDebug() only for debug logging.
using Common::Wasm::Null::Plugin::logDebug;
using Common::Wasm::Null::Plugin::logError;

using ::google::protobuf::TextFormat;
using namespace google::protobuf::util;

void PluginRootContext::onConfigure(std::unique_ptr<WasmData> configuration) {
  logDebug("called PluginRootContext::onConfigure()");
  JsonParseOptions json_options;
  Status status = JsonStringToMessage(configuration->toString(),
                                      &filter_config_, json_options);
  if (status != Status::OK) {
    logError("Cannot parse authentication filter config: " +
             configuration->toString());
  } else {
    std::string out_str;
    TextFormat::PrintToString(filter_config_, &out_str);
    logDebug("Applied authentication filter config:\n" + out_str);
  }
}

void PluginRootContext::onStart() {
  logDebug("called PluginRootContext::onStart()");
};

void PluginRootContext::onTick() {
  logDebug("called PluginRootContext::onTick()");
};

void PluginContext::onCreate() { logDebug("called PluginContext::onCreate()"); }

Http::FilterHeadersStatus PluginContext::onRequestHeaders() {
  const auto& config = filter_config();
  for (const auto& method : config.policy().peers()) {
    switch (method.params_case()) {
      case iaapi::PeerAuthenticationMethod::ParamsCase::kMtls:
        logDebug("peer authentication for mTLS:\n" + method.DebugString());
        break;
      case iaapi::PeerAuthenticationMethod::ParamsCase::kJwt:
        logDebug("peer authentication for JWT:\n" + method.DebugString());
        break;
      default:
        logDebug("unknown peer authentication:\n" + method.DebugString());
        break;
    }
  }

  for (const auto& method : config.policy().origins()) {
    logDebug("origin authentication for JWT:\n" + method.jwt().DebugString());
  }

  return Http::FilterHeadersStatus::Continue;
}

Http::FilterHeadersStatus PluginContext::onResponseHeaders() {
  logDebug("called PluginContext::onResponseHeaders()");
  return Http::FilterHeadersStatus::Continue;
}

// Registration glue

Common::Wasm::Null::NullVmPluginRootRegistry* context_registry_{};

class AuthnWasmFactory : public Common::Wasm::Null::NullVmPluginFactory {
 public:
  const std::string name() const override { return "envoy.wasm.authn"; }
  std::unique_ptr<Common::Wasm::Null::NullVmPlugin> create() const override {
    return std::make_unique<Common::Wasm::Null::NullVmPlugin>(
        Envoy::Extensions::Wasm::AuthnWasm::context_registry_);
  }
};

static Registry::RegisterFactory<AuthnWasmFactory, NullVmPluginFactory>
    register_;

}  // namespace AuthnWasm
}  // namespace Wasm
}  // namespace Extensions
}  // namespace Envoy
