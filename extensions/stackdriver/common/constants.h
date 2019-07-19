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

namespace Extensions {
namespace Stackdriver {
namespace Common {

// Measure names of metrics.
constexpr char kServerRequestCountMeasure[] =
    "istio.io/service/server/request_count_measure";
constexpr char kServerRequestBytesMeasure[] =
    "istio.io/service/server/request_bytes_measure";
constexpr char kServerResponseBytesMeasure[] =
    "istio.io/service/server/response_bytes_measure";
constexpr char kServerResponseLatenciesMeasure[] =
    "istio.io/service/server/response_latencies_measure";
constexpr char kClientRequestCountMeasure[] =
    "istio.io/service/client/request_count_measure";
constexpr char kClientRequestBytesMeasure[] =
    "istio.io/service/client/request_bytes_measure";
constexpr char kClientResponseBytesMeasure[] =
    "istio.io/service/client/response_bytes_measure";
constexpr char kClientRoundtripLatenciesMeasure[] =
    "istio.io/service/client/roundtrip_latencies_measure";

// View names of metrics.
constexpr char kServerRequestCountView[] =
    "istio.io/service/server/request_count";
constexpr char kServerRequestBytesView[] =
    "istio.io/service/server/request_bytes";
constexpr char kServerResponseBytesView[] =
    "istio.io/service/server/response_bytes";
constexpr char kServerResponseLatenciesView[] =
    "istio.io/service/server/response_latencies";
constexpr char kClientRequestCountView[] =
    "istio.io/service/client/request_count";
constexpr char kClientRequestBytesView[] =
    "istio.io/service/client/request_bytes";
constexpr char kClientResponseBytesView[] =
    "istio.io/service/client/response_bytes";
constexpr char kClientRoundtripLatenciesView[] =
    "istio.io/service/client/roundtrip_latencies";

// Monitored resource
constexpr char kPodMonitoredResource[] = "k8s_pod";
constexpr char kContainerMonitoredResource[] = "k8s_container";
constexpr char kProjectIDLabel[] = "project_id";
constexpr char kLocationLabel[] = "location";
constexpr char kClusterNameLabel[] = "cluster_name";
constexpr char kNamespaceNameLabel[] = "namespace_name";
constexpr char kPodNameLabel[] = "pod_name";
constexpr char kContainerNameLabel[] = "container_name";

// Node metadata
constexpr char kIstioMetadataKey[] = "istio.io/metadata";
constexpr char kMetadataPodNameKey[] = "name";
constexpr char kMetadataNamespaceKey[] = "namespace";
constexpr char kMetadataOwnerKey[] = "owner";
constexpr char kMetadataWorkloadNameKey[] = "workload_name";
constexpr char kMetadataContainersKey[] = "ports_to_containers";
constexpr char kPlatformMetadataKey[] = "platform_metadata";
constexpr char kGCPClusterLocationKey[] = "gcp_cluster_location";
constexpr char kGCPClusterNameKey[] = "gcp_cluster_name";
constexpr char kGCPProjectKey[] = "gcp_project";
constexpr char kUpstreamMetadataKey[] = "envoy.wasm.metadata_exchange.upstream";
constexpr char kDownstreamMetadataKey[] =
    "envoy.wasm.metadata_exchange.downstream";

// Header keys
constexpr char kAuthorityHeaderKey[] = ":authority";
constexpr char kMethodHeaderKey[] = ":method";

// Misc
constexpr double kNanosecondsPerMillisecond = 1000000.0;
constexpr char kIstioProxyContainerName[] = "istio-proxy";
constexpr char kMutualTLS[] = "MUTUAL_TLS";
constexpr char kNone[] = "NONE";

}  // namespace Common
}  // namespace Stackdriver
}  // namespace Extensions
