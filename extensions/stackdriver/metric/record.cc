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
#include "extensions/stackdriver/metric/registry.h"

namespace Extensions {
namespace Stackdriver {
namespace Metric {

constexpr double kNanosecondsPerMillisecond = 1000000.0;
constexpr char kMutualTLS[] = "MUTUAL_TLS";
constexpr char kNone[] = "NONE";

void record(
    const stackdriver::config::v1alpha1::PluginConfig::ReporterKind &kind,
    const stackdriver::common::NodeInfo &local_node_info,
    const Extensions::Stackdriver::Common::RequestInfo &request_info) {
  double latency_ms =
      double(request_info.end_timestamp - request_info.start_timestamp) /
      kNanosecondsPerMillisecond;
  if (kind == stackdriver::config::v1alpha1::PluginConfig::ReporterKind::
                  PluginConfig_ReporterKind_INBOUND) {
    opencensus::stats::Record(
        {{serverRequestCountMeasure(), 1},
         {serverRequestBytesMeasure(), request_info.request_size},
         {serverResponseBytesMeasure(), request_info.response_size},
         {serverResponseLatenciesMeasure(), latency_ms}},
        {{requestOperationKey(), request_info.request_operation},
         {requestProtocolKey(), request_info.request_protocol},
         {serviceAuthenticationPolicyKey(),
          request_info.mTLS ? kMutualTLS : kNone},
         {destinationServiceNameKey(), request_info.destination_service_host},
         {destinationServiceNamespaceKey(), local_node_info.namespace_()},
         {destinationPortKey(), std::to_string(request_info.destination_port)},
         {responseCodeKey(), std::to_string(request_info.response_code)},
         {sourcePrincipalKey(), request_info.source_principal},
         {sourceWorkloadNameKey(), request_info.peer_node_info.workload_name()},
         {sourceWorkloadNamespaceKey(),
          request_info.peer_node_info.namespace_()},
         {sourceOwnerKey(), request_info.peer_node_info.owner()},
         {destinationPrincipalKey(), request_info.destination_principal},
         {destinationWorkloadNameKey(), local_node_info.workload_name()},
         {destinationWorkloadNamespaceKey(), local_node_info.namespace_()},
         {destinationOwnerKey(), local_node_info.owner()}});
  }
  if (kind == stackdriver::config::v1alpha1::PluginConfig::ReporterKind::
                  PluginConfig_ReporterKind_OUTBOUND) {
    opencensus::stats::Record(
        {{clientRequestCountMeasure(), 1},
         {clientRequestBytesMeasure(), request_info.request_size},
         {clientResponseBytesMeasure(), request_info.response_size},
         {clientRoundtripLatenciesMeasure(), latency_ms}},
        {{requestOperationKey(), request_info.request_operation},
         {requestProtocolKey(), request_info.request_protocol},
         {serviceAuthenticationPolicyKey(),
          request_info.mTLS ? kMutualTLS : kNone},
         {destinationServiceNameKey(), request_info.destination_service_host},
         {destinationServiceNamespaceKey(),
          request_info.peer_node_info.namespace_()},
         {destinationPortKey(), std::to_string(request_info.destination_port)},
         {responseCodeKey(), std::to_string(request_info.response_code)},
         {sourcePrincipalKey(), request_info.source_principal},
         {sourceWorkloadNameKey(), local_node_info.workload_name()},
         {sourceWorkloadNamespaceKey(), local_node_info.namespace_()},
         {sourceOwnerKey(), local_node_info.owner()},
         {destinationPrincipalKey(), request_info.destination_principal},
         {destinationWorkloadNameKey(),
          request_info.peer_node_info.workload_name()},
         {destinationWorkloadNamespaceKey(),
          request_info.peer_node_info.namespace_()},
         {destinationOwnerKey(), request_info.peer_node_info.owner()}});
  }
}

}  // namespace Metric
}  // namespace Stackdriver
}  // namespace Extensions
