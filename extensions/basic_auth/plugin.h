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

#pragma once

#include "extensions/common/context.h"
#include "google/protobuf/util/json_util.h"

#ifndef NULL_PLUGIN

#include <assert.h>
#define ASSERT(_X) assert(_X)

#include "proxy_wasm_intrinsics.h"

static const std::string EMPTY_STRING;

#else

#include "include/proxy-wasm/null_plugin.h"

namespace proxy_wasm {
namespace null_plugin {
namespace BasicAuth {
namespace Plugin {

#endif

using google::protobuf::util::JsonParseOptions;
using google::protobuf::util::Status;
// PluginRootContext is the root context for all streams processed by the
// thread. It has the same lifetime as the worker thread and acts as target for
// interactions that outlives individual stream, e.g. timer, async calls.
class PluginRootContext : public RootContext {
 public:
  PluginRootContext(uint32_t id, std::string_view root_id)
      : RootContext(id, root_id) {}
  ~PluginRootContext() {}
  bool onConfigure(size_t) override;
  PluginRootContext();

  struct headerData {
    std::unordered_set<std::string> request_methods;
    std::unordered_set<std::string> encoded_credentials;
  };
  std::unordered_map<
      std::string,
      std::unordered_map<std::string, PluginRootContext::headerData>>
  basicAuthConfigurationValue() {
    return basic_auth_configuration_;
  };

 private:
  bool configure(size_t);
  std::unordered_map<
      std::string,
      std::unordered_map<std::string, PluginRootContext::headerData>>
      basic_auth_configuration_;
};

// Per-stream context.
class PluginContext : public Context {
 public:
  explicit PluginContext(uint32_t id, RootContext* root) : Context(id, root) {}
  FilterHeadersStatus onRequestHeaders(uint32_t, bool) override;
  FilterHeadersStatus credentialsCheck(
      std::unordered_map<std::string,
                         PluginRootContext::headerData>::mapped_type,
      std::string);

 private:
  inline PluginRootContext* rootContext() {
    return dynamic_cast<PluginRootContext*>(this->root());
  };
  inline std::unordered_map<
      std::string,
      std::unordered_map<std::string, PluginRootContext::headerData>>
  basicAuthConfigurationValue() {
    return rootContext()->basicAuthConfigurationValue();
  };
};

#ifdef NULL_PLUGIN
}  // namespace Plugin
}  // namespace BasicAuth
}  // namespace null_plugin
}  // namespace proxy_wasm
#endif