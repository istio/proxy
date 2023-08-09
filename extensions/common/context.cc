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

#include "extensions/common/context.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "extensions/common/node_info_bfbs_generated.h"
#include "extensions/common/util.h"
#include "flatbuffers/util.h"

// WASM_PROLOG
#ifndef NULL_PLUGIN
#include "proxy_wasm_intrinsics.h"

#else // NULL_PLUGIN

#include "include/proxy-wasm/null_plugin.h"

using proxy_wasm::WasmHeaderMapType;
using proxy_wasm::null_plugin::getHeaderMapValue;
using proxy_wasm::null_plugin::getProperty;
using proxy_wasm::null_plugin::getValue;

#endif // NULL_PLUGIN

// END WASM_PROLOG

namespace Wasm {
namespace Common {

const char kBlackHoleCluster[] = "BlackHoleCluster";
const char kPassThroughCluster[] = "PassthroughCluster";
const char kBlackHoleRouteName[] = "block_all";
const char kPassThroughRouteName[] = "allow_any";
const char kInboundPassthroughClusterIpv4[] = "InboundPassthroughClusterIpv4";
const char kInboundPassthroughClusterIpv6[] = "InboundPassthroughClusterIpv6";

// Well-known name for the grpc_stats filter.
constexpr std::string_view GrpcStatsName = "envoy.filters.http.grpc_stats";

namespace {

// Get destination service host and name based on destination cluster metadata
// and host header.
// * If cluster name is one of passthrough and blackhole clusters, use cluster
//   name as destination service name and host header as destination host.
// * Otherwise, try fetching cluster metadata for destination service name and
//   host. If cluster metadata is not available, set destination service name
//   the same as destination service host.
void populateDestinationService(bool outbound, bool use_host_header, RequestInfo* request_info) {
  if (use_host_header) {
    request_info->destination_service_host = request_info->url_host;
  } else {
    request_info->destination_service_host = outbound ? "unknown" : getServiceNameFallback();
  }

  // override the cluster name if this is being sent to the
  // blackhole or passthrough cluster
  const std::string& route_name = request_info->route_name;
  if (route_name == kBlackHoleRouteName) {
    request_info->destination_service_name = kBlackHoleCluster;
    return;
  } else if (route_name == kPassThroughRouteName) {
    request_info->destination_service_name = kPassThroughCluster;
    return;
  }

  const std::string& cluster_name = request_info->upstream_cluster;
  if (cluster_name == kBlackHoleCluster || cluster_name == kPassThroughCluster ||
      cluster_name == kInboundPassthroughClusterIpv4 ||
      cluster_name == kInboundPassthroughClusterIpv6) {
    request_info->destination_service_name = cluster_name;
    return;
  }

  // Get destination service name and host from cluster labels, which is
  // formatted as follow: cluster_metadata:
  //   filter_metadata:
  //     istio:
  //       services:
  //       - host: a.default
  //         name: a
  //         namespace: default
  //       - host: b.default
  //         name: b
  //         namespace: default
  // Multiple services could be added to a inbound cluster when they are bound
  // to the same port. Currently we use the first service in the list (the
  // oldest service) to get destination service information. Ideally client will
  // forward the canonical host to the server side so that it could accurately
  // identify the intended host.
  if (getValue({"cluster_metadata", "filter_metadata", "istio", "services", "0", "name"},
               &request_info->destination_service_name)) {
    getValue({"cluster_metadata", "filter_metadata", "istio", "services", "0", "host"},
             &request_info->destination_service_host);
  } else {
    // if cluster metadata cannot be found, fallback to destination service
    // host. If host header fallback is enabled, this will be host header. If
    // host header fallback is disabled, this will be unknown. This could happen
    // if a request does not route to any cluster.
    request_info->destination_service_name = request_info->destination_service_host;
  }
}

} // namespace

void populateRequestInfo(bool outbound, bool use_host_header_fallback, RequestInfo* request_info) {
  if (request_info->is_populated) {
    return;
  }

  request_info->is_populated = true;

  getValue({"cluster_name"}, &request_info->upstream_cluster);
  getValue({"route_name"}, &request_info->route_name);
  // Fill in request info.
  // Get destination service name and host based on cluster name and host
  // header.
  populateDestinationService(outbound, use_host_header_fallback, request_info);
  uint64_t destination_port = 0;
  if (outbound) {
    getValue({"upstream", "port"}, &destination_port);
    getValue({"upstream", "uri_san_peer_certificate"}, &request_info->destination_principal);
    getValue({"upstream", "uri_san_local_certificate"}, &request_info->source_principal);
  } else {
    getValue({"destination", "port"}, &destination_port);

    bool mtls = false;
    if (getValue({"connection", "mtls"}, &mtls)) {
      request_info->service_auth_policy =
          mtls ? ::Wasm::Common::ServiceAuthenticationPolicy::MutualTLS
               : ::Wasm::Common::ServiceAuthenticationPolicy::None;
    }
    getValue({"connection", "uri_san_local_certificate"}, &request_info->destination_principal);
    getValue({"connection", "uri_san_peer_certificate"}, &request_info->source_principal);
  }
  request_info->destination_port = destination_port;
}

std::string_view AuthenticationPolicyString(ServiceAuthenticationPolicy policy) {
  switch (policy) {
  case ServiceAuthenticationPolicy::None:
    return kNone;
  case ServiceAuthenticationPolicy::MutualTLS:
    return kMutualTLS;
  default:
    break;
  }
  return {};
}

std::string_view TCPConnectionStateString(TCPConnectionState state) {
  switch (state) {
  case TCPConnectionState::Open:
    return kOpen;
  case TCPConnectionState::Connected:
    return kConnected;
  case TCPConnectionState::Close:
    return kClose;
  default:
    break;
  }
  return {};
}

std::string_view ProtocolString(Protocol protocol) {
  switch (protocol) {
  case Protocol::TCP:
    return kProtocolTCP;
  case Protocol::HTTP:
    return kProtocolHTTP;
  case Protocol::GRPC:
    return kProtocolGRPC;
  default:
    break;
  }
  return {};
}

// Retrieves the traffic direction from the configuration context.
TrafficDirection getTrafficDirection() {
  int64_t direction;
  if (getValue({"listener_direction"}, &direction)) {
    return static_cast<TrafficDirection>(direction);
  }
  return TrafficDirection::Unspecified;
}

flatbuffers::DetachedBuffer extractEmptyNodeFlatBuffer() {
  flatbuffers::FlatBufferBuilder fbb;
  FlatNodeBuilder node(fbb);
  auto data = node.Finish();
  fbb.Finish(data);
  return fbb.Release();
}

flatbuffers::DetachedBuffer extractLocalNodeFlatBuffer() {
  flatbuffers::FlatBufferBuilder fbb;
  flatbuffers::Offset<flatbuffers::String> name, namespace_, owner, workload_name, istio_version,
      mesh_id, cluster_id;
  std::vector<flatbuffers::Offset<KeyVal>> labels, platform_metadata;
  std::vector<flatbuffers::Offset<flatbuffers::String>> app_containers;
  std::vector<flatbuffers::Offset<flatbuffers::String>> ip_addrs;
  std::string value;
  if (getValue({"node", "metadata", "NAME"}, &value)) {
    name = fbb.CreateString(value);
  }
  if (getValue({"node", "metadata", "NAMESPACE"}, &value)) {
    namespace_ = fbb.CreateString(value);
  }
  if (getValue({"node", "metadata", "OWNER"}, &value)) {
    owner = fbb.CreateString(value);
  }
  if (getValue({"node", "metadata", "WORKLOAD_NAME"}, &value)) {
    workload_name = fbb.CreateString(value);
  }
  if (getValue({"node", "metadata", "ISTIO_VERSION"}, &value)) {
    istio_version = fbb.CreateString(value);
  }
  if (getValue({"node", "metadata", "MESH_ID"}, &value)) {
    mesh_id = fbb.CreateString(value);
  }
  if (getValue({"node", "metadata", "CLUSTER_ID"}, &value)) {
    cluster_id = fbb.CreateString(value);
  }
  {
    auto buf = getProperty({"node", "metadata", "LABELS"});
    if (buf.has_value()) {
      for (const auto& [key, val] : buf.value()->pairs()) {
        labels.push_back(CreateKeyVal(fbb, fbb.CreateString(key), fbb.CreateString(val)));
      }
    }
  }
  {
    auto buf = getProperty({"node", "metadata", "PLATFORM_METADATA"});
    if (buf.has_value()) {
      for (const auto& [key, val] : buf.value()->pairs()) {
        platform_metadata.push_back(
            CreateKeyVal(fbb, fbb.CreateString(key), fbb.CreateString(val)));
      }
    }
  }
  if (getValue({"node", "metadata", "APP_CONTAINERS"}, &value)) {
    std::vector<absl::string_view> containers = absl::StrSplit(value, ',');
    for (const auto& container : containers) {
      app_containers.push_back(fbb.CreateString(toStdStringView(container)));
    }
  }
  if (getValue({"node", "metadata", "INSTANCE_IPS"}, &value)) {
    std::vector<absl::string_view> ips = absl::StrSplit(value, ',');
    for (const auto& ip : ips) {
      ip_addrs.push_back(fbb.CreateString(toStdStringView(ip)));
    }
  }

  auto labels_offset = fbb.CreateVectorOfSortedTables(&labels);
  auto platform_metadata_offset = fbb.CreateVectorOfSortedTables(&platform_metadata);
  auto app_containers_offset = fbb.CreateVector(app_containers);
  auto ip_addrs_offset = fbb.CreateVector(ip_addrs);
  FlatNodeBuilder node(fbb);
  node.add_name(name);
  node.add_namespace_(namespace_);
  node.add_owner(owner);
  node.add_workload_name(workload_name);
  node.add_istio_version(istio_version);
  node.add_mesh_id(mesh_id);
  node.add_cluster_id(cluster_id);
  node.add_labels(labels_offset);
  node.add_platform_metadata(platform_metadata_offset);
  node.add_app_containers(app_containers_offset);
  node.add_instance_ips(ip_addrs_offset);
  auto data = node.Finish();
  fbb.Finish(data);
  return fbb.Release();
}

namespace {

bool extractPeerMetadataFromUpstreamMetadata(const std::string& metadata_type,
                                             flatbuffers::FlatBufferBuilder& fbb) {
  std::string endpoint_labels;
  if (!getValue({metadata_type, "filter_metadata", "istio", "workload"}, &endpoint_labels)) {
    return false;
  }
  std::vector<absl::string_view> parts = absl::StrSplit(endpoint_labels, ';');
  // workload label should semicolon separated four parts string:
  // workload_name;namespace;canonical_service;canonical_revision;cluster_id.
  if (parts.size() < 5) {
    return false;
  }

  flatbuffers::Offset<flatbuffers::String> workload_name, namespace_, cluster_id;
  std::vector<flatbuffers::Offset<KeyVal>> labels;
  workload_name = fbb.CreateString(toStdStringView(parts[0]));
  namespace_ = fbb.CreateString(toStdStringView(parts[1]));
  if (!parts[2].empty()) {
    labels.push_back(CreateKeyVal(fbb, fbb.CreateString(kCanonicalServiceLabelName),
                                  fbb.CreateString(toStdStringView(parts[2]))));
  }
  if (!parts[3].empty()) {
    labels.push_back(CreateKeyVal(fbb, fbb.CreateString(kCanonicalServiceRevisionLabelName),
                                  fbb.CreateString(toStdStringView(parts[3]))));
  }
  if (parts.size() >= 5) {
    // In case newer proxy runs with old control plane, only extract cluster
    // name if there are the fifth part.
    cluster_id = fbb.CreateString(toStdStringView(parts[4]));
  }
  auto labels_offset = fbb.CreateVectorOfSortedTables(&labels);

  FlatNodeBuilder node(fbb);
  node.add_workload_name(workload_name);
  node.add_namespace_(namespace_);
  if (!cluster_id.IsNull()) {
    node.add_cluster_id(cluster_id);
  }
  node.add_labels(labels_offset);
  auto data = node.Finish();
  fbb.Finish(data);
  return true;
}

} // namespace

bool extractPeerMetadataFromUpstreamClusterMetadata(flatbuffers::FlatBufferBuilder& fbb) {
  return extractPeerMetadataFromUpstreamMetadata("cluster_metadata", fbb);
}

bool extractPeerMetadataFromUpstreamHostMetadata(flatbuffers::FlatBufferBuilder& fbb) {
  return extractPeerMetadataFromUpstreamMetadata("upstream_host_metadata", fbb);
}

PeerNodeInfo::PeerNodeInfo(const std::string_view peer_metadata_id_key,
                           const std::string_view peer_metadata_key) {
  // Attempt to read from filter_state first.
  found_ = getValue({peer_metadata_id_key}, &peer_id_);
  if (found_) {
    if (getValue({peer_metadata_key}, &peer_node_)) {
      return;
    }
  }

  // Sentinel value is preserved as ID to implement maybeWaiting.
  found_ = false;
  if (getValue({kMetadataNotFoundValue}, &peer_id_)) {
    peer_id_ = kMetadataNotFoundValue;
  }

  // Downstream peer metadata will never be in localhost endpoint. Skip
  // looking for it.
  if (peer_metadata_id_key == kDownstreamMetadataIdKey) {
    fallback_peer_node_ = extractEmptyNodeFlatBuffer();
    return;
  }

  // Construct a fallback peer node metadata based on endpoint labels if it is
  // not in filter state. This may happen before metadata is received as well.
  flatbuffers::FlatBufferBuilder fbb;
  if (extractPeerMetadataFromUpstreamHostMetadata(fbb)) {
    fallback_peer_node_ = fbb.Release();
  } else {
    fallback_peer_node_ = extractEmptyNodeFlatBuffer();
  }
}

const ::Wasm::Common::FlatNode& PeerNodeInfo::get() const {
  if (found_) {
    return *flatbuffers::GetRoot<::Wasm::Common::FlatNode>(
        reinterpret_cast<const uint8_t*>(peer_node_.data()));
  }
  return *flatbuffers::GetRoot<::Wasm::Common::FlatNode>(fallback_peer_node_.data());
}

// Host header is used if use_host_header_fallback==true.
void populateHTTPRequestInfo(bool outbound, bool use_host_header_fallback,
                             RequestInfo* request_info) {
  populateRequestProtocol(request_info);
  getValue({"request", "url_path"}, &request_info->url_path);
  populateRequestInfo(outbound, use_host_header_fallback, request_info);

  int64_t response_code = 0;
  if (getValue({"response", "code"}, &response_code)) {
    request_info->response_code = response_code;
  }

  uint64_t response_flags = 0;
  if (getValue({"response", "flags"}, &response_flags)) {
    request_info->response_flag = parseResponseFlag(response_flags);
  }

  if (request_info->request_protocol == Protocol::GRPC) {
    int64_t grpc_status_code = 2;
    getValue({"response", "grpc_status"}, &grpc_status_code);
    request_info->grpc_status = grpc_status_code;
    populateGRPCInfo(request_info);
  }

  std::string operation_id;
  request_info->request_operation =
      getValue({::Wasm::Common::kRequestOperationKey}, &operation_id)
          ? operation_id
          : getHeaderMapValue(WasmHeaderMapType::RequestHeaders, kMethodHeaderKey)->toString();

  getValue({"request", "time"}, &request_info->start_time);
  getValue({"request", "duration"}, &request_info->duration);
  getValue({"request", "total_size"}, &request_info->request_size);
  getValue({"response", "total_size"}, &request_info->response_size);
}

std::string_view nodeInfoSchema() {
  return std::string_view(reinterpret_cast<const char*>(FlatNodeBinarySchema::data()),
                          FlatNodeBinarySchema::size());
}

void populateExtendedHTTPRequestInfo(RequestInfo* request_info) {
  populateExtendedRequestInfo(request_info);

  if (getValue({"request", "referer"}, &request_info->referer)) {
    sanitizeBytes(&request_info->referer);
  }
  if (getValue({"request", "useragent"}, &request_info->user_agent)) {
    sanitizeBytes(&request_info->user_agent);
  }
  if (getValue({"request", "id"}, &request_info->request_id)) {
    sanitizeBytes(&request_info->request_id);
  }
  std::string trace_sampled;
  if (getValue({"request", "headers", "x-b3-sampled"}, &trace_sampled) && trace_sampled == "1") {
    if (getValue({"request", "headers", "x-b3-traceid"}, &request_info->b3_trace_id)) {
      sanitizeBytes(&request_info->b3_trace_id);
    }
    if (getValue({"request", "headers", "x-b3-spanid"}, &request_info->b3_span_id)) {
      sanitizeBytes(&request_info->b3_span_id);
    }
    request_info->b3_trace_sampled = true;
  }

  getValue({"request", "path"}, &request_info->path);
  getValue({"request", "host"}, &request_info->url_host);
  getValue({"request", "scheme"}, &request_info->url_scheme);
  std::string response_details;
  getValue({"response", "code_details"}, &response_details);
  if (!response_details.empty()) {
    request_info->response_details = response_details;
  }
}

void populateExtendedRequestInfo(RequestInfo* request_info) {
  getValue({"source", "address"}, &request_info->source_address);
  getValue({"destination", "address"}, &request_info->destination_address);
  getValue({"source", "port"}, &request_info->source_port);
  getValue({"connection_id"}, &request_info->connection_id);
  getValue({"upstream", "address"}, &request_info->upstream_host);
  getValue({"connection", "requested_server_name"}, &request_info->requested_server_name);
  auto envoy_original_path =
      getHeaderMapValue(WasmHeaderMapType::RequestHeaders, kEnvoyOriginalPathKey);
  request_info->x_envoy_original_path = envoy_original_path ? envoy_original_path->toString() : "";
  sanitizeBytes(&request_info->x_envoy_original_path);
  auto envoy_original_dst_host =
      getHeaderMapValue(WasmHeaderMapType::RequestHeaders, kEnvoyOriginalDstHostKey);
  request_info->x_envoy_original_dst_host =
      envoy_original_dst_host ? envoy_original_dst_host->toString() : "";
  sanitizeBytes(&request_info->x_envoy_original_dst_host);
  getValue({"upstream", "transport_failure_reason"},
           &request_info->upstream_transport_failure_reason);
  std::string response_details;
  getValue({"connection", "termination_details"}, &response_details);
  if (!response_details.empty()) {
    request_info->response_details = response_details;
  }
}

void populateTCPRequestInfo(bool outbound, RequestInfo* request_info) {
  // host_header_fallback is for HTTP/gRPC only.
  populateRequestInfo(outbound, false, request_info);

  uint64_t response_flags = 0;
  if (getValue({"response", "flags"}, &response_flags)) {
    request_info->response_flag = parseResponseFlag(response_flags);
  }

  request_info->request_protocol = Protocol::TCP;
}

void populateRequestProtocol(RequestInfo* request_info) {
  if (kGrpcContentTypes.count(
          getHeaderMapValue(WasmHeaderMapType::RequestHeaders, kContentTypeHeaderKey)
              ->toString()) != 0) {
    request_info->request_protocol = Protocol::GRPC;
  } else {
    // TODO Add http/1.1, http/1.0, http/2 in a separate attribute.
    // http|grpc classification is compatible with Mixerclient
    request_info->request_protocol = Protocol::HTTP;
  }
}

bool populateGRPCInfo(RequestInfo* request_info) {
  std::string value;
  if (!getValue({"filter_state", GrpcStatsName}, &value)) {
    return false;
  }
  // The expected byte serialization of grpc_stats filter is "x,y" where "x"
  // is the request message count and "y" is the response message count.
  std::vector<absl::string_view> parts = absl::StrSplit(value, ',');
  if (parts.size() == 2) {
    return absl::SimpleAtoi(parts[0], &request_info->request_message_count) &&
           absl::SimpleAtoi(parts[1], &request_info->response_message_count);
  }
  return false;
}

bool getAuditPolicy() {
  bool shouldAudit = false;
  if (!getValue<bool>({"metadata", "filter_metadata", "envoy.common", "access_log_hint"},
                      &shouldAudit)) {
    return false;
  }

  return shouldAudit;
}

bool sanitizeBytes(std::string* buf) {
  char* start = buf->data();
  const char* const end = start + buf->length();
  bool modified = false;
  while (start < end) {
    char* s = start;
    if (flatbuffers::FromUTF8(const_cast<const char**>(&s)) < 0) {
      *start = ' ';
      start += 1;
      modified = true;
    } else {
      start = s;
    }
  }
  return modified;
}

// Used for `destination_service` fallback. Unlike elsewhere when that fallback
// to workload name, this falls back to "unknown" when the canonical name label
// is not found. This preserves the existing behavior for `destination_service`
// labeling. Using a workload name as a service name could be potentially
// problematic.
std::string getServiceNameFallback() {
  auto buf = getProperty({"node", "metadata", "LABELS"});
  if (buf.has_value()) {
    for (const auto& [key, val] : buf.value()->pairs())
      if (key == ::Wasm::Common::kCanonicalServiceLabelName.data()) {
        return std::string(val);
      }
  }
  return "unknown";
}

} // namespace Common
} // namespace Wasm
