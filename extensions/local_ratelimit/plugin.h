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

#include "absl/container/flat_hash_map.h"
#include "extensions/local_ratelimit/config/v1alpha1/local_ratelimit_config.pb.h"
#include "extensions/common/context.h"

#ifndef NULL_PLUGIN

#include <assert.h>
#define ASSERT(_X) assert(_X)

#include "proxy_wasm_intrinsics.h"

#else

#include "include/proxy-wasm/null_plugin.h"

namespace proxy_wasm {
namespace null_plugin {
namespace LocalRateLimit {
namespace Plugin {

#endif


// PluginRootContext is the root context for all streams processed by the
// thread. It has the same lifetime as the worker thread and acts as target for
// interactions that outlives individual stream, e.g. timer, async calls.
class PluginRootContext : public RootContext {
 public:
  PluginRootContext(uint32_t id, std::string_view root_id)
      : RootContext(id, root_id) {}
  ~PluginRootContext() = default;

  bool onConfigure(size_t) override;
  bool configure(size_t);

  const localratelimit::config::v1alpha1::LocalRateLimitConfig& config()
{return config_;}

  const std::unordered_map<std::string, size_t>&  input_expressions(){
    return input_expressions_;
  }
  bool initialized() const { return initialized_; };

 private:
  localratelimit::config::v1alpha1::LocalRateLimitConfig config_;
  std::unordered_map<std::string, size_t> input_expressions_;

  bool initialized_ = false;
};

// Per-stream context.
class PluginContext : public Context {
 public:
  explicit PluginContext(uint32_t id, RootContext* root) : Context(id, root) {}

  FilterHeadersStatus onRequestHeaders(uint32_t, bool);

 private:
  inline PluginRootContext* rootContext() {
    return dynamic_cast<PluginRootContext*>(this->root());
  };

};

#ifdef NULL_PLUGIN
}  // namespace Plugin
}  // namespace LocalRateLimit
}  // namespace null_plugin
}  // namespace proxy_wasm
#endif
