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

#include "include/proxy-wasm/null_plugin.h"

namespace proxy_wasm {
namespace null_plugin {
namespace BasicAuth {
namespace Plugin {
NullPluginRegistry* context_registry_{};
}  // namespace Plugin

// Registration glue
RegisterNullVmPluginFactory register_basic_auth_filter(
    "envoy.wasm.basic_auth",
    []() { return std::make_unique<NullPlugin>(Plugin::context_registry_); });

}  // namespace BasicAuth
}  // namespace null_plugin
}  // namespace proxy_wasm
