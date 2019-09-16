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
constexpr char kServerRequestCountMeasure[] = "server/request_count_measure";
constexpr char kServerRequestBytesMeasure[] = "server/request_bytes_measure";
constexpr char kServerResponseBytesMeasure[] = "server/response_bytes_measure";
constexpr char kServerResponseLatenciesMeasure[] =
    "server/response_latencies_measure";
constexpr char kClientRequestCountMeasure[] = "client/request_count_measure";
constexpr char kClientRequestBytesMeasure[] = "client/request_bytes_measure";
constexpr char kClientResponseBytesMeasure[] = "client/response_bytes_measure";
constexpr char kClientRoundtripLatenciesMeasure[] =
    "client/roundtrip_latencies_measure";

// View names of metrics.
constexpr char kServerRequestCountView[] = "server/request_count";
constexpr char kServerRequestBytesView[] = "server/request_bytes";
constexpr char kServerResponseBytesView[] = "server/response_bytes";
constexpr char kServerResponseLatenciesView[] = "server/response_latencies";
constexpr char kClientRequestCountView[] = "client/request_count";
constexpr char kClientRequestBytesView[] = "client/request_bytes";
constexpr char kClientResponseBytesView[] = "client/response_bytes";
constexpr char kClientRoundtripLatenciesView[] = "client/roundtrip_latencies";

// Prefix for Istio metrics.
constexpr char kIstioMetricPrefix[] = "istio.io/service/";

// Monitored resource
constexpr char kPodMonitoredResource[] = "k8s_pod";
constexpr char kContainerMonitoredResource[] = "k8s_container";
constexpr char kProjectIDLabel[] = "project_id";
constexpr char kLocationLabel[] = "location";
constexpr char kClusterNameLabel[] = "cluster_name";
constexpr char kNamespaceNameLabel[] = "namespace_name";
constexpr char kPodNameLabel[] = "pod_name";
constexpr char kContainerNameLabel[] = "container_name";

// GCP node metadata key
constexpr char kGCPClusterLocationKey[] = "gcp_cluster_location";
constexpr char kGCPClusterNameKey[] = "gcp_cluster_name";
constexpr char kGCPProjectKey[] = "gcp_project";

// Misc
constexpr char kIstioProxyContainerName[] = "istio-proxy";

}  // namespace Common
}  // namespace Stackdriver
}  // namespace Extensions
