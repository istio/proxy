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

// NOTE: every call to this function will alloc a new metric object. Though this
// object is the same in all calls, it cannot be extracted out to a global
// variable. The metric object caches all fully resolved metrics and won't make
// define metrics abi call when the same resolved metrics are defined. In Null
// VM case, since global objects are shared between root contexts in base VM and
// in thread local VM, and the same metrics definition code path will excercised
// in both VMs (we don't really have a way to distinguish base VM and thread
// local VM), the cache will make thread local root context miss metric
// definition at host side. In real Wasm VM, this can be extracted to a global
// var.
uint32_t newExportCallMetric(const std::string& type, bool success) {
  Metric export_call(MetricType::Counter, "stackdriver_export_call",
                     {MetricTag{"wasm_filter", MetricTag::TagType::String},
                      MetricTag{"type", MetricTag::TagType::String},
                      MetricTag{"success", MetricTag::TagType::Bool}});

  return export_call.resolve("stackdriver_filter", type, success);
}

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
