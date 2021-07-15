/* Copyright 2018 Istio Authors. All Rights Reserved.
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
#include "source/common/stats/utility.h"
#include "source/server/admin/prometheus_stats.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Wasm {
namespace Istio {

// Stat prefix cannot be configured dynamically.
// https://github.com/envoyproxy/envoy/issues/14920
// https://github.com/istio/istio/issues/27635
class RegisterPrometheusNamespace {
 public:
  RegisterPrometheusNamespace() {
    ::Envoy::Server::PrometheusStatsFormatter::registerPrometheusNamespace(
        "istio");
  }
};

RegisterPrometheusNamespace register_namespace;

}  // namespace Istio
}  // namespace Wasm
}  // namespace Common
}  // namespace Extensions
}  // namespace Envoy
