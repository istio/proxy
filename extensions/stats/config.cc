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

#include "extensions/stats/config.h"

// WASM_PROLOG
#ifndef NULL_PLUGIN
#include "api/wasm/cpp/proxy_wasm_intrinsics.h"

#else // NULL_PLUGIN

#include "extensions/common/wasm/null/null.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Wasm {
namespace Null {
namespace Plugin {
#endif // NULL_PLUGIN

// END WASM_PROLOG

namespace Stats {


void PluginRootContext::onConfigure(
  std::unique_ptr<WasmData> ABSL_ATTRIBUTE_UNUSED configuration) {};

void PluginContext::onLog()  {

  Common::RequestInfo requestInfo;

  Common::initializeRequestInfo(&requestInfo);

  std::string id = "id1";

  auto counter_it = counter_map_.find(id);
  if (counter_it == counter_map_.end()) {
  auto counter = istio_requests_total_metric_->resolve("SRC_A", "SRC_V",
                                                       "DEST_A", "DEST_B");
  counter_map_.
  emplace(id, counter
  );
  counter++;
  }
  else {
  counter_it->second++;
  }
}

// Registration glue

NullVmPluginRootRegistry *context_registry_{};

class StatsFactory : public NullVmPluginFactory {
public:
  const std::string name() const override { return "envoy.wasm.stats"; }

  std::unique_ptr<NullVmPlugin> create() const override {
    return std::make_unique<NullVmPlugin>(
      context_registry_);
  }
};

static Registry::RegisterFactory<StatsFactory, NullVmPluginFactory>
  register_;

}  // namespace Stats

// WASM_EPILOG
#ifdef NULL_PLUGIN
}  // namespace Plugin
}  // namespace Null
}  // namespace Wasm
}  // namespace Common
}  // namespace Extensions
}  // namespace Envoy
#endif
