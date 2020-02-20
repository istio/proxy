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
#include "google/protobuf/util/time_util.h"

using google::protobuf::util::TimeUtil;

namespace Extensions {
namespace Stackdriver {
namespace Metric {

constexpr char kCanonicalNameLabel[] = "service.istio.io/canonical-name";
constexpr char kCanonicalRevisionLabel[] =
    "service.istio.io/canonical-revision";
constexpr char kLatest[] = "latest";

void record(bool is_outbound, const ::wasm::common::NodeInfo &local_node_info,
            const ::wasm::common::NodeInfo &peer_node_info,
            const ::Wasm::Common::RequestInfo &request_info) {
  double latency_ms = request_info.duration / 1000.0;
  const auto &operation =
      request_info.request_protocol == ::Wasm::Common::kProtocolGRPC
          ? request_info.request_url_path
          : request_info.request_operation;

  const auto &local_labels = local_node_info.labels();
  const auto &peer_labels = peer_node_info.labels();

  const auto local_name_iter = local_labels.find(kCanonicalNameLabel);
  const string &local_canonical_name = local_name_iter == local_labels.end()
                                           ? local_node_info.workload_name()
                                           : local_name->second;

  const auto peer_name_iter = peer_labels.find(kCanonicalNameLabel);
  const string &peer_canonical_name = peer_name_iter == peer_labels.end()
                                          ? peer_node_info.workload_name()
                                          : peer_name->second;

  const auto local_rev_iter = local_labels.find(kCanonicalRevisionLabel);
  const string &local_canonical_rev =
      local_rev_iter == local_labels.end() ? kLatest : local_rev->second;

  const auto peer_rev_iter = peer_labels.find(kCanonicalRevisionLabel);
  const string &peer_canonical_rev =
      peer_rev_iter == peer_labels.end() ? kLatest : peer_rev->second;

  if (is_outbound) {
    opencensus::stats::Record(
        {{clientRequestCountMeasure(), 1},
         {clientRequestBytesMeasure(), request_info.request_size},
         {clientResponseBytesMeasure(), request_info.response_size},
         {clientRoundtripLatenciesMeasure(), latency_ms}},
        {{meshUIDKey(), local_node_info.mesh_id()},
         {requestOperationKey(), operation},
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
         {destinationOwnerKey(), peer_node_info.owner()},
         {destinationCanonicalServiceNameKey(), peer_canonical_name},
         {destinationCanonicalServiceNamespaceKey(),
          peer_node_info.namespace_()},
         {destinationCanonicalRevisionKey(), peer_canonical_rev},
         {sourceCanonicalServiceNameKey(), local_canonical_name},
         {sourceCanonicalServiceNamespaceKey(), local_node_info.namespace_()},
         {sourceCanonicalRevisionKey(), local_canonical_rev}});
    return;
  }

  opencensus::stats::Record(
      {{serverRequestCountMeasure(), 1},
       {serverRequestBytesMeasure(), request_info.request_size},
       {serverResponseBytesMeasure(), request_info.response_size},
       {serverResponseLatenciesMeasure(), latency_ms}},
      {{meshUIDKey(), local_node_info.mesh_id()},
       {requestOperationKey(), operation},
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
       {destinationOwnerKey(), local_node_info.owner()},
       {destinationCanonicalServiceNameKey(), local_canonical_name},
       {destinationCanonicalServiceNamespaceKey(),
        local_node_info.namespace_()},
       {destinationCanonicalRevisionKey(), local_canonical_rev},
       {sourceCanonicalServiceNameKey(), peer_canonical_name},
       {sourceCanonicalServiceNamespaceKey(), peer_node_info.namespace_()},
       {sourceCanonicalRevisionKey(), peer_canonical_rev}});
}

}  // namespace Metric
}  // namespace Stackdriver
}  // namespace Extensions
