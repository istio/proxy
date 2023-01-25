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

#include "extensions/access_log_policy/plugin.h"
#include "source/common/common/base64.h"

namespace proxy_wasm {
namespace null_plugin {
namespace AccessLogPolicy {
namespace Plugin {
NullPluginRegistry* context_registry_{};
} // namespace Plugin

// Registration glue
RegisterNullVmPluginFactory register_access_log_policy_filter("envoy.wasm.access_log_policy", []() {
  return std::make_unique<NullPlugin>(Plugin::context_registry_);
});

} // namespace AccessLogPolicy
} // namespace null_plugin
} // namespace proxy_wasm
