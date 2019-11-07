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

#include "extensions/stackdriver/metric/record.h"

#include "extensions/stackdriver/common/constants.h"
#include "extensions/stackdriver/metric/registry.h"

namespace Extensions {
namespace Stackdriver {
namespace Metric {

void record(bool is_outbound, const ::wasm::common::NodeInfo &local_node_info,
            const ::wasm::common::NodeInfo &peer_node_info,
            const ::Wasm::Common::RequestInfo &request_info) {
  double latency_ms =
      double(request_info.end_timestamp - request_info.start_timestamp) /
      Stackdriver::Common::kNanosecondsPerMillisecond;
  if (is_outbound) {
    opencensus::stats::Record(
        {{clientRequestCountMeasure(), 1},
         {clientRequestBytesMeasure(), request_info.request_size},
         {clientResponseBytesMeasure(), request_info.response_size},
         {clientRoundtripLatenciesMeasure(), latency_ms}},
        {{meshUIDKey(), local_node_info.mesh_id()},
         {requestOperationKey(), request_info.request_operation},
         {requestProtocolKey(), request_info.request_protocol},
         {serviceAuthenticationPolicyKey(),
          ::Wasm::Common::AuthenticationPolicyString(
              request_info.service_auth_policy)},
         {destinationServiceNameKey(), request_info.destination_service_name},
         {destinationServiceNamespaceKey(), peer_node_info.namespace_()},
         {destinationPortKey(), std::to_string(request_info.destination_port)},
         {responseCodeKey(), std::to_string(request_info.response_code)},
         {sourcePrincipalKey(), request_info.source_principal},
         {sourceWorkloadNameKey(), local_node_info.workload_name()},
         {sourceWorkloadNamespaceKey(), local_node_info.namespace_()},
         {sourceOwnerKey(), local_node_info.owner()},
         {destinationPrincipalKey(), request_info.destination_principal},
         {destinationWorkloadNameKey(), peer_node_info.workload_name()},
         {destinationWorkloadNamespaceKey(), peer_node_info.namespace_()},
         {destinationOwnerKey(), peer_node_info.owner()}});
    return;
  }

  opencensus::stats::Record(
      {{serverRequestCountMeasure(), 1},
       {serverRequestBytesMeasure(), request_info.request_size},
       {serverResponseBytesMeasure(), request_info.response_size},
       {serverResponseLatenciesMeasure(), latency_ms}},
      {{meshUIDKey(), local_node_info.mesh_id()},
       {requestOperationKey(), request_info.request_operation},
       {requestProtocolKey(), request_info.request_protocol},
       {serviceAuthenticationPolicyKey(),
        ::Wasm::Common::AuthenticationPolicyString(
            request_info.service_auth_policy)},
       {destinationServiceNameKey(), request_info.destination_service_name},
       {destinationServiceNamespaceKey(), local_node_info.namespace_()},
       {destinationPortKey(), std::to_string(request_info.destination_port)},
       {responseCodeKey(), std::to_string(request_info.response_code)},
       {sourcePrincipalKey(), request_info.source_principal},
       {sourceWorkloadNameKey(), peer_node_info.workload_name()},
       {sourceWorkloadNamespaceKey(), peer_node_info.namespace_()},
       {sourceOwnerKey(), peer_node_info.owner()},
       {destinationPrincipalKey(), request_info.destination_principal},
       {destinationWorkloadNameKey(), local_node_info.workload_name()},
       {destinationWorkloadNamespaceKey(), local_node_info.namespace_()},
       {destinationOwnerKey(), local_node_info.owner()}});
}

}  // namespace Metric
}  // namespace Stackdriver
}  // namespace Extensions
