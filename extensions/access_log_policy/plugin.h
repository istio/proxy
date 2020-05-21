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
#include "extensions/access_log_policy/config/v1alpha1/access_log_policy_config.pb.h"
#include "extensions/common/context.h"
#include "extensions/common/istio_dimensions.h"

#ifndef NULL_PLUGIN

#include <assert.h>
#define ASSERT(_X) assert(_X)

#include "proxy_wasm_intrinsics.h"

static const std::string EMPTY_STRING;

#else

#include "extensions/common/wasm/null/null_plugin.h"

namespace Envoy {
namespace Extensions {
namespace Wasm {
namespace AccessLogPolicy {
namespace Plugin {

using namespace Envoy::Extensions::Common::Wasm::Null::Plugin;

// TODO(jplevyak): move these into the base envoy repo
using WasmResult = Envoy::Extensions::Common::Wasm::WasmResult;
using NullPluginRegistry =
    ::Envoy::Extensions::Common::Wasm::Null::NullPluginRegistry;
using ::Wasm::Common::IstioDimensions;

#endif

const size_t DefaultClientCacheMaxSize = 500;

// PluginRootContext is the root context for all streams processed by the
// thread. It has the same lifetime as the worker thread and acts as target for
// interactions that outlives individual stream, e.g. timer, async calls.
class PluginRootContext : public RootContext {
 public:
  PluginRootContext(uint32_t id, StringView root_id)
      : RootContext(id, root_id) {}
  ~PluginRootContext() = default;

  bool onConfigure(size_t) override;
  bool configure(size_t);

  long long lastLogTimeNanos(const IstioDimensions& key) {
    if (cache_.contains(key)) {
      return cache_[key];
    }
    return 0;
  }

  void updateLastLogTimeNanos(const IstioDimensions& key,
                              long long last_log_time_nanos);
  long long logTimeDurationNanos() { return log_time_duration_nanos_; };
  bool initialized() const { return initialized_; };

 private:
  accesslogpolicy::config::v1alpha1::AccessLogPolicyConfig config_;
  // Cache storing last log time by a client.
  absl::flat_hash_map<IstioDimensions, long long> cache_;
  int32_t max_client_cache_size_ = DefaultClientCacheMaxSize;
  long long log_time_duration_nanos_;

  bool initialized_ = false;
};

// Per-stream context.
class PluginContext : public Context {
 public:
  explicit PluginContext(uint32_t id, RootContext* root) : Context(id, root) {}

  void onLog() override;

 private:
  inline PluginRootContext* rootContext() {
    return dynamic_cast<PluginRootContext*>(this->root());
  };
  inline long long lastLogTimeNanos() {
    return rootContext()->lastLogTimeNanos(istio_dimensions_);
  };
  inline void updateLastLogTimeNanos(long long last_log_time_nanos) {
    rootContext()->updateLastLogTimeNanos(istio_dimensions_,
                                          last_log_time_nanos);
  };
  inline long long logTimeDurationNanos() {
    return rootContext()->logTimeDurationNanos();
  };
  bool isRequestFailed();

  IstioDimensions istio_dimensions_;
};

#ifdef NULL_PLUGIN
}  // namespace Plugin
}  // namespace AccessLogPolicy
}  // namespace Wasm
}  // namespace Extensions
}  // namespace Envoy
#endif
