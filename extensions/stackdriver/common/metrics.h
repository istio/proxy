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
#include "extensions/common/wasm/null/null_plugin.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Wasm {
namespace Null {
namespace Plugin {

#endif

namespace Extensions {
namespace Stackdriver {
namespace Common {

// Tracks gRPC export call made by the plugin.
static Metric export_call(MetricType::Counter, "export_call",
                          {MetricTag{"wasm_filter", MetricTag::TagType::String},
                           MetricTag{"type", MetricTag::TagType::String},
                           MetricTag{"success", MetricTag::TagType::Bool}});

// Partially resolve the export call metrics by setting wasm_filter tag as
// stackdriver_filter.
static Metric stackdriver_export_call =
    export_call.partiallyResolve("stackdriver_filter");

}  // namespace Common
}  // namespace Stackdriver
}  // namespace Extensions

#ifdef NULL_PLUGIN
}  // namespace Plugin
}  // namespace Null
}  // namespace Wasm
}  // namespace Common
}  // namespace Extensions
}  // namespace Envoy
#endif
