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
constexpr char kGCEInstanceMonitoredResource[] = "gce_instance";
constexpr char kProjectIDLabel[] = "project_id";
constexpr char kLocationLabel[] = "location";
constexpr char kClusterNameLabel[] = "cluster_name";
constexpr char kNamespaceNameLabel[] = "namespace_name";
constexpr char kPodNameLabel[] = "pod_name";
constexpr char kContainerNameLabel[] = "container_name";
constexpr char kGCEInstanceIDLabel[] = "instance_id";
constexpr char kZoneLabel[] = "zone";

// GCP node metadata key
constexpr char kGCPLocationKey[] = "gcp_location";
constexpr char kGCPClusterNameKey[] = "gcp_gke_cluster_name";
constexpr char kGCPProjectKey[] = "gcp_project";
constexpr char kGCPGCEInstanceIDKey[] = "gcp_gce_instance_id";

// Misc
constexpr char kIstioProxyContainerName[] = "istio-proxy";
constexpr double kNanosecondsPerMillisecond = 1000000.0;

// Stackdriver root context id.
constexpr char kOutboundRootContextId[] = "stackdriver_outbound";
constexpr char kInboundRootContextId[] = "stackdriver_inbound";

// Stackdriver service endpoint node metadata key.
constexpr char kMonitoringEndpointKey[] = "STACKDRIVER_MONITORING_ENDPOINT";
constexpr char kLoggingEndpointKey[] = "STACKDRIVER_LOGGING_ENDPOINT";
constexpr char kMeshTelemetryEndpointKey[] =
    "STACKDRIVER_MESH_TELEMETRY_ENDPOINT";

}  // namespace Common
}  // namespace Stackdriver
}  // namespace Extensions
