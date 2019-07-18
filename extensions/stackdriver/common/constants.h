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
constexpr char kIstioProxyContainerName[] = "istio-proxy";

// Header keys
constexpr char kAuthorityHeaderKey[] = ":authority";
constexpr char kMethodHeaderKey[] = ":method";

}  // namespace Common
}  // namespace Stackdriver
}  // namespace Extensions