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

#ifndef NULL_PLUGIN
#include "api/wasm/cpp/proxy_wasm_intrinsics.h"
#else
#include "include/proxy-wasm/null_plugin.h"

namespace proxy_wasm {
namespace null_plugin {

#endif

namespace Extensions {
namespace Stackdriver {
namespace Common {

// newExportCallMetric create a fully resolved metric based on the given type
// and a boolean which indicates whether the call succeeds or not. Current type
// could only be logging.
uint32_t newExportCallMetric(const std::string& type, bool success);

} // namespace Common
} // namespace Stackdriver
} // namespace Extensions

#ifdef NULL_PLUGIN
} // namespace null_plugin
} // namespace proxy_wasm
#endif
