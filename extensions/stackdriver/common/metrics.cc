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

#include "extensions/stackdriver/common/metrics.h"

#ifdef NULL_PLUGIN
namespace proxy_wasm {
namespace null_plugin {
#endif

namespace Extensions {
namespace Stackdriver {
namespace Common {

uint32_t newExportCallMetric(const std::string& type, bool success) {
  // NOTE: export_call cannot be a static global object, because in Nullvm,
  // global metric is shared by base VM and thread local VM, but at host side,
  // metrics are attached to a specific VM/root context. Since (1) metric object
  // keeps an internal map which records all fully resolved metrics and avoid
  // define metric ABI call when the same metric are seen (2) base VM always
  // initiliazing before thread local VM, sharing a global metric object between
  // base VM and thread local VM would cause host side thread local VM root
  // context missing metric definition. This is not going to be a problem with
  // real Wasm VM due to memory isolation.
  Metric export_call(MetricType::Counter, "envoy_export_call",
                     {MetricTag{"wasm_filter", MetricTag::TagType::String},
                      MetricTag{"type", MetricTag::TagType::String},
                      MetricTag{"success", MetricTag::TagType::Bool}});

  return export_call.resolve("stackdriver_filter", type, success);
}

} // namespace Common
} // namespace Stackdriver
} // namespace Extensions

#ifdef NULL_PLUGIN
} // namespace null_plugin
} // namespace proxy_wasm
#endif
